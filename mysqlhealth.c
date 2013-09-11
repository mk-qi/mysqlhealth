/*
 * mysqlhealth.c  v0.0.1.1
 * root@mkrss.com
 * build:
 *   osx:  make
 *   linux: make  or make static
 *   make static will  link libmysqlclient.so as static
 *   so that you need not install mysql-client package again
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>       /* For  Linux */
#include <errno.h>
#include <arpa/inet.h>  /*inet_ntoa*/

/* debug */
#define debug 1

/* DATABASE  connection info */
#define DBSERVER	"localhost"
#define DBUSER		"root"
#define DBPASS		""
#define DBNAME		"information_schema"
#define QUERY		"show processlist"

/* DAEMON */
#define PgName		"mysqlhealth"
#define WorkDir 	"/tmp"

/* LOG Level */
#define INFO		1
#define WARN		2
#define ERROR		3
#define DEBUG		4
#define LogFile 	"/tmp/mysqlhealth.log"

/* Socket read time out */
#define timeouts  5

long long ustime( void )
{
	struct timeval	tv;
	long long	ust;
	gettimeofday( &tv, NULL );
	ust	= ( (long long) tv.tv_sec) * 1000000;
	ust	+= tv.tv_usec;
	return(ust);
}

/* FUNCTION DECLARATION */
int mysqlhealth( char *host, char *user, char *pass, char *dbase, char *query );
static void handle_connect( int client_socket );
/* Return the UNIX time in microseconds */

/* HTTP response code */
static char* ok_response =
	"HTTP/1.0 200 OK\r\n"
	"Content-type: text/html\n"
	"\r\n"
	"<html>\r\n"
	" <body>\r\n"
	"  <p>This server running ok.</p>\r\n"
	" </body>\r\n"
	"</html>\r\n";

static char* bad_response =
	"HTTP/1.0 500  Internal Server Error\r\n"
	"Content-type: text/html\r\n"
	"\r\n"
	"<html>\r\n"
	" <body>\r\n"
	"  <h1> Internal Server Error</h1>\r\n"
	"  <p> There is something wrong with healt check.</p>\r\n"
	" </body>\r\n"
	"</html>\r\n";

void log_msg( int level, const char *fmt, ... )
{
	va_list args;
	char	msg[1024];
	va_start( args, fmt );
	if ( vsnprintf( msg, sizeof(msg), fmt, args ) == -1 )
	{
		msg[sizeof(msg) - 1] = '\0';
	}
	va_end( args );
	FILE	*fp;
	char	buf[256];
	fp = fopen( LogFile, "a" );
	char* log_type;
	switch ( level )
	{
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
	gettimeofday( &tv, NULL );
	int off = strftime( buf, sizeof(buf), "%d %b %H:%M:%S.", localtime( &tv.tv_sec ) );
	snprintf( buf + off, sizeof(buf) - off, "%03d", (int) tv.tv_usec / 1000 );
	fprintf( fp, "[%d] %s %s %s\n", (int) getpid(), buf, log_type, msg );
	fflush( fp );
	fclose( fp );
}


/* DAEMON */
void daemonize()
{
	int i;
	if ( getppid() == 1 )
		return;     /*  */
	switch ( fork() )
	{
	case -1:
		exit( EXIT_FAILURE );
	case 0:
		break;
	default:
		exit( EXIT_SUCCESS );
	}
	if ( setsid() == -1 )
	{
		exit( EXIT_FAILURE );;
	}
	if ( chdir( WorkDir ) < 0 )
	{
		log_msg( 2, "Change running directory %s, failure", WorkDir );
		exit( EXIT_FAILURE );
	}
	/* umask(027); */
	/* close all descriptors */
	for ( i = getdtablesize(); i >= 0; --i )
	{
		close( i );
	}
	i = open( "/dev/null", O_RDWR );
	dup( i );
	dup( i );
}


void socket_server( int port )
{
	int			svr_sock, cli_sock;
	socklen_t		clilen;
	struct sockaddr_in	serv_addr, cli_addr;
    
    	/* socket recive timeouts  */
    	struct timeval timeout;      
           timeout.tv_sec = 10;
           timeout.tv_usec = 0;
    	//
	svr_sock = socket( AF_INET, SOCK_STREAM, 0 );
	if ( svr_sock < 0 )
	{
		log_msg( 2, "error creating socket: %i", errno );
		exit( 1 );
	}
	int yes = 1;
	/* set server reuse server address */
	if ( setsockopt( svr_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes) ) == -1 )
	{
		exit( 1 );
	}
    	// socket recive timeout = 5s/
    	if (setsockopt ( svr_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0){
        	exit( 1 );
	}
                    
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons( port );

	if ( bind( svr_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr) ) < 0 )
	{
		/* log_msg(2, "error binding socket: %i\n", errno); */
		exit( 1 );
	}
	if ( listen( svr_sock, 2 ) != 0 )
	{
		log_msg( 2, "error listen socket: %i", errno );
		exit( 1 );
	}
	log_msg( 1, "%s listening on port 5000", PgName );
	while ( 1 )
	{
		clilen		= sizeof(cli_addr);
		cli_sock	= accept( svr_sock, (struct sockaddr *) &cli_addr, &clilen );
		if ( cli_sock < 0 )
		{
			if ( errno == EINTR )
				/* The call was interrupted by a signal.  Try again.  */
				continue;
			else
				log_msg( 2, "accept error: %i", errno );
		}
		getpeername( cli_sock, (struct sockaddr *) &cli_addr, &clilen );
		handle_connect( cli_sock );
	} /* while end */
	close( svr_sock );
}


