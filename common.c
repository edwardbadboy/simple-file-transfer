
//此文件实现文件传输协议
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <assert.h>

#ifdef WIN32
	#define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料
	#include <winsock2.h>
#else
	#include <sys/socket.h>
	#include <arpa/inet.h>
#endif
#define OUTPORT
#include "common.h"

volatile sig_atomic_t f_stop=0;

//#define DEBUG_OUT

//获取本地文件大小
long filesize(FILE *stream)
{
	long curpos=0, length=0;

	if((curpos = ftell(stream))==-1 ){
		return -1;
	}
	if(fseek(stream, 0L, SEEK_END)==-1){
		return -1;
	}
	if((length = ftell(stream))==-1){
		return -1;
	}
	if(fseek(stream, curpos, SEEK_SET)==-1){
		return -1;
	}
	return length;
}

f_command* f_command_new(const char * cmd_name,char * data, size_t data_len, int copy_data, int own_data)
{
	f_command *cmd=NULL;
	size_t name_len=0;

	cmd=malloc(sizeof(f_command));
	if(cmd==NULL){
#ifdef DEBUG_OUT
		perror("malloc");
#endif
		return NULL;
	}

	name_len=strlen(cmd_name);
	cmd->name=malloc(name_len+1);
	if(cmd->name==NULL){
#ifdef DEBUG_OUT
		perror("malloc");
#endif
		free(cmd);
		return NULL;
	}

	if(strcpy(cmd->name,cmd_name)==NULL){
		free(cmd->name);
		free(cmd);
		return NULL;
	}

	cmd->own_data=own_data || copy_data ;

	if(data==NULL){
		cmd->data=NULL;
		cmd->data_len=0;
		return cmd;
	}

	cmd->data_len=data_len;
	if(copy_data==0){
		cmd->data=data;
		return cmd;
	}

	cmd->data=malloc(data_len);
	if(cmd->data==NULL){
#ifdef DEBUG_OUT
		perror("malloc");
#endif
		free(cmd->name);
		free(cmd);
		return NULL;
	}
	if(memcpy(cmd->data,data,data_len)==NULL){
		free(cmd->name);
		free(cmd);
		return NULL;
	}

	return cmd;
}

void f_command_free(f_command *cmd)
{
	free(cmd->name);
	if(cmd->own_data && cmd->data!=NULL){
		free(cmd->data);
	}
	free(cmd);
	return;
}

//从网络上获取固定长度的数据
int f_rcv_n(int fd,char* data,size_t data_len)
{
	char *t=NULL;
	size_t rcvd=0;
	ssize_t r=0;

	t=data;
	//只要数据没有接收完毕，则一直继续接收
	while(rcvd<data_len)
	{
		r=recv(fd, t, MIN(data_len-rcvd,INT_MAX), 0);
		if(r<0 || f_stop==1){
			if(errno==EINTR && f_stop==0){
				continue;
			}
#ifdef DEBUG_OUT
			perror("recv");
#endif
			return -1;
		}else if(r==0){
#ifdef DEBUG_OUT
			perror("recv");
#endif
			return -1;
		}
		t+=r;
		rcvd+=r;
	}
	return 0;
}

//向网络发送固定长度的数据
int f_snd_n(int fd,const char* data,size_t data_len,int more)
{
	size_t snt=0;
	ssize_t r=0;
	const char *t=NULL;

	t=data;
	//只要消息没有发送完毕就一直发送
	while(snt<data_len)
	{
		r=send(fd, t, MIN(data_len-snt,INT_MAX), MSG_NOSIGNAL | (more?MSG_MORE:0));
		if( (r<0) || (f_stop==1) ){
			if((errno==EINTR) && (f_stop==0)){
				continue;
			}
#ifdef DEBUG_OUT
			perror("send");
#endif
			return -1;
		}
		t+=r;
		snt+=r;
	}
	return 0;
}

