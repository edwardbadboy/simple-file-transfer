
#ifndef COMMON_HEADER
#define COMMON_HEADER

#include <stdio.h>
#include <signal.h>
#include <limits.h>

#if ULONG_MAX<SIZE_MAX
	#error "ULONG_MAX must >= SIZE_MAX"
#endif

#ifdef WIN32
	#ifdef OUTPORT
		#define F_DLLDES __declspec(dllexport)  
	#else
		#define F_DLLDES __declspec(dllimport) 
	#endif
	typedef int ssize_t;
#else
	#ifdef OUTPORT
		#define F_DLLDES	
	#else
		#define F_DLLDES extern
	#endif
#endif


//此头文件定义文件传输协议的语义和语法

//定义几个命令
#define REQ_FILE "REQ_FILE"
#define RESP_DATA "RESP_DATA"
#define RESP_FIN "RESP_FIN"
#define RESP_ERROR "RESP_ERROR"

//定义一次处理数据的最大数量
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#ifndef PATH_MAX
	#define PATH_MAX 1024
#endif
#define F_MAXSIZE MAX(4096 ,PATH_MAX)
#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_MORE
	#define MSG_MORE 0
#endif


#ifndef WIN32
	#ifdef OUTPORT
		F_DLLDES extern volatile sig_atomic_t f_stop;
	#else
		F_DLLDES volatile sig_atomic_t f_stop;
	#endif
#else
	F_DLLDES extern volatile sig_atomic_t f_stop;
#endif

//一个表示在网络上传输的命令及其内容的结构体
typedef struct _f_command
{
	//'\0'结尾的字符串形式的命令名字
	char* name;
	//不同的命令对应的数据的意思可能不同
	char* data;
	//数据的长度
	size_t data_len;
	int own_data;
}f_command;

//处理数据的函数

//新建一个命令。将为命令名分配内存。data是命令的内容。copy_data指示是持有data的拷贝还是只是引用data。own_data指示是否接管data的所有权。
F_DLLDES f_command* f_command_new(const char * cmd_name,char * data, size_t data_len, int copy_data, int own_data);
//删除一个命令。释放为命令和命令名分配的内存。如果命令的内容是从别处拷贝的data，或在new时指示接管了data的所有权，那么还会释放data所占用的内存。
F_DLLDES void f_command_free(f_command *cmd);

//从网络上获取固定长度的数据
F_DLLDES int f_rcv_n(int fd,char* data,size_t data_len);
//向网络发送固定长度的数据
int f_snd_n(int fd,const char* data,size_t data_len,int more);

//用TCP协议，发送一个消息，并且保留消息边界fd的socket描述符，data是字节数组，data_len是数组长度
//返回0表示成功，其他值表示失败
F_DLLDES int f_snd_msg(int fd,const char* data,size_t data_len);
//用TCP协议，从网络上获取一个消息
F_DLLDES char* f_rcv_msg(int fd,size_t * data_len);

//发送一个命令，名字和数据分开成两个消息发送
F_DLLDES int f_snd_cmd(int fd,const f_command* cmd);
//将接收到的数据解析成程序可理解的命令
F_DLLDES f_command* f_rcv_cmd(int fd);

//遇到错误时将错误相关信息构造成命令通知给对方
F_DLLDES void f_snd_error_info(int fd,int err_no);

//发送文件，整个文件包含在一个消息中，适合于小文件
F_DLLDES int f_snd_file(int fd,const char* filename);
//接收文件，整个文件包含在一个消息中，适合于小文件
F_DLLDES int f_rcv_file(int fd,const char* filename);

//发送大文件，文件被分解成许多"RESP_DATA"命令发送出去。
F_DLLDES int f_snd_file_in_pieces(int fd,const char* filename);
//接收大文件，文件被分解成许多"RESP_DATA"命令发送出去。
F_DLLDES int f_rcv_file_in_pieces(int fd,const char* filename);

//协议的解释：TCP不为我们保留消息边界，以流的形式工作，在一方一次发送一个较长的流时，另一方需要多次调用接收函数才能接收完毕。
//如果程序的有意义的数据正好被这样截断，譬如一个4字节的int被截断后其语义就发生了变化，所以有必要设计自己的消息边界
//只要没有遇到消息边界，就一直接收，这样就能保证协议的基本语义的传送。
//在此基础上，再抽象出程序的逻辑语义——“命令”，将逻辑语义分解成基本语义传送。

//本文件传输协议包含3个命令：1、REQ_FILE，表示客户向服务器请求一个文件，命令的名字保存在f_command.cmd中，要请求的文件的路径保存在f_command.data中。
//2、RESP_DATA，表示服务器相应客户的请求，开始向客户发送文件的数据，其中命令的名字保存在f_command.cmd中，文件的数据在data中
//3、一次传输可以由一个REQ_FILE发起，后跟多个RESP_DATA，最后由RESP_FIN表示传输完成
//f_command结构体中的变量都是指针变量，其包含的数据在运行时动态加载，分配的内存空间在使用完后释放

#endif

