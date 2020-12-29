#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "libclocksync.h"

int				DIE = 0;

int				shmem_fd = -1;
volatile clocksync_shmem*	shmem_ptr = NULL;

int				print_delta = 0;

int				is_multicast = 0;
int				is_master = 0;
int				is_ipv6 = 0;

int				udp_socket = -1;
struct sockaddr*		udp_addr = NULL;
size_t				udp_addr_size;
struct sockaddr_in		udp_addr4;
struct sockaddr_in6		udp_addr6;

static void sigma(int x) {
	if (++DIE >= 10) _exit(0);
}

static void help() {
	fprintf(stderr,"clocksync [options]\n");
	fprintf(stderr,"master or slave daemon to maintain synchronized clocks across computers\n");
	fprintf(stderr,"(C) 2009 Impact Studio Pro. Written by Jonathan Campbell\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  -4      IPv4 (default)\n");
	fprintf(stderr,"  -6      IPv6\n");
	fprintf(stderr,"  -m      Run as master, transmit clock to slaves\n");
	fprintf(stderr,"  -s      Run as slave, receive from master and sync\n");
	fprintf(stderr,"  -a <a>  Use this address\n");
	fprintf(stderr,"  -p      Print time and delta\n");
}

static int parse_argv(int argc,char **argv) {
	const char *addr = NULL;
	int i;

	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-') {
			while (*a == '-') a++;

			if (!strcmp(a,"4")) {
				is_ipv6 = 0;
			}
			else if (!strcmp(a,"6")) {
				is_ipv6 = 1;
			}
			else if (!strcmp(a,"m")) {
				is_master = 1;
			}
			else if (!strcmp(a,"s")) {
				is_master = 0;
			}
			else if (!strcmp(a,"a")) {
				addr = argv[i++];
			}
			else if (!strcmp(a,"p")) {
				print_delta = 1;
			}
			else if (!strcmp(a,"help")) {
				help();
				return 1;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				help();
				return 1;
			}
		}
		else {
		}
	}

	if (addr == NULL) {
		fprintf(stderr,"You must specify an address\n");
		help();
		return 1;
	}

	if (is_ipv6) {
		memset(&udp_addr6,0,sizeof(udp_addr6));
		udp_addr6.sin6_family = AF_INET6;
		udp_addr6.sin6_port = htons(CLOCKSYNC_PORT);
		if (inet_pton(AF_INET6,addr,&udp_addr6.sin6_addr) < 0) {
			fprintf(stderr,"Cannot parse IPv6 address\n");
			return 1;
		}
		udp_addr = (struct sockaddr*)(&udp_addr6);
		udp_addr_size = sizeof(udp_addr6);

		is_multicast = udp_addr6.sin6_addr.s6_addr[0] == 0xFF;
	}
	else {
		memset(&udp_addr4,0,sizeof(udp_addr4));
		if (inet_aton(addr,&udp_addr4.sin_addr) < 0) {
			fprintf(stderr,"Cannot parse IPv4 address\n");
			return 1;
		}
		udp_addr4.sin_family = AF_INET;
		udp_addr4.sin_port = htons(CLOCKSYNC_PORT);
		udp_addr = (struct sockaddr*)(&udp_addr4);
		udp_addr_size = sizeof(udp_addr4);

		is_multicast = (htonl(udp_addr4.sin_addr.s_addr) & 0xF0000000) == 0xE0000000;
	}

	printf("multicast=%d\n",is_multicast);
	return 0;
}

void bswap4(char *x) {
	char t;

	t   = x[0];	x[0] = x[3];	x[3] = t;
	t   = x[1];	x[1] = x[2];	x[2] = t;
}

void bswap8(char *x) {
	char t;

	t   = x[0];	x[0] = x[7];	x[7] = t;
	t   = x[1];	x[1] = x[6];	x[6] = t;
	t   = x[2];	x[2] = x[5];	x[5] = t;
	t   = x[3];	x[3] = x[4];	x[4] = t;
}

