/* Simple multicast ping program with congestion control
 *
 * Taken from the book "Multicast Sockets - Practical Guide for Programmers"
 * written by David Makofske and Kevin Almeroth.  For online information see
 * http://www.nmsl.cs.ucsb.edu/MulticastSocketsBook/
 */

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <net/if.h>		/* struct ifreq */
#include <sys/ioctl.h>		/* SIOCGIFADDR */

#include "mping.h"

#define MC_GROUP_DEFAULT "239.255.255.1"
#define MC_PORT_DEFAULT  10000
#define MC_TTL_DEFAULT   1

struct response_buffer *resp_buf[RESPONSE_BUFFER_SIZE];
int empty_location = 0;

/*#define BANDWIDTH 10000.0 */          /* bw in bytes/sec for mping */
#define BANDWIDTH 100.0                 /* bw in bytes/sec for mping */

unsigned int last_pkt_count = 0;        /* packets heard in last full second */
unsigned int curr_pkt_count = 0;        /* packets heard so far this second */

struct timeval current_time;

/* pointer to mping packet buffer */
struct mping_struct *rcvd_pkt;

int sock;			/* socket descriptor */
pid_t pid;			/* pid of mping program */

struct sockaddr_in mc_addr;	/* socket address structure */
struct ip_mreq mc_request;	/* multicast request structure */

struct in_addr localIP;		/* address struct for local IP */

/* counters and statistics variables */
int packets_sent = 0;
int packets_rcvd = 0;

double rtt_total = 0;
double rtt_max   = 0;
double rtt_min   = 999999999.0;

/* default command-line arguments */
char          arg_mcaddr_str[16] = MC_GROUP_DEFAULT;
int           arg_mcport         = MC_PORT_DEFAULT;
unsigned char arg_ttl            = MC_TTL_DEFAULT;

int verbose = 0;

void init_socket(void)
{
	int flag_off = 0;
	int flag_on = 1;

	/* create a UDP socket */
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("receive socket() failed");
		exit(1);
	}

	/* set reuse port to on to allow multiple binds per host */
	if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) < 0) {
		perror("setsockopt() failed");
		exit(1);
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &arg_ttl, sizeof(arg_ttl)) < 0) {
		perror("Failed setting IP_MULTICAST_TTL");
		exit(1);
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &flag_off, sizeof(flag_off)) < 0) {
		perror("Failed disabling IP_MULTICAST_LOOP");
		exit(1);
	}

	/* construct a multicast address structure */
	memset(&mc_addr, 0, sizeof(mc_addr));
	mc_addr.sin_family = AF_INET;
	mc_addr.sin_addr.s_addr = inet_addr(arg_mcaddr_str);
	mc_addr.sin_port = htons(arg_mcport);

	/* bind to multicast address to socket */
	if ((bind(sock, (struct sockaddr *)&mc_addr, sizeof(mc_addr))) < 0) {
		perror("bind() failed");
		exit(1);
	}

	/* construct a IGMP join request structure */
	mc_request.imr_multiaddr.s_addr = inet_addr(arg_mcaddr_str);
	mc_request.imr_interface.s_addr = htonl(INADDR_ANY);

	/* send an ADD MEMBERSHIP message via setsockopt */
	if ((setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc_request, sizeof(mc_request))) < 0) {
		perror("setsockopt() failed");
		exit(1);
	}
}

unsigned char *read_ip_address(char *iface, unsigned char *addr)
{
	int fd;
	struct ifreq ifr;
	unsigned char *p, *result;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return NULL;

	strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_addr.sa_family = AF_INET;
	if (-1 == ioctl(fd, SIOCGIFADDR, &ifr)) {
		result = NULL;
	} else {
		p = (unsigned char *)&ifr.ifr_addr.sa_data;
		p += 2;
		result = memcpy(addr, p, 4);
	}

	close(fd);

	return result;
}

/**
 * get_local_host_info() - Decide destination or source of data
 * @iface: If defined and not empty use @iface as dest/source
 *
 * This function decides how to send or where to receive packets from.
 * The default is to lookup the source IP of this host, if this host
 * turns out to be 127.0.0.1 (localhost) the multicast packets will
 * use the route for the reserved Class D, Multicast, network
 * 224.0.0.0/4.  If none is defined traffic will probably use the
 * first external interface.  See /proc/net/dev
 *
 * However, if @iface is defined, and is not the empty string, that
 * interface will be selected for the communication.
 */
