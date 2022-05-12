/*
 * Copyright (c) 2021-2022  Joachim Wiberg <troglobit@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE		/* For TIMESPEC_TO_TIMEVAL() in GLIBC */
#endif
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef VERSION
#define VERSION          "2.0"
#endif

#define dbg(fmt,args...) do { if (debug) printf(fmt "\n", ##args); } while (0)
#define sig(s,c)    do { struct sigaction a = {.sa_handler=c};sigaction(s,&a,0); } while(0)

#define MC_GROUP_DEFAULT "225.1.2.3"
#define MC_GROUP_INET6   "ff2e::42"
#define MC_PORT_DEFAULT  4321
#define MC_TTL_DEFAULT   1

#define RESPONSES_MAX    100

#define MAX_BUF_LEN      1024
#define MAX_HOSTNAME_LEN 256

#define SENDER           's'
#define RECEIVER         'r'

#define INET_ADDRSTR_LEN 64
typedef struct sockaddr_storage inet_addr_t;

struct mping {
	char            version[4];

	unsigned char   type;
	unsigned char   ttl;

	inet_addr_t     src_host;
	inet_addr_t     dest_host;

	unsigned int    seq_no;
	pid_t           pid;

	struct timeval  tv;
} mping;

/*#define BANDWIDTH 10000.0 */          /* bw in bytes/sec for mping */
#define BANDWIDTH 100.0                 /* bw in bytes/sec for mping */

/* pointer to mping packet buffer */
struct mping *rcvd_pkt;

int   sd;                               /* socket descriptor */
pid_t pid;                              /* our process id */

inet_addr_t         myaddr;
inet_addr_t         mcaddr;
int                 ipproto;
struct group_req    gr;

struct timeval      start;              /* start time for sender */

/* Cleared by signal handler */
volatile sig_atomic_t running = 1;

#ifdef AF_INET6
#define OPTSTR     "6"
#else
#define OPTSTR     ""
#endif

/* counters and statistics variables */
int packets_sent = 0;
int packets_rcvd = 0;

double rtt_total = 0;
double rtt_max   = 0;
double rtt_min   = 999999999.0;

/* default command-line arguments */
char          arg_mcaddr[INET_ADDRSTR_LEN] = MC_GROUP_DEFAULT;
int           arg_mcport     = MC_PORT_DEFAULT;
int           arg_count      = -1;
int           arg_timeout    = 5;
int           arg_deadline   = 0;
unsigned char arg_ttl        = MC_TTL_DEFAULT;

int debug = 0;
int quiet = 0;

static void init_socket(int family, int ifindex)
{
	int off = 0;
	int on = 1;

	/* create a UDP socket */
	if ((sd = socket(family, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err(1, "failed creating UDP socket");

	/* set reuse port to on to allow multiple binds per host */
	if ((setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
		err(1, "Failed enabling SO_REUSEADDR");

#ifdef AF_INET6
	if (family == AF_INET6) {
		int hops = arg_ttl;

		ipproto = IPPROTO_IPV6;

		if (setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)))
			err(1, "Failed setting IPV6_MULTICAST_HOPS: %s", strerror(errno));

		if (setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)))
			err(1, "Failed setting IPV6_MULTICAST_IF: %s", strerror(errno));
	} else
#endif
	{
		struct ip_mreqn imr = { .imr_ifindex = ifindex };

		ipproto = IPPROTO_IP;

		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &arg_ttl, sizeof(arg_ttl)))
			err(1, "Failed setting IP_MULTICAST_TTL");

		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &off, sizeof(off)) < 0)
			err(1, "Failed disabling IP_MULTICAST_LOOP");

		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &imr, sizeof(imr)))
			err(1, "Failed setting IP_MULTICAST_IF %d", ifindex);
	}

	/* bind to multicast address to socket */
	if ((bind(sd, (struct sockaddr *)&mcaddr, sizeof(mcaddr))) < 0)
		err(1, "bind() failed");

	/* construct a IGMP/MLD join request structure */
	memset(&gr, 0, sizeof(gr));
	gr.gr_group     = mcaddr;
	gr.gr_interface = ifindex;
	if ((setsockopt(sd, ipproto, MCAST_JOIN_GROUP, &gr, sizeof(gr))) < 0)
		err(1, "failed joining group %s on ifindex %d", arg_mcaddr, ifindex);
}

static size_t strlencpy(char *dst, const char *src, size_t len)
{
	const char *p = src;
	size_t num = len;

	while (num > 0) {
		num--;
		if ((*dst++ = *src++) == 0)
			break;
	}

	if (num == 0 && len > 0)
		*dst = 0;

	return src - p - 1;
}

