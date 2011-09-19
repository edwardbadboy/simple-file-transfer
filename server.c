#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define INVALID_SOCKET	-1

#include "common.h"
#include "addr_helper.h"

static int sendbigf(int fd);
static int do_monitor(int sockfd);
static int accept_new_connection(int sockfd);
static int init_socket(char *port_str);
static int init_signals(void);
static int restore_signals(void);
static void catch_sig_int(int signo);

static volatile sig_atomic_t g_exit=0;
static struct sigaction chldact,oldchldact,intact,oldintact;

int main(int argc,char* argv[])
{
	int sockfd=0,r=0;

	if(argc!=2){
		fprintf(stderr,"usage: %s bind_port\n",argv[0]);
		return 1;
	}

	if(init_signals()==-1){
		return 1;
	}

	sockfd=init_socket(argv[1]);
	if(sockfd==INVALID_SOCKET){
		restore_signals();
		return 1;
	}

	if (listen(sockfd, 1) == -1){
		perror("listen");
		close(sockfd);
		restore_signals();
		return 1;
	}

	if(do_monitor(sockfd)<0){
		r=1;
	}
	
	if(close(sockfd)==-1){
		perror("close");
		r=1;
	}

	if(restore_signals()==-1){
		return r=1;
	}

	return r;
}

static int init_signals(void)
{
	int r=0;
	chldact.sa_handler=SIG_IGN;
	sigemptyset(&chldact.sa_mask);
	chldact.sa_flags=SA_NOCLDWAIT;
	r=sigaction(SIGCHLD,&chldact,&oldchldact);
	if(r==-1){
		perror("sigaction SIGCHLD");
		return -1;
	}

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
	r=sigaction(SIGCHLD,&oldchldact,NULL);
	if(r==-1){
		perror("restore SIGCHLD sigaction");
		return -1;
	}
	r=sigaction(SIGINT,&oldintact,NULL);
	if(r==-1){
		perror("restore SIGINT sigaction");
		return -1;
	}
	return 0;
}

static int do_monitor(int sockfd)
{
	int pid=0;
	int new_fd=0;

	while(1)
	{
		new_fd=accept_new_connection(sockfd);
		if(new_fd<0){
			if(g_exit==1){
				break;
			}
			continue;
		}

		pid=fork();
		if(pid==0){
			//我们进入了子进程
			int re;
			re=sendbigf(new_fd);
			if(re==0){
				fprintf(stderr,"info: send file OK.\n");
			}else{
				fprintf(stderr,"error: send file failed\n");
			}
			if(close(new_fd)==-1){
				perror("close");
			}
			fprintf(stderr,"info: child exits\n");
			exit(0);
		}else if(pid>0){
			//我们还在父进程里
			fprintf(stderr,"info: created PID: %d to send file.\n",pid);
		}else{
			perror("fork failed");
		}
		close(new_fd);
	}
	return 0;
}

int accept_new_connection(int sockfd)
{
	int new_fd=0;
	struct sockaddr_storage their_addr;
	size_t their_addr_len=0;
	char ipstr[INET6_ADDRSTRLEN]={0};
	int r=0;

	memset(&their_addr,0,sizeof(their_addr));
	their_addr_len = sizeof(their_addr);
	if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &their_addr_len)) == INVALID_SOCKET)
	{
		perror("accept");
		return -1;
	}

	r=addr_to_str(&their_addr,their_addr_len ,ipstr,sizeof(ipstr));
	if(r==0){
		fprintf(stderr,"info: accepted connection from %s\n",ipstr);
	}

	return new_fd;
}

static int init_socket(char *port_str)
{
	int sockfd=0;
	int sockflag=1;
	int rv=0;
	char ipstr[INET6_ADDRSTRLEN]={0};
	struct addrinfo hints, *servinfo=NULL, *p=NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	rv = getaddrinfo(NULL, port_str, &hints, &servinfo);
	if(rv!=0){
		fprintf(stderr, "error: getaddrinfo: %s\n", gai_strerror(rv));
		return INVALID_SOCKET;
	}
	
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == INVALID_SOCKET) {
			perror("socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockflag,sizeof(sockflag)) != 0) {
			perror("setsockopt");
			freeaddrinfo(servinfo);
			return INVALID_SOCKET;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
			perror("bind");
			close(sockfd);
			continue;
		}

		rv=addr_to_str((struct sockaddr_storage*)(p->ai_addr),p->ai_addrlen,ipstr,sizeof(ipstr));
		if(rv==0){
			fprintf(stderr,"info: server binded at %s\n",ipstr);
		}
		break;
	}

	if(p == NULL){
		fprintf(stderr, "error: server failed to bind\n");
		freeaddrinfo(servinfo);
		return INVALID_SOCKET;
	}

	freeaddrinfo(servinfo);

	return sockfd;
}

//此函数负责调用文件传输协议
static int sendbigf(int fd)
{
	char* fname=NULL;
	int r=0;
	f_command *cmd=NULL;
	cmd=f_rcv_cmd(fd);
	if(cmd==NULL){
		return -1;
	}
	if(strcmp(REQ_FILE,cmd->name)==0){
		cmd->data[cmd->data_len-1]=0;
		fname=cmd->data;
	}else{
		f_command_free(cmd);
		fprintf(stderr,"error: client sent bad command\n");
		return -1;
	}
	fprintf(stderr,"info: client asks for %s\n",fname);
	r=f_snd_file_in_pieces(fd,fname);
	f_command_free(cmd);
	return r;
}

static void catch_sig_int(int signo)
{
	if(signo==SIGINT){
		f_stop=1;
		g_exit=1;
	}
	return;
}