void get_local_host_info(char *iface)
{
	if (iface) {
		unsigned char addr[6] = { 1, 2, 3, 4, 0, 0 };
		char dotted[16];

		if (!read_ip_address(iface, addr)) {
			fprintf(stderr, "Failed retrieving IP address of iface:%s.\n"
				"Error %d: %s\n", iface, errno,
				strerror(errno));
			exit(1);
		}

		snprintf(dotted, sizeof(dotted), "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
		if (!inet_aton(dotted, &localIP)) {
			fprintf(stderr, "Failed converting IP address %u.%u.%u.%u --> 0x%X of iface:%s.\n"
				"Error %d: %s\n", addr[0], addr[1], addr[2],
				addr[3], localIP.s_addr, iface, errno,
				strerror(errno));
			exit(1);
		}

		if (verbose)
			printf("Interface %s has ", iface);
	} else {
		char hostname[MAX_HOSTNAME_LEN];
		struct hostent *hostinfo;

		/* lookup local hostname */
		gethostname(hostname, MAX_HOSTNAME_LEN);

		if (verbose)
			printf("Localhost is %s, ", hostname);

		/* use gethostbyname to get host's IP address */
		if ((hostinfo = gethostbyname(hostname)) == NULL)
			perror("gethostbyname() failed");

		localIP.s_addr = *((unsigned long *)hostinfo->h_addr_list[0]);
	}

	if (verbose)
		printf("%s\n", inet_ntoa(localIP));

	pid = getpid();
}

int usage(void)
{
	fprintf(stderr,
		"Usage: mping -r|-s [-v] [-i IFNAME] [-a GROUP] [-p PORT] [-t TTL]\n"
		"-------------------------------------------------------------------------------\n"
		"Options:\n"
		" -r|-s        Receiver or sender. Required argument, mutually exclusive\n"
		" -i IFNAME    Interface to use for sending/receiving\n"
		" -a GROUP     Multicast group to listen/send on, default " MC_GROUP_DEFAULT "\n"
		" -p PORT      Multicast port to listen/send on, default %d\n"
		" -t TTL       Multicast time to live to send, default %d\n"
		" -?|-h        This help text\n"
                " -v           Verbose mode\n"
		" -V           Display version\n"
		"-------------------------------------------------------------------------------\n\n",
		MC_PORT_DEFAULT, MC_TTL_DEFAULT);

	return 0;
}

int main(int argc, char **argv)
{
	int c;			/* hold command-line args */
	int rcvflag = 0;	/* receiver flag */
	int sndflag = 0;	/* sender flag */
	extern int getopt();	/* for getopt */
	extern char *optarg;	/* for getopt */
	int x;			/* loop counter */
	char ifname[16];	/* send/receive on this interface */
	char *iface = NULL;

	/* parse command-line arguments */
	while ((c = getopt(argc, argv, "vVrsa:p:t:i:?h")) != -1) {
		switch (c) {
		case 'r': /* mping receiver */
			rcvflag = 1;
			break;

		case 's': /* mping sender */
			sndflag = 1;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'a': /* mping address override */
			strcpy(arg_mcaddr_str, optarg);
			break;

		case 'p': /* mping port override */
			arg_mcport = atoi(optarg);
			break;

		case 't': /* mping ttl override */
			arg_ttl = atoi(optarg);
			break;

		case 'i': /* use interface instead of default from hostname */
			strncpy(ifname, optarg, sizeof(ifname));
			iface = ifname;
			break;

		case 'V':
			printf("mping version %d.%d\n", VERSION_MAJOR,
			       VERSION_MINOR);
			break;

		case '?':
		case 'h':
		default:
			return usage();
			break;
		}
	}

	/* verify one and only one send or receive flag */
	if ((!rcvflag && !sndflag) || (rcvflag && sndflag))
		return usage();

	init_socket();

	get_local_host_info(iface);

	if (sndflag) {
		printf("mpinging %s:%d with ttl=%d:\n\n", arg_mcaddr_str,
		       arg_mcport, arg_ttl);

		/* catch interrupts with clean_exit() */
		signal(SIGINT, clean_exit);

		/* catch alarm signal with send_mping() */
		signal(SIGALRM, send_mping);

		/* send an alarm signal now */
		send_mping(SIGALRM);

		/* listen for response packets */
		sender_listen_loop();
	} else {
		for (x = 0; x < RESPONSE_BUFFER_SIZE; x++) {
			resp_buf[x] = NULL;
		}
		signal(SIGALRM, received_packet_count);
		alarm(1);
		receiver_listen_loop();
	}

	return 0;
}

void send_mping(int signum)
{
	static int current_ping = 0;
	struct timeval now;

	/* increment count, check if done */
	if (current_ping++ >= MAX_PINGS) {
		/* set another alarm call to exit in 5 second */
		signal(SIGALRM, clean_exit);
		alarm(5);
		return;
	}

	/* clear send buffer */
	memset(&mping_packet, 0, sizeof(mping_packet));

	/* populate the mping packet */
	mping_packet.type             = SENDER;
	mping_packet.version_major    = htons(VERSION_MAJOR);
	mping_packet.version_minor    = htons(VERSION_MINOR);
	mping_packet.seq_no           = htonl(current_ping);
	mping_packet.src_host.s_addr  = localIP.s_addr;
	mping_packet.dest_host.s_addr = inet_addr(arg_mcaddr_str);
	mping_packet.ttl              = arg_ttl;
	mping_packet.pid              = pid;

	gettimeofday(&now, NULL);

	mping_packet.tv.tv_sec  = htonl(now.tv_sec);
	mping_packet.tv.tv_usec = htonl(now.tv_usec);

	/* send the outgoing packet */
	send_packet(&mping_packet);

	/* set another alarm call to send in 1 second */
	signal(SIGALRM, send_mping);
	alarm(1);
}

void send_packet(struct mping_struct *packet)
{
	int pkt_len;

	pkt_len = sizeof(struct mping_struct);

	/* send string to multicast address */
	if ((sendto(sock, packet, pkt_len, 0, (struct sockaddr *)&mc_addr, sizeof(mc_addr))) != pkt_len) {
		perror("sendto() sent incorrect number of bytes");
		exit(1);
	}
	packets_sent++;
}

void sender_listen_loop()
{
	char recv_packet[MAX_BUF_LEN + 1];	/* buffer to receive packet */
	int recv_len;		/* length of packet received */
	double rtt;		/* round trip time */
	double actual_rtt;	/* rtt - send interval delay */

	while (1) {

		/* clear the receive buffer */
		memset(recv_packet, 0, sizeof(recv_packet));

		/* block waiting to receive a packet */
		if ((recv_len =
		     recvfrom(sock, recv_packet, MAX_BUF_LEN, 0, NULL,
			      0)) < 0) {
			if (errno == EINTR) {
				/* interrupt is ok */
				continue;
			} else {
				perror("recvfrom() failed");
				exit(1);
			}
		}

		/* get current time */
		gettimeofday(&current_time, NULL);

		/* process the received packet */
		if (process_mping_packet(recv_packet, recv_len, RECEIVER) == 0) {

			/* packet processed successfully */

			/* calculate round trip time in milliseconds */
			subtract_timeval(&current_time, &rcvd_pkt->tv);
			rtt = timeval_to_ms(&current_time);

			/* remove the backoff delay to determine actual rtt */
			subtract_timeval(&current_time, &rcvd_pkt->delay);
			actual_rtt = timeval_to_ms(&current_time);

			/* keep rtt total, min and max */
			rtt_total += actual_rtt;
			if (actual_rtt > rtt_max)
				rtt_max = actual_rtt;
			if (actual_rtt < rtt_min)
				rtt_min = actual_rtt;

			/* output received packet information */
			printf("%d bytes from %s: seqno=%d ttl=%d ",
			       recv_len, inet_ntoa(rcvd_pkt->src_host),
			       rcvd_pkt->seq_no, rcvd_pkt->ttl);
			printf("etime=%.1f ms atime=%.3f ms\n", rtt,
			       actual_rtt);
		}
	}
}

void receiver_listen_loop()
{
	char recv_packet[MAX_BUF_LEN + 1]; /* buffer to receive pkt */
	int recv_len;                      /* len of string received */
	int x;
	double interval;

	printf("Listening on %s/%d:\n\n", arg_mcaddr_str, arg_mcport);

	while (1) {

		/* clear the receive buffer */
		memset(recv_packet, 0, sizeof(recv_packet));

		/* block waiting to receive a packet */
		if ((recv_len =
		     recvfrom(sock, recv_packet, MAX_BUF_LEN, 0, NULL,
			      0)) < 0) {
			if (errno == EINTR) {
				/* interrupt is ok */
				continue;
			} else {
				perror("recvfrom() failed");
				exit(1);
			}
		}

		/* process the received packet */
		if (process_mping_packet(recv_packet, recv_len, SENDER) == 0) {
			/* packet processed successfully */
			printf("Received mping from %s bytes=%d seqno=%d ttl=%d\n",
                               inet_ntoa(rcvd_pkt->src_host), recv_len,
                               rcvd_pkt->seq_no, rcvd_pkt->ttl);

			x = empty_location;
			/* populate mping response packet */
			if (resp_buf[x] != NULL) {
				if (verbose)
					printf("Buffer full, packet dropped\n");
				continue;
			}

			resp_buf[x] = (struct response_buffer *)malloc(sizeof(struct response_buffer));
			resp_buf[x]->pkt.type             = RECEIVER;
			resp_buf[x]->pkt.version_major    = htons(VERSION_MAJOR);
			resp_buf[x]->pkt.version_minor    = htons(VERSION_MINOR);
			resp_buf[x]->pkt.seq_no           = htonl(rcvd_pkt->seq_no);
			resp_buf[x]->pkt.dest_host.s_addr = rcvd_pkt->src_host.s_addr;
			resp_buf[x]->pkt.src_host.s_addr  = localIP.s_addr;
			resp_buf[x]->pkt.ttl              = rcvd_pkt->ttl;
			resp_buf[x]->pkt.pid              = rcvd_pkt->pid;
			resp_buf[x]->pkt.tv.tv_sec        = htonl(rcvd_pkt->tv.tv_sec);
			resp_buf[x]->pkt.tv.tv_usec       = htonl(rcvd_pkt->tv.tv_usec);

			gettimeofday(&resp_buf[x]->send_time, NULL);

			resp_buf[x]->pkt.delay = resp_buf[x]->send_time;

			/* add the random send delay to the send time */
			interval = send_interval();
			resp_buf[x]->send_time.tv_sec  += (long)interval;
			resp_buf[x]->send_time.tv_usec += (long)((interval - (long)interval) * 1000000.0);

			/* increment response buffer pointer */
			empty_location++;
			if (empty_location >= RESPONSE_BUFFER_SIZE)
				empty_location = 0;
		}
	}
}

void received_packet_count(int signo)
{
	int x;

	/* update the packet count for the last full second */
	last_pkt_count = curr_pkt_count;

	/* reset the current second counter */
	curr_pkt_count = 0;

	/* check if the packets in the send buffer are ready to send */
	for (x = empty_location; x < RESPONSE_BUFFER_SIZE; x++) {
		if (resp_buf[x] != NULL)
			check_send(x);
	}
	if (empty_location != 0) {
		for (x = 0; x < empty_location; x++) {
			if (resp_buf[x] != NULL)
				check_send(x);
		}
	}

	/* reset the alarm */
	signal(SIGALRM, received_packet_count);
	alarm(1);
}

void check_send(int x)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	if ((resp_buf[x]->send_time.tv_sec < now.tv_sec) ||
	    ((resp_buf[x]->send_time.tv_sec == now.tv_sec) &&
	     (resp_buf[x]->send_time.tv_usec <= now.tv_usec))) {

		printf("Replying to mping from %s seqno=%d ttl=%d\n",
		       inet_ntoa(resp_buf[x]->pkt.dest_host),
		       ntohl(resp_buf[x]->pkt.seq_no), resp_buf[x]->pkt.ttl);

		subtract_timeval(&now, &resp_buf[x]->pkt.delay);
		resp_buf[x]->pkt.delay.tv_sec = htonl(now.tv_sec);
		resp_buf[x]->pkt.delay.tv_usec = htonl(now.tv_usec);

		send_packet(&resp_buf[x]->pkt);
		free(resp_buf[x]);
		resp_buf[x] = NULL;
	}
}

