#include "addr_helper.h"

int addr_to_str(struct sockaddr_storage* addr, size_t addr_len,char* str,size_t str_len)
{
	void *ip_addr=NULL;
	struct sockaddr_in * ipv4=NULL;
	struct sockaddr_in6 * ipv6=NULL;
	char ipstr[INET6_ADDRSTRLEN]={0};
	char ports[NI_MAXSERV]={0};
	const char* s=NULL;

	if(addr->ss_family==AF_INET){
		ipv4=(struct sockaddr_in *)(addr);
		ip_addr=&(ipv4->sin_addr);
		sprintf(ports,"%hu",ntohs(ipv4->sin_port));
		s=inet_ntop(addr->ss_family,ip_addr,ipstr,sizeof(ipstr));
		if(s==NULL){
#ifdef DEBUG_OUT
			perror("inet_ntop");
#endif
			return -1;
		}
		snprintf(str,str_len,"%s:%s",ipstr,ports);
	}else if(addr->ss_family==AF_INET6){
		ipv6=(struct sockaddr_in6 *)(addr);
		ip_addr=&(ipv6->sin6_addr);
		sprintf(ports,"%hu",ntohs(ipv6->sin6_port));
		s=inet_ntop(addr->ss_family,ip_addr,ipstr,sizeof(ipstr));
		if(s==NULL){
#ifdef DEBUG_OUT
			perror("inet_ntop");
#endif
			return -1;
		}
		snprintf(str,str_len,"[%s]:%s",ipstr,ports);
	}else{
#ifdef DEBUG_OUT
		fprintf(stderr,"unkown address family %hu\n",addr->ss_family);
#endif
		return -1;
	}

	str[str_len-1]=0;
	return 0;
}

