/*
 * Copyright (c) 2012, devolo AG
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <fcntl.h>

#include "network.h"

#define PORT 22208

static int cfg_socket = -1;
static struct sockaddr_in addr, bindaddr, localaddr;

static int get_local_address(struct in_addr *in, const char *interface)
{
	struct ifreq ifr;
	int ret;

	strcpy(ifr.ifr_name, interface);
	ret = ioctl(cfg_socket, SIOCGIFADDR, &ifr);
	if (ret < 0)
	{
		printf("ERROR: unable to get interface.\n");
		return -1;
	}
	memcpy(in, &((struct sockaddr_in *) (&ifr.ifr_addr))->sin_addr, sizeof(((struct sockaddr_in *) (&ifr.ifr_addr))->sin_addr));

	return 0;
}

static int get_broadcast_address(struct in_addr *in, const char *interface)
{
	struct ifreq ifr;
	int ret;

	strcpy(ifr.ifr_name, interface);
	ret = ioctl(cfg_socket, SIOCGIFBRDADDR, &ifr);
	if (ret < 0)
	{
		printf("ERROR: unable to get broadcast address.\n");
		return -1;
	}

	if (ifr.ifr_broadaddr.sa_family != AF_INET)
	{
		printf("ERROR: unsupported address family.\n");
		return -1;
	}

	memcpy(in, &((struct sockaddr_in *) (&ifr.ifr_broadaddr))->sin_addr, sizeof(((struct sockaddr_in *) (&ifr.ifr_broadaddr))->sin_addr));

	return 0;
}

int netw_open(const char *interface)
{
	int ret = 0;

	if (!interface)
	{
		printf("ERROR: No interface specified\n");
		return -1;
	}

	if(-1 != cfg_socket)
	{
		printf("ERROR: Network already initialized.\n");
		return -1;
	}

	cfg_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(0 > cfg_socket)
	{
		printf("ERROR: Unable to initialize network socket\n");
		return -1;
	}

	ret = get_local_address(&(localaddr.sin_addr), interface);
	if (ret)
		return ret;

	printf("netw_open: local address %s\n", inet_ntoa(localaddr.sin_addr));

	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindaddr.sin_port = htons(PORT);

	if(-1 == bind(cfg_socket, (struct sockaddr *) &bindaddr, sizeof(bindaddr)))
	{
		printf("ERROR: Unable to bind socket\n");
		netw_close();
		return -1;
	}

	int optval = 1;
	setsockopt(cfg_socket, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);

	ret = get_broadcast_address(&(addr.sin_addr), interface);
	if (ret)
		return ret;

	printf("netw_open: broadcast address %s\n", inet_ntoa(addr.sin_addr));

	return 0;
}

int netw_close(void)
{
	if(-1 != cfg_socket)
	{
		close(cfg_socket);
		cfg_socket = -1;
		return 0;
	}
	return -1;
}


int netw_send(unsigned char *data, int datalen)
{
	if(-1 == cfg_socket) return -1;
	if(0 == datalen) return -2;
	
	int bytes_sent = sendto(cfg_socket, data, datalen, 0 /* flags */,
		(struct sockaddr *) &addr, sizeof(addr));

	if(-1 == bytes_sent)
		perror("sendto ERROR: ");

	return bytes_sent;
}

int netw_receive(unsigned char *buf, int maxbuflen)
{
//	return recv(cfg_socket, buf, maxbuflen, MSG_DONTWAIT);

	struct sockaddr_in raddr;
	socklen_t address_len = sizeof(raddr);

	int r = recvfrom(cfg_socket, buf, maxbuflen, MSG_DONTWAIT,
		(struct sockaddr *) &raddr, &address_len);

	if(raddr.sin_addr.s_addr == localaddr.sin_addr.s_addr)
		return 0;

	return r;
}

int netw_get_fd(void)
{
	return cfg_socket;
}

