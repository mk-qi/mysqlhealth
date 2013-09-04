/*
 * check_mysqhealth_daemon.c  v0.0.1.1
 * root@mkrss.com
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h> /* For  Linux */
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>  /*inet_ntoa*/


/* debug */
#define debug 1

/* DATABASE  connection info */
#define DBSERVER   "localhost"
#define DBUSER     "root"
#define DBPASS     ""
#define DBNAME     "information_schema"
#define QUERY      "show processlist"



/* DAEMON */
#define PgName   "mysqlhealth"
#define WorkDir   "/tmp"
#define PidFile   "/tmp/mysqlhealth.pid"

/* LOG Level */
#define INFO     1
#define WARN     2
#define ERROR    3
#define DEBUG    4

#define LogFile   "/tmp/mysqlhealth.log"

// FUNCTION DECLARATION
int  mysqlhealth(char *host, char *user, char *pass, char *dbase, char *query);
/* Return the UNIX time in microseconds */
long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust;
}

// HTTP response code 
static char* ok_response =
  "HTTP/1.0 200 OK\r\n"
  "Content-type: text/html\n"
  "\r\n"
  "<html>\r\n"
  " <body>\r\n"
  "  <p>This server running ok.</p>\r\n"
  " </body>\r\n"
 "</html>\r\n";

static char* bad_response  = 
  "HTTP/1.0 500  Internal Server Error\r\n"
  "Content-type: text/html\r\n"
  "\r\n"
  "<html>\r\n"
  " <body>\r\n"
  "  <h1> Internal Server Error</h1>\r\n"
  "  <p> There is something wrong with healt check.</p>\r\n"
  " </body>\r\n"
  "</html>\r\n";


static void handle_connect (int client_socket);

void log_msg(int level, const char *fmt, ...) {
    va_list args;
    char msg[1024];
	
	va_start(args,fmt);
    if (vsnprintf(msg, sizeof(msg), fmt, args) == -1) {
        msg[sizeof(msg) - 1] = '\0';
    	
    }
    va_end(args);
    
    FILE *fp;
    char buf[256];
    
    fp =  fopen(LogFile,"a");

    char* log_type;
    
    switch(level) {
        case ERROR:
          log_type = "ERROR";
          break;
        case INFO:
          log_type = "INFO";
          break;  
          case DEBUG:
            log_type = "DEBUG";
            break; 
        default:
          log_type = "(***)";
          break;
    }
    /*log time  & formart */
    struct timeval tv;
    gettimeofday(&tv,NULL);
    
    int off = strftime(buf,sizeof(buf),"%d %b %H:%M:%S.",localtime(&tv.tv_sec));
    snprintf(buf+off,sizeof(buf)-off,"%03d",(int)tv.tv_usec/1000);
    fprintf(fp,"[%d] %s %s %s\n",(int)getpid(),buf,log_type,msg);
    fflush(fp);
    fclose(fp);
}

// signal_handler 
void signal_handler(int sig)
{
    switch(sig)
    {
        case SIGHUP:
            log_msg(1, "Received SIGHUP signal.");
            break;
        case SIGINT:
        case SIGTERM:
            log_msg(1, "Received SIGTERM signal. but do nothing");
            break;
        default:
            log_msg(1, "Unhandled signal");
            break;
    }
}
// DAEMON
void daemonize() {
	int pidhandle ,i;
	
	char str[10];
	char *msg;
    msg=(char*)malloc(50*sizeof(char));
	if(getppid() == 1) return; //
	
    switch (fork()) {
           case -1:
               exit(EXIT_FAILURE);
           case 0:
               break;
           default:
               exit(EXIT_SUCCESS);
    }
    if (setsid() == -1) {
            exit(EXIT_FAILURE);;
    }
	
    if(chdir(WorkDir) < 0) { 
        log_msg(2,"Change running directory %s, failure", WorkDir);
        exit(EXIT_FAILURE);
	}
    
	//umask(027); 

	pidhandle = open(PidFile,O_RDWR|O_CREAT,0640);
	if (pidhandle < 0) { 
		sprintf(msg, "Error while creating lock file: %s!", PidFile);
		log_msg(2, msg);
        exit(EXIT_FAILURE);
	} 

	if (lockf(pidhandle,F_TLOCK,0) < 0) { 
		sprintf(msg, "Error while locking the file: %s", PidFile);
		log_msg(2, msg);
        exit(EXIT_FAILURE);
	}

	sprintf(str,"%d\n",getpid());
	
	if(write(pidhandle,str,strlen(str)) < 0) { // write pid
		sprintf(msg, "Error while writing pid to pid file: %s", PidFile);
		log_msg(2, msg);
		exit(-1);
	}
    
    /* close all descriptors */
    for (i = getdtablesize(); i >= 0; --i){
        close(i);
    }
    /* Open STDIN */
    i = open("/dev/null", O_RDWR);
    /* STDOUT */
    dup(i);
    /* STDERR */
    dup(i);
  
    signal(SIGCHLD,SIG_IGN); 
    signal(SIGTSTP,SIG_IGN); 
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
  
    signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
    /* Set up a signal handler */
    free(msg);
    log_msg(1, "process start running");

}


