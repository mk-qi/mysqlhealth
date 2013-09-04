CC=gcc
check_mysqhealth_daemon:    check_mysqhealth_daemon.c
	$(CC) -Wall check_mysqhealth_daemon.c   -o check_mysqhealth_daemon   -L/usr/lib/mysql -lmysqlclient -lz -lm
static:
	$(CC) -Wall check_mysqhealth_daemon.c  /usr/lib64/mysql/libmysqlclient.a  -o check_mysqhealth_daemon  -L/usr/lib64 -lpthread -lm -lrt -ldl
clean:
	rm -rf check_mysqhealth_daemon
apple:
	$(CC) -Wall check_mysqhealth_daemon.c   -o check_mysqhealth_daemon   -L/usr/lib/mysql -lmysqlclient -lz -lm