int main(int argc,char **argv) {
	int so;

	if (parse_argv(argc,argv))
		return 1;

	/* set up socket */
	if (is_ipv6)	udp_socket = socket(AF_INET6,SOCK_DGRAM,0);
	else		udp_socket = socket(AF_INET,SOCK_DGRAM,0);

	if (udp_socket < 0) {
		fprintf(stderr,"Cannot create socket, %s\n",strerror(errno));
		return 1;
	}

	/* please don't do IPv4-in-IPv6 */
	if (is_ipv6) {
		so = 1;
		if (setsockopt(udp_socket,IPPROTO_IPV6,IPV6_V6ONLY,&so,sizeof(so)) < 0)
			fprintf(stderr,"Cannot set IPV6_ONLY %s\n",strerror(errno));
	}

	/* don't send me my own packets */
	if (is_multicast) {
		so = 0;
		if (setsockopt(udp_socket,IPPROTO_IP,IP_MULTICAST_LOOP,&so,sizeof(so)) < 0)
			fprintf(stderr,"Cannot set IP_MULTICAST_LOOP\n");

		if (is_ipv6) {
			so = 0;
			if (setsockopt(udp_socket,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,&so,sizeof(so)) < 0)
				fprintf(stderr,"Cannot set IPV6_MULTICAST_LOOP\n");
		}
	}

	/* broadcast please */
	so = 1;
	if (setsockopt(udp_socket,SOL_SOCKET,SO_BROADCAST,&so,sizeof(so)) < 0)
		fprintf(stderr,"Cannot set SO_BROADCAST %s\n",strerror(errno));

	/* allow others to reuse this port please */
	so = 1;
	if (setsockopt(udp_socket,SOL_SOCKET,SO_REUSEADDR,&so,sizeof(so)) < 0)
		fprintf(stderr,"Cannot set SO_REUSEADDR %s\n",strerror(errno));

	/* bind */
	if (bind(udp_socket,udp_addr,udp_addr_size) < 0) {
		fprintf(stderr,"Cannot bind socket, %s\n",strerror(errno));
		return 1;
	}

	/* join multicast, if needed */
	if (is_multicast) {
		if (is_ipv6) {
			struct ipv6_mreq i;
			memcpy(&i.ipv6mr_multiaddr,&udp_addr6.sin6_addr,sizeof(struct in6_addr));
			i.ipv6mr_interface = 0;
			if (setsockopt(udp_socket,IPPROTO_IPV6,IPV6_JOIN_GROUP,&i,sizeof(i)) < 0)
				fprintf(stderr,"Cannot add multicast ipv6 membership\n");
		}
		else {
			struct ip_mreq i;
			i.imr_multiaddr.s_addr = udp_addr4.sin_addr.s_addr;
			i.imr_interface.s_addr = INADDR_ANY;
			if (setsockopt(udp_socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&i,sizeof(i)) < 0)
				fprintf(stderr,"Cannot add multicast membership\n");
		}
	}

	/* setup shmem */
	shmem_fd = open(CLOCKSYNC_SHMEM_PATH,O_RDWR|O_CREAT|O_TRUNC,0644);
	if (shmem_fd < 0) {
		fprintf(stderr,"Cannot create shared memory area\n");
		return 1;
	}
	if (ftruncate(shmem_fd,4096) < 0) {
		fprintf(stderr,"Cannot ftruncate shared memory area\n");
		return 1;
	}
	shmem_ptr = (volatile clocksync_shmem*)mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,shmem_fd,0);
	if (shmem_ptr == (volatile clocksync_shmem*)(-1)) {
		fprintf(stderr,"Cannot mmap\n");
		return 1;
	}
	memset((void*)shmem_ptr,0,sizeof(*shmem_ptr));

	signal(SIGINT, sigma);
	signal(SIGQUIT,sigma);
	signal(SIGTERM,sigma);

	/* main loop, either as master, send clock, or slave, get time */
	if (is_master) {
		/* we're master, so naturally, there's no difference :) */
		shmem_ptr->sig = CLOCKSYNC_SIG;
		shmem_ptr->time = 0.0f;

		char sequence = (char)rand();
		/* periodically send our time out to others.
		 * if others send a ping, send a ping back */
		while (DIE == 0) {
			struct timeval tv;
			fd_set fd;

			FD_ZERO(&fd);
			FD_SET(udp_socket,&fd);
			tv.tv_sec = 0;
			tv.tv_usec = 500000;
			if (select(udp_socket+1,&fd,NULL,NULL,&tv) > 0) {
				double now = clocksync_local();
				/* incoming packet. read. */
				/* note packet can be IPv4 or IPv6. so use a char[] to hold addr.
				 * we need the addr to know where to send responses back to */
				char tmp[256],addr[256];
				socklen_t addrlen = sizeof(addr);
				int rd = recvfrom(udp_socket,tmp,sizeof(tmp)-1,0,(struct sockaddr*)addr,&addrlen);
				if (rd > 0) {
					tmp[rd] = 0;

					if (tmp[0] == 'P') {	/* ping */
						/* ping response */
						              tmp[0] = 'p';
						*((uint32_t*)(tmp+1)) = 0x01020304;		/* byte order */
						*((double*)  (tmp+5)) = now;			/* time */
						if (sendto(udp_socket,tmp,1+4+8,0,(struct sockaddr*)addr,addrlen) < 0)
							fprintf(stderr,"cannot send ping response\n");

						/* wait, to avoid ping floods */
						usleep(100000);
					}
				}
			}
			else {
				/* nothing. send packet. loop around. select will wait another 100ms or until another packet */
				char data[1+1+4+8];				/* char + char + 32-bit DWORD + double */
				              data[0]  = 'T';			/* tick */
					      data[1]  = sequence++;		/* sequence */
				*((uint32_t*)(data+2)) = 0x01020304;		/* byte order */
				*((double*)  (data+6)) = clocksync_local();	/* time */
				if (sendto(udp_socket,data,1+1+4+8,0,udp_addr,udp_addr_size) < 8)
					fprintf(stderr,"Problem sending packet\n");

				if (print_delta)
					printf("sent: %.3f\n",*((double*)(data+6)));
			}
		}
	}
	else {
		/* pick up timestamps from master. store difference in shared memory segment, so clients can merely "get"
		 * the master clock by getting the current time and adding the difference. our job is to get updates from
		 * the master, and use averaging to smooth out jitters caused by interrupts, network timing, etc. */
		double cur_diff = 0;
		double new_diff = 0;
		int first_diff = 1;
		char last_seq = 0;
		double ptime = -1;
		int init_seq = 1;
		char tmp[256];
		int rd;

		while (DIE == 0) {
			struct timeval tv;
			fd_set fd;

			FD_ZERO(&fd);
			FD_SET(udp_socket,&fd);
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			int n = select(udp_socket+1,&fd,NULL,NULL,&tv);

			double now = clocksync_local();
			if (ptime < 0) ptime = now - 0.1;
			double delta = now - ptime;
			ptime = now;

			if (n > 0) {
				rd = recv(udp_socket,tmp,sizeof(tmp)-1,0);
				if (rd == 14) {
					if (tmp[0] == 'T') {
						if (init_seq) {
							last_seq = tmp[1];
							init_seq = 0;
						}
						else {
							char seqd = tmp[1] - last_seq;
							if (seqd > 0) {
								last_seq = tmp[1];

								/* byte order conversion. this allows PowerPC based masters to work even
								 * though PowerPC is the opposite byte order from Intel */
								if (*((uint32_t*)(tmp+2)) == 0x04030201) {
									bswap4(tmp+2);
									bswap8(tmp+6);
								}

								/* if SIOCGSTAMP is available, use that */
#if defined(SIOCGSTAMP)
								double packet_now = -1;
								{
									struct timeval tv;
									if (ioctl(udp_socket,SIOCGSTAMP,&tv) == 0)
										packet_now = ((double)tv.tv_sec) + (((double)tv.tv_usec) / 1000000);
								}
#else
								double packet_now = now;
#endif

								double mt = *((double*)(tmp+6));
								if (print_delta) printf("master=%.3f\n",mt);
								new_diff = mt - packet_now;
							}
						}
					}
				}
			}

			/* adjust clock */
			if (first_diff) {
				if (new_diff != 0.0) {
					cur_diff = new_diff;
					first_diff = 0;
				}
			}
			else {
				double m = delta / 5;
				if (m < 0.00) m = 0.00;
				if (m > 0.85) m = 0.85;
				cur_diff = (cur_diff * (1.0 - m)) + (new_diff * m);
			}

			/* carry on our average to other processes */
			shmem_ptr->sig = CLOCKSYNC_SIG;
			shmem_ptr->time = (float)cur_diff;

			if (print_delta)
				printf("diff=%.3f new=%.3f\n",cur_diff,new_diff);
		}
	}

	munmap((void*)shmem_ptr,4096);
	unlink(CLOCKSYNC_SHMEM_PATH);
	close(shmem_fd);
	return 0;
}

