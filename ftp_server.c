#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <pwd.h>
#include <netinet/in.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>

#ifndef BSIZE	
#define BSIZE 1024
#endif

#define SERVERPORT 1027
#define SERVERROOTPATH "/home/git"
#define MAX_EVENTS 10

typedef struct port{
	int p1;
	int p2;
}Port;

typedef struct State{
	int mode;
	int logged_in;
	int username_ok;
	char *username;
	char *message;
	int connection;
	int sock_pasv;
	int tr_pid;
}State;

typedef struct Command{
	char command[5];
	char arg[BSIZE];
}Command;

typedef enum conn_mode{
	NORMAL, 
	SERVER,
	CLIENT
}conn_mode;

typedef enum cmdlist{
	ABOR, CWD, DELE, LIST, MDTM, MKD, NLST, PASS, PASV,
	PORT, PWD, QUIT, RETR, RMD, RNFR, RNTO, SITE, SIZE,
	STOR, TYPE, USER, NOOP
}cmdlist;

static const char *cmdlist_str[]=
{
	"ABOR", "CWD", "DELE", "LIST", "MKD", "MDTM", "NLST", "PASS", "PASV",
	"PORT", "PWD", "QUIT", "RETR", "RMD", "RNFR", "RNTO", "SITE", "SIZE",
	"STOR", "TYPE", "USER", "NOOP"
};

static const char *usernames[]=
{
	"ftp", "public", "test", "anonymous","foo"
};

static char *welcome_message = "A very warm welcome!";

int create_socket(int port);

void str_perm(int perm, char *str_perm)
{
	int curperm = 0;
	int flag = 0;
	int read, write, exec;

	char fbuff[3];

	read = write = exec = 0;
	int i;

	for(i = 6; i >= 0; i -= 3)
	{
		curperm = ((perm & ALLPERMS) >> i) & 0x7;

		memset(fbuff, 0, 3);

		read = (curperm >> 2) & 0x1;
		write = (curperm >> 1) & 0x1;
		exec = (curperm >> 0) & 0x1;

		sprintf(fbuff, "%c%c%c", read?'r':'-', write?'w':'-', exec?'x':'-');
		strcat(str_perm, fbuff);

	}
}

int accept_connection(int socket)
{
	int addrlen = 0;
	struct sockaddr_in client_address;
	addrlen = sizeof(client_address);
	return accept(socket, (struct sockaddr*)&client_address, &addrlen);
}

void write_state(State *state)
{
	write(state->connection, state->message, strlen(state->message));
}

int lookup(char *needle, const char **haystack, int count)
{
	int i;

	for(i = 0; i < count; i++)
	{
		if(strcmp(needle, haystack[i]) == 0)
			return i;
	}

	return -1;
}

int lookup_cmd(char *cmd)
{
	const int cmdlist_count = sizeof(cmdlist_str)/sizeof(char *);
	return lookup(cmd, cmdlist_str, cmdlist_count);
}

void ftp_user(Command *cmd, State *state)
{
	const int total_usernames = sizeof(usernames)/sizeof(char *);

	if(lookup(cmd->arg, usernames, total_usernames) >= 0)
	{
		state->username = malloc(32);
		memset(state->username, 0, 32);

		strcpy(state->username, cmd->arg);
		state->username_ok = 1;
		state->message = "331 User name ok,need password\n";
	}
	else
	{
		state->message = "530 Invalid username\n";
	}

	write_state(state);
}

void ftp_pass(Command *cmd, State *state)
{
	if(state->username_ok == 1)
	{
		state->logged_in = 1;
		state->message = "230 Login successful\n";
	}
	else
	{
		state->message = "500 Invalid username or password\n";
	}

	write_state(state);
}

void gen_port(Port *port)
{
	printf("gen_port\n");
	srand(time(NULL));
	port->p1 = 128 + (rand() % 64);
	port->p2 = rand() % 0xff;
	printf("%d %d\n", port->p1, port->p2);
}

void getip(int sock, int *ip)
{
	socklen_t addr_size = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	getsockname(sock, (struct sockaddr *)&addr, &addr_size);

	int host, i;

	host = (addr.sin_addr.s_addr);
	for(i = 0; i < 4; i++)
	{
		ip[i] = (host >> i*8)&0xff;
	}
}