//从网络上获取一个消息
char* f_rcv_msg(int fd,size_t * data_len)
{
	char *data=NULL;
	size_t len=0;
	int r=0;

	//在common.h已经用条件编译在预处理时就检查过了，但是为了保险起见，这里再检查一次
	//这是为了确保我们的size_t类型的len能够被ntohl处理
	assert(ULONG_MAX>=SIZE_MAX);
	assert(sizeof(u_long)==sizeof(size_t));

	//首先获取消息的大小
	r=f_rcv_n(fd,(char*)&len,sizeof(len));
	if(r<0){
		*data_len=-1;
		return NULL;
	}
	len=ntohl(len);

	//为接下来的消息正文分配内存
	data=(char*)malloc(len);
	if(data==NULL){
#ifdef DEBUG_OUT
		perror("malloc");
#endif
		*data_len=-1;
		return NULL;
	}

	//获取消息正文
	r=f_rcv_n(fd,data,len);
	if(r<0){
		free(data);
		*data_len=-1;
		return NULL;
	}

	*data_len=len;
	return data;
}

//向网络发送一个消息
int f_snd_msg(int fd,const char* data,size_t data_len)
{
	size_t len=0;
	int r=0;

	assert(ULONG_MAX>=SIZE_MAX);
	assert(sizeof(u_long)==sizeof(size_t));

	if(data_len>ULONG_MAX){
		return -1;
	}
	len=htonl(data_len);
	r=f_snd_n(fd,(const char*)&len,sizeof(len),1);
	if(r<0){
		return -1;
	}

	r=f_snd_n(fd,data,data_len,0);
	if(r<0){
		return -1;
	}
	
	return 0;
}

//把一个命令分解成消息传送到网络上
int f_snd_cmd(int fd,const f_command* cmd)
{
	int r=0;
	r=f_snd_msg(fd,cmd->name,strlen(cmd->name)+1);
	if(r==-1){
		return -1;
	}
	if(cmd->data!=NULL){
		r=f_snd_msg(fd,cmd->data,cmd->data_len);
		if(r==-1){
			return -1;
		}
	}
	return 0;
}

//把从网络上接收到的消息组合成命令
f_command* f_rcv_cmd(int fd)
{
	f_command *cmd=NULL;
	size_t len=0;
	char *name=NULL,*data=NULL;

	name=f_rcv_msg(fd,&len);
	if(name==NULL){	
#ifdef DEBUG_OUT
		fprintf(stdout,"cannot get cmd name\n");
#endif
		return NULL;
	}
	
	if(strcmp(RESP_FIN,name)==0){
		cmd=f_command_new(name,NULL,0,0,1);
	}else{
		data=f_rcv_msg(fd,&len);
		if(data==NULL){
			free(name);
			return NULL;
		}
		cmd=f_command_new(name,data,len,0,1);
	}

	free(name);
	return cmd;
}

//直接把文件放在一个消息中发送
int f_snd_file(int fd,const char* filename)
{
	FILE* in=NULL;
	char* data;
	size_t r=0,len=0;
	int snd_r=0;
	in=fopen(filename,"rb");
	if(in==NULL){
		f_snd_error_info(fd,errno);
#ifdef DEBUG_OUT
		perror("fopen");
#endif
		return -1;
	}

	if((len=filesize(in))==-1){
		f_snd_error_info(fd,errno);
#ifdef DEBUG_OUT
		perror("get file length");
#endif
		return -1;
	}

	data=malloc(len);
	if(data==NULL){	
		f_snd_error_info(fd,errno);
#ifdef DEBUG_OUT
		perror("malloc");
#endif
		fclose(in);
		return -1;
	}
	r=fread(data,1,len,in);
	if(r<len){
#ifdef DEBUG_OUT
		perror("fread");
#endif
		free(data);
		fclose(in);
		return -1;
	}
	fclose(in);

	snd_r=f_snd_msg(fd,data,len);

	free(data);
	if(snd_r==-1){
		return -1;
	}
	return 0;
}