const char *inet_address(inet_addr_t *ss, char *buf, size_t len)
{
	static char fallback[INET_ADDRSTR_LEN];
	struct sockaddr_in *sin;

	if (!buf) {
		buf = fallback;
		len = sizeof(fallback);
	}

#ifdef AF_INET6
	if (ss->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
		return inet_ntop(AF_INET6, &sin6->sin6_addr, buf, len);
	}
#endif

	sin = (struct sockaddr_in *)ss;
	return inet_ntop(AF_INET, &sin->sin_addr, buf, len);
}

static int address_inet(const char *address, inet_addr_t *ina)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)ina;
	void *ptr = &sin->sin_addr;

#ifdef AF_INET6
	if (strchr(address, ':')) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ina;
		void *ptr6 = &sin6->sin6_addr;

		if (inet_pton(AF_INET6, arg_mcaddr, ptr6)) {
			mcaddr.ss_family = AF_INET6;
			sin6->sin6_port = htons(arg_mcport);
			return 0;
		}

		return 1;	/* 225.1.2.3:4321 unsupported atm. */
	}
#endif

	if (inet_pton(AF_INET, arg_mcaddr, ptr)) {
		mcaddr.ss_family = AF_INET;
		sin->sin_port = htons(arg_mcport);
		return 0;
	}

	return -1;
}

/*
 * Find first interface that is not loopback, but is up, has link, and
 * multicast capable.
 */
static char *ifany(char *iface, size_t len)
{
	struct ifaddrs *ifaddr, *ifa;

	if (getifaddrs(&ifaddr) == -1)
		return NULL;

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		int ifindex;

		if (!(ifa->ifa_flags & IFF_UP))
			continue;

		if (!(ifa->ifa_flags & IFF_RUNNING))
			continue;

		if (!(ifa->ifa_flags & IFF_MULTICAST))
			continue;

		ifindex = if_nametoindex(ifa->ifa_name);
                dbg("Found iface %s, ifindex %d", ifa->ifa_name, ifindex);
		strlencpy(iface, ifa->ifa_name, len);
		iface[len] = 0;
		break;
	}
	freeifaddrs(ifaddr);

	return iface;
}

/* The BSD's or SVR4 systems like Solaris don't have /proc/net/route */
static char *altdefault(char *iface, size_t len)
{
	char buf[256];
	FILE *fp;

	fp = popen("netstat -rn", "r");
	if (!fp)
		return NULL;

	while (fgets(buf, sizeof(buf), fp)) {
		char *token;

		if (strncmp(buf, "default", 7) && strncmp(buf, "0.0.0.0", 7))
			continue;

		token = strtok(buf, " \t\n");
		while (token) {
			if (if_nametoindex(token)) {
				strlencpy(iface, token, len);
				pclose(fp);

				return iface;
			}

			token = strtok(NULL, " \t\n");
		}
	}

	pclose(fp);
	return NULL;
}

/* Find default outbound *LAN* interface, i.e. skipping tunnels */
char *ifdefault(char *iface, size_t len)
{
	uint32_t dest, gw, mask;
	char buf[256], ifname[17];
	char *ptr;
	FILE *fp;
	int rc, flags, cnt, use, metric, mtu, win, irtt;
	int best = 100000, found = 0;

	fp = fopen("/proc/net/route", "r");
	if (!fp) {
		ptr = altdefault(iface, len);
		if (!ptr)
			goto fallback;

		return ptr;
	}

	/* Skip heading */
	ptr = fgets(buf, sizeof(buf), fp);
	if (!ptr)
		goto end;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		rc = sscanf(buf, "%16s %X %X %X %d %d %d %X %d %d %d\n",
			   ifname, &dest, &gw, &flags, &cnt, &use, &metric,
			   &mask, &mtu, &win, &irtt);

		if (rc < 10 || !(flags & 1)) /* IFF_UP */
			continue;

		if (dest != 0 || mask != 0)
			continue;

		if (!iface[0] || !strncmp(iface, "tun", 3)) {
			if (metric >= best)
				continue;

			strlencpy(iface, ifname, len);
			iface[len] = 0;
			best = metric;
			found = 1;

                        dbg("Found default inteface %s", iface);
		}
	}

end:
	fclose(fp);
	if (found)
		return iface;
fallback:
	return ifany(iface, len);
}