void ftp_pasv(Command *cmd, State *state)
{
	if(state->logged_in == 1)
	{
		int ip[4];
		char buff[255];
		char *response = "227 Entering Passvie Mode (%d, %d, %d, %d, %d, %d)\n";
		Port *port = malloc(sizeof(Port));
		gen_port(port);
		getip(state->connection, ip);
	
		state->sock_pasv = create_socket((256*port->p1)+port->p2);

		printf("port: %d\n", 256*port->p1 + port->p2);
		sprintf(buff, response, ip[0], ip[1], ip[2], ip[3], port->p1, port->p2);
		state->message = buff;
		state->mode = SERVER;
		puts(state->message);
	}
	else
	{
		state->message = "530 Please login with user and PASS.\n";
		printf("%s\n", state->message);
	}

	write_state(state);
}

void ftp_list(Command *cmd, State *state)
{
	
}

void ftp_cwd(Command *cmd, State *state)
{
	
}

void ftp_pwd(Command *cmd, State *state)
{

}

void ftp_mkd(Command *cmd, State *state)
{
	
}

void ftp_rmd(Command *cmd, State *state)
{
	
}
	
void ftp_retr(Command *cmd, State *state)
{
	int connection;
	int fd;
	struct stat stat_buf;
	off_t offset = 0;
	int send_total;
	
	if(!state->logged_in)
	{
		state->message = "530 Please login with USER and PASS.\n";
		return;
	}

	if(state->mode != SERVER)
	{
		state->message = "550 please use PASV instead of PORT.\n";
		return;
	}

	if(!(access(cmd->arg, R_OK | W_OK) ==0 && (fd = open(cmd->arg, O_RDWR))))
	{
		state->message = "550 Faild to get file.\n";
		return;
	}

	fstat(fd, &stat_buf);
	state->message = "150 Opening BINARY mode data connection.\n";
	write_state(state);

	connection = accept_connection(state->sock_pasv);
	close(state->sock_pasv);
	send_total = sendfile(connection, fd, &offset, stat_buf.st_size);
	printf("send_total:%d\n", send_total);
	if(send_total)
	{
		if(send_total != stat_buf.st_size)
		{
			perror("ftp_retr : sendfile");
			state->message = "550 Faild to send file.\n";
			goto err;
		}

		state->message = "226 File send OK";
	}
	else
	{
		state->message = "550 Faild to read file.\n";
	}
err:

	close(fd);
	close(connection);
	write_state(state);
	
}
	
void ftp_dele(Command *cmd, State *state)
{

}

void ftp_stor(Command *cmd, State *state)
{
	int connection, fd;
	off_t offset = 0;
	int pipefd[2];
	int res = 1;
	const int buff_size = 8192;

	printf("%s\n", cmd->arg);
	FILE *fp = fopen(cmd->arg, "w");
	if(fp == NULL)
	{
		perror("ftp_stor:fopen");
	}
	else if(state->logged_in)
	{
		if(state->mode == SERVER)
		{
			state->message = "550 Please use PASV instead of PORT.\n";
		}
		else
		{
			fd = fileno(fp);
			connection = accept_connection(state->sock_pasv);
			close(state->sock_pasv);

			if(pipe(pipefd) == -1)
				perror("ftp_stor:pipe");
			
			state->message = "125 Data connection already open; transfer starting.\n";
			write_state(state);

			while((res = splice(connection, 0, pipefd[1], NULL, buff_size, SPLICE_F_MORE | SPLICE_F_MOVE)) > 0)
			{
				splice(pipefd[0], NULL, fd, 0, buff_size, SPLICE_F_MORE | SPLICE_F_MOVE);
			}

			if(res == -1)
			{
				perror("ftp_stor:splice");
				exit(EXIT_SUCCESS);
			}
			else
			{
				state->message = "226 File send Ok.\n";
			}
			close(connection);
			close(fd);
		}
	}
	else
	{
			state->message = "530 Please login with USER and PASS.\n";
	}
	close(connection);

	write_state(state);


}

void ftp_size(Command *cmd, State *state)
{

}

