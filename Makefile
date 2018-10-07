CC=gcc

OBJ=ftp_client.o

LIB= -lpthread
INCLUDE=
CFLAGS= -g -Wall

target=ftp_client

all:$(OBJ)
	$(CC) $^ -o $(target) $(LIB)

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	rm -rf *.o $(target)