void socket_server(int port) {
    char msg[100];
    int svr_sock, cli_sock;
    
	socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    svr_sock = socket(AF_INET, SOCK_STREAM, 0);
 
    if(svr_sock < 0) {
        sprintf(msg, "error creating socket: %i\n", errno);
    	log_msg(2, msg);
        exit(1);
    }
     
     int yes=1;
     //set server reuse server address
     if (setsockopt(svr_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
         exit(1);
     }
	 
	 serv_addr.sin_family = AF_INET;
	 serv_addr.sin_addr.s_addr = INADDR_ANY;
	 serv_addr.sin_port = htons(port);
	 
	 if (bind (svr_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		 sprintf(msg, "error binding socket: %i\n", errno);
		 log_msg(2, msg);
		 exit(1);
	 }
	 
	 if (listen(svr_sock, 2) != 0) {
		 sprintf(msg, "error binding socket: %i\n", errno);
		 log_msg(2, msg);
		 exit(1);
	 }
	 while (1)
	 {
		 clilen = sizeof(cli_addr);
		 cli_sock = accept(svr_sock, (struct sockaddr *) &cli_addr, &clilen);
		 if(cli_sock < 0) {
			 if (errno == EINTR)
				 /* The call was interrupted by a signal.  Try again.  */
				 continue;
			 else
				 //printf(msg, "accept error: %i\n", errno);  
				 sprintf(msg, "accept error: %i\n", errno);
			 log_msg(2, msg);
		 }
		 getpeername (cli_sock, (struct sockaddr *) &cli_addr, &clilen);
		 printf ("connection accepted from %s\n",inet_ntoa (cli_addr.sin_addr));
		 sprintf(msg, "connection accepted from %s", inet_ntoa (cli_addr.sin_addr));
		 log_msg(1, msg);
		 handle_connect(cli_sock);
	 }/* while end */
	 close(svr_sock);
 }
/*   */
static void handle_connect (int client_socket)
{
	int buffer_len=1024;
	char* request = (char*) malloc(buffer_len);
	memset(request, 0, buffer_len*sizeof(char));
	read (client_socket, request, buffer_len-1);
	char* method = (char*) malloc(8);
	char* URL = (char*) malloc(128);
	sscanf(request, "%s %s ", method, URL);
	char* response = (char*) malloc(buffer_len);
	memset(response, 0, buffer_len*sizeof(char));
	int ret = mysqlhealth(DBSERVER, DBUSER, DBPASS, DBNAME,QUERY);
	//response      
	if (ret == 1 ){
		sprintf(response,ok_response,method);
	} else if ( ret == 2 ) {
		sprintf(response,bad_response,method);
	}else {
		sprintf(response,bad_response,method);
	}
	send(client_socket,response,strlen(response),0);
	free(response);
	free(request);
	free(method);
	free(URL);
	close(client_socket);
}
/*  */

int mysqlhealth(char *host, char *user, char *pass, char *dbase, char *query) {
    long long start = ustime();
    MYSQL mysql; char *msg;
	msg=(char*)malloc(100*sizeof(char));
	if(mysql_init(&mysql) == NULL)
	{
		log_msg(2, "MySql Initialization Error!");
    } else {
		if(mysql_real_connect(&mysql, host, user, pass, dbase, 0, NULL, 0) == NULL) {
			//printf(msg, "MySql Server Error: %s", mysql_error(&mysql));
			sprintf(msg, "MySql Server Error: %s", mysql_error(&mysql));
			log_msg(2, msg);
			return(2);
		} else {
			if (debug){
				printf(msg, "MySql Server Version: %s", mysql_get_server_info(&mysql));
			    sprintf(msg, "MySql Server Version: %s", mysql_get_server_info(&mysql));
                log_msg(4, msg);
            }
            MYSQL_RES *result;
        	MYSQL_ROW row;
        	int i, num_fields;
        	mysql_query(&mysql, query);
            result = mysql_store_result(&mysql);
            num_fields = mysql_num_fields(result);
            while ((row = mysql_fetch_row(result)))
        	{
        		for(i = 0; i < num_fields; i++){
                    sprintf(msg,"%s ", row[i] ? row[i] : "NULL");
                    if (debug){
                        log_msg(4, msg);
                    }    
        		}
        		//printf("\n");
        	}
            sprintf(msg,"show processlist with : %.3f seconds",(float)(ustime()-start)/1000000);
            log_msg(1, msg);
            mysql_free_result(result);
		}
	}
    mysql_close(&mysql);
	free(msg);
    return(1);
}
int main(){
    daemonize();
    socket_server(5000);
    return(0);
}