void ftp_abor(Command *cmd, State *state)
{

}

void ftp_quit(Command *cmd, State *state)
{
	state->message = "221 Goodbye, friend Inever thought i'd die like this.\n";
	write_state(state);
	close(state->connection);
	exit(0);
}

void ftp_type(Command *cmd, State *state)
{

}

void response(Command *cmd, State *state)
{
	switch(lookup_cmd(cmd->command))
	{
		case USER:ftp_user(cmd, state); break;
		case PASS:ftp_pass(cmd, state); break;
		case PASV:ftp_pasv(cmd, state); break;
		case LIST:ftp_list(cmd, state); break;
		case CWD:ftp_cwd(cmd, state); break;
		case PWD:ftp_pwd(cmd, state); break;
		case MKD:ftp_mkd(cmd, state); break;
		case RMD:ftp_rmd(cmd, state); break;
		case RETR:ftp_retr(cmd, state); break;
		case DELE:ftp_dele(cmd, state); break;
		case STOR:ftp_stor(cmd, state); break;
		case SIZE:ftp_size(cmd, state); break;
		case ABOR:ftp_abor(cmd, state); break;
		case QUIT:ftp_quit(cmd, state); break;
		case TYPE:ftp_type(cmd, state); break;
		case NOOP:
			  if(state->logged_in){
			  	state->message = "200 Nice to NOOP you\n";
			  }
			  else{
			  	state->message = "500 NOOB hehe.\n";
			  }
			  write_state(state);
			  break;
		default:
			  state->message = "500 unknown command\n";
			  write_state(state);
			  break;
	}
}

void parse_command(char *cmdstring, Command *cmd)
{
	sscanf(cmdstring, "%s %s", cmd->command, cmd->arg);
}

void *communication(void *_c)
{
	chdir("/");

	int connection = *(int *)_c, bytes_read;

	char buffer[BSIZE];
	char welcome[BSIZE] = "220";

	Command *cmd = (Command *)malloc(sizeof(Command));
	State *state = (State *)malloc(sizeof(State));

	state->username = NULL;

	memset(buffer, 0, BSIZE);

	strcat(welcome, " ");

	if(strlen(welcome_message) < BSIZE-4)
	{
		strcat(welcome, welcome_message);
	}
	else
	{
		strcat(welcome, "welcome to nice FTP service");
	}
	
	strcat(welcome, "\n");

	write(connection, welcome, strlen(welcome));

	while(bytes_read = read(connection, buffer, BSIZE) > 0)
	{
		if(!(bytes_read > BSIZE) && bytes_read > 0)
		{
			printf("User %s sent command: %s\n", (state->username == 0)?"unknown":state->username, buffer);
			parse_command(buffer, cmd);
			state->connection = connection;

			if(buffer[0] <= 127 || buffer[0] >= 0)
			{
				response(cmd, state);
			}

			memset(buffer, 0, BSIZE);
			memset(cmd, 0, sizeof(cmd));
		}
		else
		{
			perror("server:read");
		}
	}

	close(connection);
	printf("client disconnected.\n");
	return NULL;
}

int create_socket(int port)
{
	int sock;
	int reuse = 1;
	
	struct sockaddr_in server_address = (struct sockaddr_in){
		AF_INET,
			htons(port),
			(struct in_addr){INADDR_ANY}
	};

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot open socket");
		exit(EXIT_FAILURE);
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	if(bind(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)
	{
		fprintf(stderr, "Cannot bind socket to address!\n");
		exit(EXIT_FAILURE);
	}

	listen(sock, 5);

	return sock;

}

void server(int port)
{
	/*
	if(chroot(SERVERROOTPATH) != 0)
	{
		printf("%s", "chroot errno:please run as root!\n");
		exit(0);
	}
	*/

	int sock = create_socket(port);
	
	struct sockaddr_in client_address;
	int len = sizeof(client_address);
	
	while(1)
	{
		int connection;
		connection = accept(sock, (struct sockaddr *)&client_address, &len);
		
		pthread_t pid;

		pthread_create(&pid, NULL, communication, (void *)(&connection));
	}

}

int main(int argc, char *argv[])
{
	server(SERVERPORT);
	return 0;
}
