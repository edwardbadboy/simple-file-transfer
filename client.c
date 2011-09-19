#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <signal.h>

#define INVALID_SOCKET	-1
#include "common.h"
#include "addr_helper.h"

static int connect_to(char* server,char* ports);
static int get_file(int sockfd);
static int reqfile(int fd,char* filename);
static int rcvbigfile(int fd,char* filename);
static int init_signals(void);
static int restore_signals(void);
static void catch_sig_int(int signo);

static volatile sig_atomic_t g_exit=0;
static struct sigaction intact,oldintact;

int main(int argc, char *argv[])
{
	int sockfd,r=0;

	if (argc != 3){
		fprintf(stderr,"usage: %s server_hostname_or_ip port\n",argv[0]);
		return 1;
	}

	if(init_signals()==-1){
		return 1;
	}

	sockfd=connect_to(argv[1],argv[2]);
	if(sockfd==INVALID_SOCKET){
		return 1;
	}

	if(get_file(sockfd)==-1){
		r=1;
	}

	if(close(sockfd)==-1){
		perror("close");
		r=1;
	}

	if(restore_signals()==-1){
		r=1;
	}

	return r;
}

static int init_signals(void)
{
	int r=0;
	intact.sa_handler=catch_sig_int;
	sigemptyset(&intact.sa_mask);
	intact.sa_flags=0;
	r=sigaction(SIGINT,&intact,&oldintact);
	if(r==-1){
		perror("sigaction SIGINT");
		return -1;
	}
	return 0;
}

static int restore_signals(void)
{
	int r=0;
	r=sigaction(SIGINT,&oldintact,NULL);
	if(r==-1){
		perror("restore SIGINT sigaction");
		return -1;
	}
	return 0;
}

static int get_file(int sockfd)
{
	char getf[PATH_MAX]={0},savef[PATH_MAX]={0},*n_pos=NULL;

	printf("Type a file name to get:\n");
	if(fgets(getf,sizeof(getf),stdin)==NULL){
		return -1;
	}
	getf[sizeof(getf)-1]=0;
	n_pos=strstr(getf,"\n");
	if(n_pos!=NULL){
		*n_pos=0;
	}

	//向服务器请求一个文件
	if(reqfile(sockfd,getf)==-1){
		return -1;
	}

	printf("Type a file name to save:\n");
	if(fgets(savef,sizeof(savef),stdin)==NULL){
		return -1;
	}
	savef[sizeof(savef)-1]=0;
	n_pos=strstr(savef,"\n");
	if(n_pos!=NULL){
		*n_pos=0;
	}
	
	//接收这个文件
	if(rcvbigfile(sockfd,savef)==-1){
		fprintf(stderr,"error: receive file failed\n");
		return -1;
	}
	fprintf(stderr,"info: receive file OK.\n\n");

	return 0;
}

static int connect_to(char* server,char* ports)
{
	int sockfd=0;  
	struct addrinfo hints, *servinfo=NULL, *p=NULL;
	int rv=0;
	char s[INET6_ADDRSTRLEN]={0};
	int connected=0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(server, ports, &hints, &servinfo)) != 0) {
		fprintf(stderr, "error: getaddrinfo failed: %s\n", gai_strerror(rv));
		freeaddrinfo(servinfo); 
		return INVALID_SOCKET;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == INVALID_SOCKET) {
			perror("socket");
			continue;
		}

		rv=addr_to_str((struct sockaddr_storage*)(p->ai_addr),p->ai_addrlen ,s,sizeof(s));
		if(rv==0){
			fprintf(stderr,"info: connecting to %s\n", s);
		}

		connected=1;
		while (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			if(errno==EINTR && f_stop==0){
				continue;
			}
			connected=0;
			break;
		}
		if(connected==0){
			perror("connect");
			close(sockfd);
			if(f_stop==0){
				continue;
			}else{
				fprintf(stderr,"info: user canceled the connection\n");
				p=NULL;
				break;
			}
		}
		fprintf(stderr,"info: connected\n");
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "error: cannot connect\n");
		return INVALID_SOCKET;
	}

	freeaddrinfo(servinfo); // all done with this structure

	return sockfd;
}

//构造f_command，调用传输协议，向服务器请求文件
static int reqfile(int fd,char* filename)
{
	size_t i=0;
	int r=0;
	f_command * cmd=NULL;

	i=strlen(filename);
	cmd=f_command_new(REQ_FILE,filename,i+1,1,1);
	if(cmd==NULL){
		return -1;
	}
	r=f_snd_cmd(fd,cmd);
	f_command_free(cmd);

	return r;
}

//直接调用传输协议接收文件
static int rcvbigfile(int fd,char* filename)
{
	return f_rcv_file_in_pieces(fd,filename);
}

static void catch_sig_int(int signo)
{
	if(signo==SIGINT){
		f_stop=1;
		g_exit=1;
	}
	return;
}

