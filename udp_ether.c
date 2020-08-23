#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <bits/ioctls.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <errno.h>

#define ETH_HDRLEN 14
#define IP4_HDRLEN 20
#define UDP_HDRLEN  8

uint16_t checksum (uint16_t *, int);
uint16_t udp4_checksum (struct ip, struct udphdr, uint8_t *, int);
char *allocate_strmem (int);
uint8_t *allocate_ustrmem (int);
int *allocate_intmem (int);

int main (int argc, char **argv) {
	int i, status, datalen, frame_length, sd, bytes, *ip_flags;
	char *interface, *target, *src_ip, *dst_ip;
	struct ip iphdr;
	struct udphdr udphdr;
	uint8_t *data, *src_mac, *dst_mac, *ether_frame;
	struct addrinfo hints, *res;
	struct sockaddr_in *ipv4;
	struct sockaddr_ll device;
	struct ifreq ifr;
	void *tmp;


	src_mac = allocate_ustrmem (6);
	dst_mac = allocate_ustrmem (6);
	data = allocate_ustrmem (IP_MAXPACKET);
	ether_frame = allocate_ustrmem (IP_MAXPACKET);
	interface = allocate_strmem (40);
	target = allocate_strmem (40);
	src_ip = allocate_strmem (INET_ADDRSTRLEN);
	dst_ip = allocate_strmem (INET_ADDRSTRLEN);
	ip_flags = allocate_intmem (4);


	strcpy (interface, "eth0");


	sd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL));


	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	ioctl (sd, SIOCGIFHWADDR, &ifr);
	close (sd);


	memcpy (src_mac, ifr.ifr_hwaddr.sa_data, 6 * sizeof (uint8_t));

	memset (&device, 0, sizeof (device));
	device.sll_ifindex = if_nametoindex (interface);
	printf ("Index for interface %s is %i\n", interface, device.sll_ifindex);


	dst_mac[0] = 0xff;
	dst_mac[1] = 0xff;
	dst_mac[2] = 0xff;
	dst_mac[3] = 0xff;
	dst_mac[4] = 0xff;
	dst_mac[5] = 0xff;


	strcpy (src_ip, "192.168.0.7");


	strcpy (target, "192.168.0.7");


	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = hints.ai_flags | AI_CANONNAME;


	getaddrinfo (target, NULL, &hints, &res);
	ipv4 = (struct sockaddr_in *) res->ai_addr;
	tmp = &(ipv4->sin_addr);
	inet_ntop (AF_INET, tmp, dst_ip, INET_ADDRSTRLEN);
	freeaddrinfo (res);


	device.sll_family = AF_PACKET;
	memcpy (device.sll_addr, src_mac, 6 * sizeof (uint8_t));
	device.sll_halen = 6;


	datalen = 4;
	data[0] = 'T';
	data[1] = 'e';
	data[2] = 's';
	data[3] = 't';


	iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);
	iphdr.ip_v = 4;
	iphdr.ip_tos = 0;
	iphdr.ip_len = htons (IP4_HDRLEN + UDP_HDRLEN + datalen);
	iphdr.ip_id = htons (0); 
	ip_flags[0] = 0;
	ip_flags[1] = 0;
	ip_flags[2] = 0;
	ip_flags[3] = 0;
	iphdr.ip_off = htons ((ip_flags[0] << 15)
	                      + (ip_flags[1] << 14)
	                      + (ip_flags[2] << 13)
	                      +  ip_flags[3]);

	iphdr.ip_ttl = 255;


	iphdr.ip_p = IPPROTO_UDP;


	if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		return 1;
	}


	if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		return 1;
	}


	iphdr.ip_sum = 0;
	iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);

	udphdr.source = htons (4950);
	udphdr.dest = htons (4950);
	udphdr.len = htons (UDP_HDRLEN + datalen);
	udphdr.check = udp4_checksum (iphdr, udphdr, data, datalen);

	frame_length = 6 + 6 + 2 + IP4_HDRLEN + UDP_HDRLEN + datalen;

	memcpy (ether_frame, dst_mac, 6 * sizeof (uint8_t));
	memcpy (ether_frame + 6, src_mac, 6 * sizeof (uint8_t));

	ether_frame[12] = ETH_P_IP / 256;
	ether_frame[13] = ETH_P_IP % 256;

	memcpy (ether_frame + ETH_HDRLEN, &iphdr, IP4_HDRLEN * sizeof (uint8_t));
	memcpy (ether_frame + ETH_HDRLEN + IP4_HDRLEN, &udphdr, UDP_HDRLEN * sizeof (uint8_t));
	memcpy (ether_frame + ETH_HDRLEN + IP4_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));


	if ((sd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 0) {
		perror ("socket() failed ");
		return 1;
	}


	sendto (sd, ether_frame, frame_length, 0, (struct sockaddr *) &device, sizeof (device));


	close (sd);


	free (src_mac);
	free (dst_mac);
	free (data);
	free (ether_frame);
	free (interface);
	free (target);
	free (src_ip);
	free (dst_ip);
	free (ip_flags);

	return 0;
}


