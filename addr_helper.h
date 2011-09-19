#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int addr_to_str(struct sockaddr_storage* addr, size_t addr_len,char* str,size_t str_len);

