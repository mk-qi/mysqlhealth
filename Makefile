CC=gcc
check_mysqhealth_daemon:    check_mysqhealth_daemon.c
        $(CC) -Wall check_mysqhealth_daemon.c  /usr/lib64/mysql/libmysqlclient.a  -o check_mysqhealth_daemon  -L/usr/lib64 -lpthread -lm -lrt -ldl
clean:
        rm -rf daemon_mysql_health