/* Find IP address of default outbound LAN interface */
int ifinfo(char *iface, inet_addr_t *addr, int family)
{
	char buf[INET_ADDRSTR_LEN] = { 0 };
	struct ifaddrs *ifaddr, *ifa;
	char ifname[16] = { 0 };
	int found = 0;
	int rc = -1;

	if (!iface || !iface[0])
		iface = ifdefault(ifname, sizeof(ifname));
	if (!iface || !iface[0])
		errx(1, "no suitable default interface available");

	rc = getifaddrs(&ifaddr);
	if (rc == -1)
		err(1, "failed querying available interfaces");

	rc = -1; /* Return -1 if iface with family is not found */
	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (strcmp(iface, ifa->ifa_name))
			continue;

		found = 1;
		if (!(ifa->ifa_flags & IFF_UP)) {
			dbg("%s is not UP, skipping ...", iface);
			continue;
		}

		if (!ifa->ifa_addr) {
			dbg("%s has no address, skipping ...", iface);
			continue;
		}

		if (family == AF_UNSPEC) {
			if (ifa->ifa_addr->sa_family != AF_INET &&
			    ifa->ifa_addr->sa_family != AF_INET6)
				dbg("%s no IPv4 or IPv6 address, skipping ...", iface);
				continue;
		} else if (ifa->ifa_addr->sa_family != family) {
			dbg("%s not matching address family, skipping ...", iface);
			continue;
		}

		if (!(ifa->ifa_flags & IFF_MULTICAST)) {
			warnx("%s is not multicast capable, check interface flags.", iface);
			break;
		}

		dbg("Found %s addr %s", ifa->ifa_name, inet_address((inet_addr_t *)ifa->ifa_addr, buf, sizeof(buf)));
		*addr = *(inet_addr_t *)ifa->ifa_addr;
		rc = if_nametoindex(ifa->ifa_name);
                dbg("iface %s, ifindex %d, addr %s", ifa->ifa_name, rc, buf);
		break;
	}
	freeifaddrs(ifaddr);

	if (!found)
		warnx("no such interface %s.", iface);
	if (rc == -1)
		warnx("no %s address on on interface %s.", family == AF_INET ? "ipv4" : "ipv6", iface);

	return rc;
}

/* subtract sub from val and leave result in val */
void subtract_timeval(struct timeval *val, const struct timeval *sub)
{
	val->tv_sec  -= sub->tv_sec;
	val->tv_usec -= sub->tv_usec;
	if (val->tv_usec < 0) {
		val->tv_sec--;
		val->tv_usec += 1000000;
	}
}

/* return the timeval converted to a number of milliseconds */
double timeval_to_ms(const struct timeval *val)
{
	return val->tv_sec * 1000.0 + val->tv_usec / 1000.0;
}

static int cleanup(void)
{
	if ((setsockopt(sd, ipproto, MCAST_LEAVE_GROUP, &gr, sizeof(gr))) < 0)
		err(1, "setsockopt() failed");

	close(sd);

	printf("\n--- %s mping statistics ---\n", arg_mcaddr);
	printf("%d packets transmitted, %d packets received\n", packets_sent, packets_rcvd);
	if (packets_rcvd == 0)
		printf("round-trip min/avg/max = NA/NA/NA ms\n");
	else
		printf("round-trip min/avg/max = %.3f/%.3f/%.3f ms\n",
		       rtt_min, (rtt_total / packets_rcvd), rtt_max);

        if (arg_count > 0 && arg_count > packets_rcvd)
                return 1;

	return 0;
}

static void clean_exit(int signo)
{
	(void)signo;
	running = 0;
}

void send_packet(struct mping *packet)
{
	int pkt_len = sizeof(struct mping);

	if ((sendto(sd, packet, pkt_len, 0, (struct sockaddr *)&mcaddr, sizeof(mcaddr))) != pkt_len)
		err(1, "sendto() sent incorrect number of bytes");

        packets_sent++;
}