uint16_t checksum (uint16_t *addr, int len) {
	int count = len;
	register uint32_t sum = 0;
	uint16_t answer = 0;


	while (count > 1) {
		sum += *(addr++);
		count -= 2;
	}


	if (count > 0) {
		sum += *(uint8_t *) addr;
	}


	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}


	answer = ~sum;

	return (answer);
}


uint16_t udp4_checksum (struct ip iphdr, struct udphdr udphdr, uint8_t *payload, int payloadlen) {
	char buf[IP_MAXPACKET];
	char *ptr;
	int chksumlen = 0;
	int i;

	ptr = &buf[0];


	memcpy (ptr, &iphdr.ip_src.s_addr, sizeof (iphdr.ip_src.s_addr));
	ptr += sizeof (iphdr.ip_src.s_addr);
	chksumlen += sizeof (iphdr.ip_src.s_addr);


	memcpy (ptr, &iphdr.ip_dst.s_addr, sizeof (iphdr.ip_dst.s_addr));
	ptr += sizeof (iphdr.ip_dst.s_addr);
	chksumlen += sizeof (iphdr.ip_dst.s_addr);


	*ptr = 0;
	ptr++;
	chksumlen += 1;


	memcpy (ptr, &iphdr.ip_p, sizeof (iphdr.ip_p));
	ptr += sizeof (iphdr.ip_p);
	chksumlen += sizeof (iphdr.ip_p);


	memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
	ptr += sizeof (udphdr.len);
	chksumlen += sizeof (udphdr.len);


	memcpy (ptr, &udphdr.source, sizeof (udphdr.source));
	ptr += sizeof (udphdr.source);
	chksumlen += sizeof (udphdr.source);


	memcpy (ptr, &udphdr.dest, sizeof (udphdr.dest));
	ptr += sizeof (udphdr.dest);
	chksumlen += sizeof (udphdr.dest);


	memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
	ptr += sizeof (udphdr.len);
	chksumlen += sizeof (udphdr.len);


	*ptr = 0;
	ptr++;
	*ptr = 0;
	ptr++;
	chksumlen += 2;


	memcpy (ptr, payload, payloadlen);
	ptr += payloadlen;
	chksumlen += payloadlen;


	for (i=0; i<payloadlen%2; i++, ptr++) {
		*ptr = 0;
		ptr++;
		chksumlen++;
	}

	return checksum ((uint16_t *) buf, chksumlen);
}


char * allocate_strmem (int len) {
	void *tmp;

	if (len <= 0) {
		fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
		exit (EXIT_FAILURE);
	}

	tmp = (char *) malloc (len * sizeof (char));
	if (tmp != NULL) {
		memset (tmp, 0, len * sizeof (char));
		return (tmp);
	} else {
		fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_strmem().\n");
		exit (EXIT_FAILURE);
	}
}


uint8_t * allocate_ustrmem (int len) {
	void *tmp;

	if (len <= 0) {
		fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_ustrmem().\n", len);
		exit (EXIT_FAILURE);
	}

	tmp = (uint8_t *) malloc (len * sizeof (uint8_t));
	if (tmp != NULL) {
		memset (tmp, 0, len * sizeof (uint8_t));
		return (tmp);
	} else {
		fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_ustrmem().\n");
		exit (EXIT_FAILURE);
	}
}


int * allocate_intmem (int len) {
	void *tmp;

	if (len <= 0) {
		fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_intmem().\n", len);
		exit (EXIT_FAILURE);
	}

	tmp = (int *) malloc (len * sizeof (int));
	if (tmp != NULL) {
		memset (tmp, 0, len * sizeof (int));
		return (tmp);
	} else {
		fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_intmem().\n");
		exit (EXIT_FAILURE);
	}
}
