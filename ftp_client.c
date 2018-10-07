#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_COMMAND_PORT 1027
#define CLIENT_PORT 1029
#define SERVER_DATA_PORT 20
#define BUFSIZE 512 
#define BSIZE 512

int create_socket(int port);

typedef struct mode_cmd
{
	char mode[4];
	char argc[BSIZE];
}mode_cmd;

void parse_mode(char *buf, mode_cmd *Mode)
{
	memcpy(Mode->mode, buf, 3);
	memcpy(Mode->argc, buf+4, strlen(buf));
}

void send_user(int sock)
{
	char buf[25];
	memset(buf, 0, sizeof(buf));

	sprintf(buf, "%s %s", "USER", "test");

	send(sock, buf, sizeof(buf), 0);
}

void send_pass(int sock)
{
	char buf[25];
	memset(buf, 0, sizeof(buf));

	sprintf(buf, "%s %s", "PASS", "test");
	
	send(sock, buf, sizeof(buf), 0);
}

void send_pasv(int sock)
{
	char buf[25];
	memset(buf, 0, sizeof(buf));

	sprintf(buf, "%s", "PASV\r\n");
	
	send(sock, buf, sizeof(buf), 0);
}

int get_data_port(int sock, mode_cmd *Mode)
{
	int server_port = 0;
	int p[6];

	sscanf(Mode->argc, "%*[^(]( %d, %d, %d, %d, %d, %d)", &p[0], &p[1], &p[2], &p[3], &p[4], &p[5]);

	server_port = (p[4])*256 + p[5];
	printf("server_port: %d\n", server_port);

	return server_port;
}

void recv_data(int sock)
{
	char *filename = "pv.c";
	int fd, pipefd[2];
	int res = 1;

	fd = open(filename, O_RDWR | O_CREAT, 0666);
	
	if(pipe(pipefd) == -1)
		perror("ftp_client:pipe");
	
	while((res = splice(sock, 0, pipefd[1], NULL, BUFSIZE, SPLICE_F_MORE | SPLICE_F_MOVE)) > 0)
	{
		splice(pipefd[0], NULL, fd, 0, BUFSIZE, SPLICE_F_MORE| SPLICE_F_MOVE);
	}
	if(res == -1)
	{
		perror("ftp_client:splice");
		exit(-1);
	}
}

void send_retr(int sock, int server_port)
{
	int server_sock;
	char buf[50];
	char *filename = "/home/rhm/git/pv/pv.c";

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s %s\r\n", "RETR", filename);

	send(sock, buf, sizeof(buf), 0);

	server_sock = create_socket(server_port);
	recv_data(server_sock);
	
}

int create_socket(int port)
{
	int sock;
	int sddr_len;
	
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	sddr_len = sizeof(server_addr);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		printf("socket create error.\n");
		exit(-1);
	}
	
	if(connect(sock, (struct sockaddr *)&server_addr, sddr_len) < 0)
	{
		printf("connect error %d\n", errno);
		exit(-1);
	}

	return sock;
}

void cmd_ftp(int sock)
{
	char buf[BUFSIZE];
	int len;
	int server_port;

	mode_cmd *Mode = (mode_cmd *)malloc(sizeof(mode_cmd));
	
	for(;;)
	{
		memset(buf, 0, BUFSIZE);
		
		len = recv(sock, buf, BUFSIZE, 0);
		if(len < 0)
		{
			printf("recv error %d\n", errno);
			return;
		}

		printf("%s\n", buf);
		parse_mode(buf, Mode);
		Mode->mode[3] = '\0';
		if(strcmp(Mode->mode, "220") == 0 || strcmp(Mode->mode, "530") == 0)
		{
			send_user(sock);
		}
		else if(strcmp(Mode->mode, "221") == 0)
		{
			printf("server is die.\n");
			break;
		}
		else if(strcmp(Mode->mode, "331") == 0)
		{
			send_pass(sock);
		}
		else if(strcmp(Mode->mode, "230") == 0)
		{
			send_pasv(sock);
		}
		else if(strcmp(Mode->mode, "227") == 0)
		{
			server_port = get_data_port(sock, Mode);
			send_retr(sock, server_port);
		}
	}
}

void client(int port)
{
	int sock;

	sock = create_socket(port);

	cmd_ftp(sock);

	return;
}

int main(int argc, const char *argv[])
{
	client(SERVER_COMMAND_PORT);	
	return 0;
}