void send_mping(int signo)
{
	static int seqno = 0;
	struct timespec now;

        (void)signo;
	clock_gettime(CLOCK_MONOTONIC, &now);

        /*
         * Tracks number of sent mpings.  If deadline mode is enabled we
         * ignore this exit and wait for arg_count number of replies or
         * deadline timeout.
         */
        if (arg_deadline) {
		struct timeval tv;

		TIMESPEC_TO_TIMEVAL(&tv, &now);
                subtract_timeval(&tv, &start);

		if ((arg_count > 0 && packets_rcvd >= arg_count) ||
		    (tv.tv_sec >= arg_deadline)) {
			running = 0;
			return;
		}
	} else if (arg_count > 0 && seqno >= arg_count) {
		sig(SIGALRM, clean_exit);
		alarm(arg_timeout);
		return;
	}

	TIMESPEC_TO_TIMEVAL(&mping.tv, &now);
        strlencpy(mping.version, VERSION, sizeof(mping.version));
	mping.type       = SENDER;
	mping.ttl        = arg_ttl;
	mping.src_host   = myaddr;
	mping.dest_host  = mcaddr;
	mping.seq_no     = htonl(seqno);
	mping.pid        = pid;
	mping.tv.tv_sec  = htonl(mping.tv.tv_sec);
	mping.tv.tv_usec = htonl(mping.tv.tv_usec);

	send_packet(&mping);
	seqno++;

	/* set another alarm call to send in 1 second */
	sig(SIGALRM, send_mping);
	alarm(1);
}