/*   */
static void handle_connect( int client_socket )
{   
    long long pstart = ustime();
	int	buffer_len	= 1024;
	char	* request	= (char *) malloc( buffer_len );
	memset( request, 0, buffer_len * sizeof(char) );
	read( client_socket, request, buffer_len - 1 );
	char* method = (char *) malloc( 8 );
	sscanf( request, "%s", method );
	char* response = (char *) malloc( buffer_len );
	memset( response, 0, buffer_len * sizeof(char) );
	int ret = mysqlhealth( DBSERVER, DBUSER, DBPASS, DBNAME, QUERY );
	/* response */
	if ( ret == 1 )
	{
		sprintf( response, ok_response, method );
	} else if ( ret == 2 )
	{
		sprintf( response, bad_response, method );
	}else {
		sprintf( response, bad_response, method );
	}
	send( client_socket, response, strlen( response ), 0 );
	free( response );
	free( request );
	free( method );
	close( client_socket );
	log_msg( 1, "show %.3f",(float) (ustime() - pstart) );
    
}


/*  */
int mysqlhealth( char *host, char *user, char *pass, char *dbase, char *query )
{
	long long	start = ustime();
	MYSQL		mysql; char *msg;
	msg = (char *) malloc( 100 * sizeof(char) );
	if ( mysql_init( &mysql ) == NULL )
	{
		log_msg( 2, "MySql Initialization Error!" );
	} else {
		mysql_options( &mysql, MYSQL_OPT_CONNECT_TIMEOUT, "5" );
		mysql_options( &mysql, MYSQL_READ_DEFAULT_GROUP, PgName );
		if ( mysql_real_connect( &mysql, host, user, pass, dbase, 3306, NULL, 0 ) == NULL )
		{
			log_msg( 2, "MySql Server Error: %s", mysql_error( &mysql ) );
			return(2);
		} else {
			if ( debug )
			{
				log_msg( 2, "MySql Server Version: %s", mysql_get_server_info( &mysql ) );
			}
			MYSQL_RES	*result;
			MYSQL_ROW	row;
			int		num_fields;
			mysql_query( &mysql, query );
			result		= mysql_store_result( &mysql );
			num_fields	= mysql_num_fields( result );
			while ( (row = mysql_fetch_row( result ) ) )
			{  /*
				 * for(i = 0; i < num_fields; i++){
				 *        sprintf(msg,"%s ", row[i] ? row[i] : "NULL");
				 *        if (debug){
				 *            log_msg(4, msg);
				 *        }
				 * }*/
				/* printf("\n"); */
			}
			log_msg( 1, "show processlist with : %.3f seconds", (float) (ustime() - start) / 1000000 );
			mysql_free_result( result );
		}
	}
	mysql_close( &mysql );
	free( msg );
	return(1);
}

int main()
{   
	daemonize();
	socket_server( 5000 );
	return(0);
}