//直接从包含一整个文件的消息中取出文件，与上面的函数是一对，没有用到命令的功能。
int f_rcv_file(int fd,const char* filename)
{
	FILE* out=NULL;
	char* data=NULL;
	size_t r=0,len=0;
	out=fopen(filename,"wb");
	if(out==0){
		return -1;
	}
	data=f_rcv_msg(fd,&len);
	if(data==NULL){
		fclose(out);
		return -1;
	}
	r=fwrite(data,len,1,out);
	fclose(out);
	free(data);
	return 0;
}

//发送文件的算法，利用RESP_DATA命令不断的传输数据，数据发送完毕后发送一个RESP_FIN命令
int f_snd_file_in_pieces(int fd,const char* filename)
{
	FILE* in=NULL;
	char data[F_MAXSIZE]={0};
	size_t r=0;
	f_command* cmd=NULL;
	in=fopen(filename,"rb");
	if(in==NULL){
		f_snd_error_info(fd,errno);
#ifdef DEBUG_OUT
		perror("fopen");
#endif
		return -1;
	}

	while(feof(in)==0)
	{	
		r=fread(data,1,F_MAXSIZE,in);
		if(r==0){
			break;
		}
		if(r==-1){
			fclose(in);
			return -1;
		}

#ifdef DEBUG_OUT
		fprintf(stdout,"read file block size %u\n",r);
#endif

		cmd=f_command_new("RESP_DATA",data,r,0,0);
		if(cmd==NULL){
			f_snd_error_info(fd,errno);
			fclose(in);
			return -1;
		}
		if(f_snd_cmd(fd,cmd)!=0){
			f_command_free(cmd);
			fclose(in);
			return -1;
		}
		f_command_free(cmd);
	}
	fclose(in);

	cmd=f_command_new("RESP_FIN",NULL,0,0,1);
	if(cmd==NULL){
		f_snd_error_info(fd,errno);
		return -1;
	}
	if(f_snd_cmd(fd,cmd)!=0){
		f_command_free(cmd);
		return -1;
	}
	f_command_free(cmd);
	return 0;
}

//接收文件的算法，把数据从不断到来的RESP_DATA命令中提取出来，直到收到到RESP_FIN命令为止
int f_rcv_file_in_pieces(int fd,const char* filename)
{
	FILE* out=NULL;
	size_t r=0;
	int result=0;
	f_command * cmd=NULL;
	out=fopen(filename,"wb");
	if(out==NULL){
		return -1;
	}
	while(1)
	{
		cmd=f_rcv_cmd(fd);
		if(cmd==NULL){
#ifdef DEBUG_OUT
			fprintf(stdout,"cannot get cmd\n");
#endif
			result=-1;
			break;
		}
		if(strcmp(RESP_DATA,cmd->name)==0){
			r=fwrite(cmd->data,cmd->data_len,1,out);
			f_command_free(cmd);
		}else if(strcmp(RESP_FIN,cmd->name)==0){
			f_command_free(cmd);
			break;
		}else if(strcmp(RESP_ERROR,cmd->name)==0){
#ifdef DEBUG_OUT
			cmd->data[cmd->data_len-1]=0;
			fprintf(stdout,"error duiring file transfer: %s\n",cmd->data);
#endif
			f_command_free(cmd);
			result=-1;
			break;
		}else{
#ifdef DEBUG_OUT
			fprintf(stdout,"got unknown cmd\n");
#endif
			f_command_free(cmd);
		}
	}
	fclose(out);
	return result;
}

void f_snd_error_info(int fd,int err_no)
{
	f_command *cmd=NULL;
	char buf[256]={0};
	char *err_info=NULL;
	int result=0;
#ifndef WIN32
	result=strerror_r(err_no,buf,sizeof(buf)-1);
#else
	result=strerror_s(buf,sizeof(buf)-1,err_no);
#endif
	if(result!=0){
		strncpy(buf,"cannot get error info",sizeof(buf)-1);
	}
	err_info=buf;
	cmd=f_command_new(RESP_ERROR,err_info,strlen(err_info)+1,1,1);
	if(cmd==NULL){
		return;
	}
	f_snd_cmd(fd,cmd);
	f_command_free(cmd);
	return;
}