int process_mping(char *packet, int len, unsigned char type)
{
	if (len < (int)sizeof(struct mping)) {
		dbg("Discarding packet: too small (%zu bytes)", strlen(packet));
		return -1;
	}

	rcvd_pkt = (struct mping *)packet;
	rcvd_pkt->seq_no        = ntohl(rcvd_pkt->seq_no);
	rcvd_pkt->tv.tv_sec     = ntohl(rcvd_pkt->tv.tv_sec);
	rcvd_pkt->tv.tv_usec    = ntohl(rcvd_pkt->tv.tv_usec);

	if (strcmp(rcvd_pkt->version, VERSION)) {
		dbg("Discarding packet: version mismatch (%s)", rcvd_pkt->version);
		return -1;
	}

	if (rcvd_pkt->type != type) {
		if (debug) {
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

	if (rcvd_pkt->type == RECEIVER) {
		if (rcvd_pkt->pid != pid) {
			dbg("Discarding packet: pid mismatch (%u/%u)", pid, rcvd_pkt->pid);
			return -1;
		}
	}

	packets_rcvd++;

	return 0;
}

void sender_listen_loop(void)
{
	send_mping(0);

	while (running) {
                char recv_packet[MAX_BUF_LEN + 1] = { 0 };
                int len;

		if ((len = recvfrom(sd, recv_packet, MAX_BUF_LEN, 0, NULL, 0)) < 0) {
			if (errno == EINTR)
				continue; /* interrupt is ok */
                        err(1, "recvfrom() failed");
		}

		if (process_mping(recv_packet, len, RECEIVER) == 0) {
			struct timespec now;
                        struct timeval tv;
                        double rtt;		/* round trip time */

                        clock_gettime(CLOCK_MONOTONIC, &now);
			TIMESPEC_TO_TIMEVAL(&tv, &now);

			/* calculate round trip time in milliseconds */
			subtract_timeval(&tv, &rcvd_pkt->tv);
			rtt = timeval_to_ms(&tv);

			/* keep rtt total, min and max */
			rtt_total += rtt;
			if (rtt > rtt_max)
				rtt_max = rtt;
			if (rtt < rtt_min)
				rtt_min = rtt;

			/* output received packet information */
                        if (!quiet)
                                printf("%d bytes from %s: seqno=%u ttl=%d time=%.1f ms\n",
                                       len, inet_address(&rcvd_pkt->src_host, NULL, 0),
                                       rcvd_pkt->seq_no, rcvd_pkt->ttl, rtt);
		}
	}
}

void receiver_listen_loop(void)
{
	printf("Listening on %s:%d\n", arg_mcaddr, arg_mcport);

	while (running) {
                char recv_packet[MAX_BUF_LEN + 1];
                int len;

		if ((len = recvfrom(sd, recv_packet, MAX_BUF_LEN, 0, NULL, 0)) < 0) {
			if (errno == EINTR)
                                continue; /* interrupt is ok */

                        err(1, "recvfrom() failed");
		}

		if (process_mping(recv_packet, len, SENDER) == 0) {
                        if (!quiet)
                                printf("Received mping from %s bytes=%d seqno=%u ttl=%d\n",
                                       inet_address(&rcvd_pkt->src_host, NULL, 0),
                                       len, rcvd_pkt->seq_no, rcvd_pkt->ttl);

			rcvd_pkt->type       = RECEIVER;
			rcvd_pkt->src_host   = myaddr;
			rcvd_pkt->dest_host  = rcvd_pkt->src_host;
			rcvd_pkt->seq_no     = htonl(rcvd_pkt->seq_no);
			rcvd_pkt->tv.tv_sec  = htonl(rcvd_pkt->tv.tv_sec);
			rcvd_pkt->tv.tv_usec = htonl(rcvd_pkt->tv.tv_usec);

                        /* send reply immediately */
			send_packet(rcvd_pkt);

			if (arg_count > 0 && packets_sent >= arg_count)
				exit(0);
		}
	}
}

int usage(void)
{
	fprintf(stderr,
		"Usage:\n"
                "  mping [-" OPTSTR "dhqrsv] [-c COUNT] [-i IFNAME] [-p PORT] [-t TTL] [-w SEC] [-W SEC] [GROUP]\n"
                "\n"
		"Options:\n"
#ifdef AF_INET6
		"  -6          Use IPv6 instead of IPv4, see below for defaults\n"
#endif
                "  -c COUNT    Stop after sending/receiving COUNT packets\n"
                "  -d          Debug messages\n"
		"  -h          This help text\n"
		"  -i IFNAME   Interface to use for sending/receiving\n"
		"  -p PORT     Multicast port to listen/send to, default %d\n"
                "  -q          Quiet output, only startup and and summary lines\n"
		"  -r          Receiver mode, default\n"
                "  -s          Sender mode\n"
		"  -t TTL      Multicast time to live to send, IPv6 hops, default %d\n"
		"  -v          Show program version and contact information\n"
                "  -w DEADLINE Timeout before exiting, waiting for COUNT replies\n"
                "  -W TIMEOUT  Time to wait for a response, in seconds, default 5\n"
                "\n"
                "Defaults to use multicast group %s, UDP dst port %d, unless -6 in which\n"
		"case a multicast group %s is used.  When a group argument is given, the\n"
		"address family is chosen from that.  The selected outbound interface is chosen\n"
                "by querying the routing table, unless -i IFNAME\n",
                MC_PORT_DEFAULT, MC_TTL_DEFAULT, MC_GROUP_DEFAULT, MC_PORT_DEFAULT,
		MC_GROUP_INET6);

	return 0;
}

int main(int argc, char **argv)
{
	int family = AF_INET;
	char *iface = NULL;
        inet_addr_t addr;
	char ifname[16];
        int mode = 'r';
	int ifindex;
	int c;

	while ((c = getopt(argc, argv, OPTSTR "c:dh?i:p:qrst:vW:w:")) != -1) {
		switch (c) {
#ifdef AF_INET6
		case '6':
			family = AF_INET6;
			break;
#endif

                case 'c':
                        arg_count = atoi(optarg);
                        break;

		case 'd':
			debug = 1;
			break;

		case 'i':
			strlencpy(ifname, optarg, sizeof(ifname));
			iface = ifname;
			break;

		case 'p':
			arg_mcport = atoi(optarg);
			break;

                case 'q':
                        quiet = 1;
                        break;

		case 'r':
                        mode = 'r';
			break;

		case 's':
                        mode = 's';
			break;

		case 't':
			arg_ttl = atoi(optarg);
			break;

		case 'v':
			printf("mping version %s\n"
                               "\n"
                               "Bug report address: https://github.com/troglobit/mping/issues\n"
                               "Project homepage:   https://github.com/troglobit/mping/\n", VERSION);
			return 0;

                case 'w':
                        arg_deadline = atoi(optarg);
                        break;

                case 'W':
                        arg_timeout = atoi(optarg);
                        break;

		case '?':
		case 'h':
		default:
			return usage();
		}
	}

        if (optind < argc)
                strlencpy(arg_mcaddr, argv[optind], sizeof(arg_mcaddr));
#ifdef AF_INET6
	else if (family == AF_INET6)
                strlencpy(arg_mcaddr, MC_GROUP_INET6, sizeof(arg_mcaddr));
#endif

	if (address_inet(arg_mcaddr, &mcaddr))
		errx(1, "invalid multicast group");

	if (mcaddr.ss_family != family)
		family = mcaddr.ss_family;

	pid = getpid();
	ifindex = ifinfo(iface, &addr, family);
	if (ifindex <= 0)
		exit(1);

	init_socket(mcaddr.ss_family, ifindex);
	myaddr = addr;

	if (mode == 's') {
		struct timespec now;

		clock_gettime(CLOCK_MONOTONIC, &now);
		TIMESPEC_TO_TIMEVAL(&start, &now);
		printf("MPING %s:%d (ttl %d)\n", arg_mcaddr, arg_mcport, arg_ttl);

		sig(SIGINT, clean_exit);
		sig(SIGALRM, send_mping);

		sender_listen_loop();
	} else
		receiver_listen_loop();

	return cleanup();
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