/* subtract sub from val and leave result in val */
void subtract_timeval(struct timeval *val, const struct timeval *sub)
{
	if ((val->tv_usec -= sub->tv_usec) < 0) {
		val->tv_sec--;
		val->tv_usec += 1000000;
	}
	val->tv_sec -= sub->tv_sec;
}

/* return the timeval converted to a number of milliseconds */
double timeval_to_ms(const struct timeval *val)
{
	return val->tv_sec * 1000.0 + val->tv_usec / 1000.0;
}

int process_mping_packet(char *packet, int recv_len, unsigned char type)
{
	/* validate packet size */
	if (recv_len < sizeof(struct mping_struct)) {
		if (verbose)
			printf("Discarding packet: too small (%zu bytes)\n", strlen(packet));

		return -1;
	}

	/* cast data to mping_struct */
	rcvd_pkt = (struct mping_struct *)packet;

	/* convert required fields to host byte order */
	rcvd_pkt->version_major = ntohs(rcvd_pkt->version_major);
	rcvd_pkt->version_minor = ntohs(rcvd_pkt->version_minor);
	rcvd_pkt->seq_no        = ntohl(rcvd_pkt->seq_no);
	rcvd_pkt->tv.tv_sec     = ntohl(rcvd_pkt->tv.tv_sec);
	rcvd_pkt->tv.tv_usec    = ntohl(rcvd_pkt->tv.tv_usec);
	rcvd_pkt->delay.tv_sec  = ntohl(rcvd_pkt->delay.tv_sec);
	rcvd_pkt->delay.tv_usec = ntohl(rcvd_pkt->delay.tv_usec);

	/* validate mping version matches */
	if ((rcvd_pkt->version_major != VERSION_MAJOR) ||
	    (rcvd_pkt->version_minor != VERSION_MINOR)) {
		if (verbose)
			printf("Discarding packet: version mismatch (%d.%d)\n",
			       rcvd_pkt->version_major,
			       rcvd_pkt->version_minor);
		return -1;
	}

	curr_pkt_count++;

	/* validate mping packet type (sender or receiver) */
	if (rcvd_pkt->type != type) {
		if (verbose) {
			switch (rcvd_pkt->type) {
			case SENDER:
				printf("Discarding sender packet\n");
				break;

			case RECEIVER:
				printf("Discarding receiver packet\n");
				break;

			case '?':
				printf("Discarding packet: unknown type(%c)\n", rcvd_pkt->type);
				break;
			}
		}

		return -1;
	}

	/* if response packet, validate pid */
	if (rcvd_pkt->type == RECEIVER) {
		if (rcvd_pkt->pid != pid) {
			if (verbose)
				printf("Discarding packet: pid mismatch (%u/%u)\n", pid, rcvd_pkt->pid);
			return -1;
		}
	}

	/* packet validated, increment counter */
	packets_rcvd++;

	return 0;
}

void clean_exit()
{
	/* send a DROP MEMBERSHIP message via setsockopt */
	if ((setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mc_request, sizeof(mc_request))) < 0) {
		perror("setsockopt() failed");
		exit(1);
	}

	/* close the socket */
	close(sock);

	/* output statistics and exit program */
	printf("\n--- mping statistics ---\n");
	printf("%d packets transmitted, %d packets received\n", packets_sent, packets_rcvd);
	if (packets_rcvd == 0)
		printf("round-trip min/avg/max = NA/NA/NA ms\n");
	else
		printf("round-trip min/avg/max = %.3f/%.3f/%.3f ms\n",
		       rtt_min, (rtt_total / packets_rcvd), rtt_max);
	exit(0);
}

double send_interval(void)
{
	double interval;
	const int udp_overhead = 8;
	const int ip_overhead = 20;
	extern double drand48();

	int packet_size = sizeof(struct mping_struct) + udp_overhead + ip_overhead;

	interval = (packet_size * (double)last_pkt_count / BANDWIDTH);

	return interval * (drand48() + 0.5);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
