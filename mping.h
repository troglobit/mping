/* Simple multicast ping program with congestion control
 *
 * Taken from the book "Multicast Sockets - Practical Guide for Programmers"
 * written by David Makofske and Kevin Almeroth.  For online information see
 * http://www.nmsl.cs.ucsb.edu/MulticastSocketsBook/
 */
#ifndef __MPING2_H__
#define __MPING2_H__

#include <sys/types.h>           /* for type definitions */
#include <sys/socket.h>          /* for socket API calls */
#include <netinet/in.h>          /* for address structs */
#include <arpa/inet.h>           /* for sockaddr_in */
#include <unistd.h>              /* for symbolic constants */
#include <errno.h>               /* for system error messages */
#include <sys/time.h>            /* for timeval and gettimeofday() */
#include <netdb.h>               /* for hostname calls */
#include <signal.h>              /* for signal calls */
#include <stdlib.h>              /* for drand48() */

#define MAX_BUF_LEN      1024    /* size of receive buffer */
#define MAX_HOSTNAME_LEN  256    /* size of host name buffer */
#define MAX_PINGS           5    /* number of pings to send */

#define VERSION_MAJOR 1          /* mping version major */
#define VERSION_MINOR 2          /* mping version minor */

#define SENDER   's'             /* mping sender identifier */
#define RECEIVER 'r'             /* mping receiver identifier */

/* mping packet structure */
struct mping_struct
{
   unsigned short version_major;
   unsigned short version_minor;
   unsigned char type;
   unsigned char ttl;
   struct in_addr src_host;
   struct in_addr dest_host;
   unsigned int seq_no;
   pid_t pid;
   struct timeval tv;
   struct timeval delay;
} mping_packet;

/* packet buffer for mping responses */
struct response_buffer
{
   struct mping_struct pkt;
   struct timeval send_time;
};

#define RESPONSE_BUFFER_SIZE 100

/* function prototypes */
void send_mping ();
void send_packet (struct mping_struct *packet);
void sender_listen_loop ();
void receiver_listen_loop ();
void subtract_timeval (struct timeval *val, const struct timeval *sub);
double timeval_to_ms (const struct timeval *val);
int process_mping_packet (char *packet, int recv_len, unsigned char type);
void clean_exit ();
void output_results ();
void received_packet_count ();
void check_send (int);
double send_interval ();

#endif /* __MPING2_H__ */
