
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


//��ͷ�ļ������ļ�����Э���������﷨

//���弸������
#define REQ_FILE "REQ_FILE"
#define RESP_DATA "RESP_DATA"
#define RESP_FIN "RESP_FIN"
#define RESP_ERROR "RESP_ERROR"

//����һ�δ������ݵ��������
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

//һ����ʾ�������ϴ������������ݵĽṹ��
typedef struct _f_command
{
	//'\0'��β���ַ�����ʽ����������
	char* name;
	//��ͬ�������Ӧ�����ݵ���˼���ܲ�ͬ
	char* data;
	//���ݵĳ���
	size_t data_len;
	int own_data;
}f_command;

//�������ݵĺ���

//�½�һ�������Ϊ�����������ڴ档data����������ݡ�copy_dataָʾ�ǳ���data�Ŀ�������ֻ������data��own_dataָʾ�Ƿ�ӹ�data������Ȩ��
F_DLLDES f_command* f_command_new(const char * cmd_name,char * data, size_t data_len, int copy_data, int own_data);
//ɾ��һ������ͷ�Ϊ�����������������ڴ档�������������Ǵӱ𴦿�����data������newʱָʾ�ӹ���data������Ȩ����ô�����ͷ�data��ռ�õ��ڴ档
F_DLLDES void f_command_free(f_command *cmd);

//�������ϻ�ȡ�̶����ȵ�����
F_DLLDES int f_rcv_n(int fd,char* data,size_t data_len);
//�����緢�͹̶����ȵ�����
int f_snd_n(int fd,const char* data,size_t data_len,int more);

//��TCPЭ�飬����һ����Ϣ�����ұ�����Ϣ�߽�fd��socket��������data���ֽ����飬data_len�����鳤��
//����0��ʾ�ɹ�������ֵ��ʾʧ��
F_DLLDES int f_snd_msg(int fd,const char* data,size_t data_len);
//��TCPЭ�飬�������ϻ�ȡһ����Ϣ
F_DLLDES char* f_rcv_msg(int fd,size_t * data_len);

//����һ��������ֺ����ݷֿ���������Ϣ����
F_DLLDES int f_snd_cmd(int fd,const f_command* cmd);
//�����յ������ݽ����ɳ������������
F_DLLDES f_command* f_rcv_cmd(int fd);

//��������ʱ�����������Ϣ���������֪ͨ���Է�
F_DLLDES void f_snd_error_info(int fd,int err_no);

//�����ļ��������ļ�������һ����Ϣ�У��ʺ���С�ļ�
F_DLLDES int f_snd_file(int fd,const char* filename);
//�����ļ��������ļ�������һ����Ϣ�У��ʺ���С�ļ�
F_DLLDES int f_rcv_file(int fd,const char* filename);

//���ʹ��ļ����ļ����ֽ�����"RESP_DATA"����ͳ�ȥ��
F_DLLDES int f_snd_file_in_pieces(int fd,const char* filename);
//���մ��ļ����ļ����ֽ�����"RESP_DATA"����ͳ�ȥ��
F_DLLDES int f_rcv_file_in_pieces(int fd,const char* filename);

//Э��Ľ��ͣ�TCP��Ϊ���Ǳ�����Ϣ�߽磬��������ʽ��������һ��һ�η���һ���ϳ�����ʱ����һ����Ҫ��ε��ý��պ������ܽ�����ϡ�
//����������������������ñ������ضϣ�Ʃ��һ��4�ֽڵ�int���ضϺ�������ͷ����˱仯�������б�Ҫ����Լ�����Ϣ�߽�
//ֻҪû��������Ϣ�߽磬��һֱ���գ��������ܱ�֤Э��Ļ�������Ĵ��͡�
//�ڴ˻����ϣ��ٳ����������߼����塪������������߼�����ֽ�ɻ������崫�͡�

//���ļ�����Э�����3�����1��REQ_FILE����ʾ�ͻ������������һ���ļ�����������ֱ�����f_command.cmd�У�Ҫ������ļ���·��������f_command.data�С�
//2��RESP_DATA����ʾ��������Ӧ�ͻ������󣬿�ʼ��ͻ������ļ������ݣ�������������ֱ�����f_command.cmd�У��ļ���������data��
//3��һ�δ��������һ��REQ_FILE���𣬺�����RESP_DATA�������RESP_FIN��ʾ�������
//f_command�ṹ���еı�������ָ������������������������ʱ��̬���أ�������ڴ�ռ���ʹ������ͷ�

#endif

