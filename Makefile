CC=gcc
mysqlhealth:    mysqlhealth.c
	$(CC) -Wall mysqlhealth.c   -o mysqlhealth   -L/usr/lib/mysql -lmysqlclient -lz -lm
static:
	$(CC) -Wall mysqlhealth.c  /usr/lib64/mysql/libmysqlclient.a  -o mysqlhealth  -L/usr/lib64 -lpthread -lm -lrt -ldl
clean:
	rm -rf mysqlhealth
apple:
	$(CC) -Wall mysqlhealth.c   -o mysqlhealth   -L/usr/lib/mysql -lmysqlclient -lz -lm