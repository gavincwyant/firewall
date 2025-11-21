#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include "fire_wall.h"
#include "bucket.h"

#define LOG(format, ...) \
    do { if (verbose) fprintf(stdout, format, ##__VA_ARGS__); } while (0)

static int running = 1;
static ip_table *table;
int total_packets = 0;
int dropped_packets = 0;
int number_of_ips = 0;

static int verbose = 0;

static bucket *global_bucket;

static int handle_packet(struct nfq_q_handle *q_handle, struct nfgenmsg *nf_meta,
                         struct nfq_data *nf_data, void *data) {
    struct nfqnl_msg_packet_hdr *packet_header;
    unsigned char *payload;
    int id = 0;
    int ret = nfq_get_payload(nf_data, &payload);
    packet_header = nfq_get_msg_packet_hdr(nf_data);

    // if we have a header, we want the id
    if (packet_header) {
        id = ntohl(packet_header->packet_id);
    }

    struct ip *ip_hdr = (struct ip *)payload;
    uint32_t source_addr = ip_hdr->ip_src.s_addr;
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_hdr->ip_src, src, sizeof(src));
    inet_ntop(AF_INET, &ip_hdr->ip_dst, dst, sizeof(dst));
    //TCP
    if (ip_hdr->ip_p == IPPROTO_TCP){
        //tcp header
        //try looking only for dns!
        struct tcphdr *tcp_hdr = (struct tcphdr *)(payload + ip_hdr->ip_hl*4);
        //if (ntohs(tcp_hdr->th_dport) != 53 && ntohs(tcp_hdr->th_sport) != 53){continue;}
        LOG("TCP\t%s:%d → %s:%d\n",src, ntohs(tcp_hdr->th_sport), dst, ntohs(tcp_hdr->th_dport));
        unsigned char *tcp_payload = (unsigned char *)tcp_hdr + (4*tcp_hdr->th_off);
        int tcp_payload_len = ntohs(ip_hdr->ip_len) - (ip_hdr->ip_hl * 4) - (tcp_hdr->th_off * 4);
        LOG("payload:\n");
        if (verbose){
        	print_strings(tcp_payload, tcp_payload_len, 5);
        }
        LOG("\n\n");
    }
    //UDP
    else if (ip_hdr->ip_p == IPPROTO_UDP) {
        struct udphdr *udp_hdr = (struct udphdr *)(payload + ip_hdr->ip_hl*4);
        //if (ntohs(udp_hdr->uh_dport) != 53 && ntohs(udp_hdr->uh_sport) != 53){continue;}
        LOG("UDP\t%s:%d → %s:%d\n", src, ntohs(udp_hdr->uh_sport), dst, ntohs(udp_hdr->uh_dport));
        unsigned char *udp_payload = (unsigned char *)udp_hdr + sizeof(struct udphdr);
        int udp_payload_len = ntohs(ip_hdr->ip_len) - (ip_hdr->ip_hl * 4) - sizeof(struct udphdr);
        LOG("payload:\n");
        if (verbose){
        	print_strings(udp_payload, udp_payload_len, 5);
        }
        LOG("\n\n");
    }
    int verdict = NF_ACCEPT;
	bucket *buc = lookup(table, source_addr);
	//if ip does not have a bucket, make one!
	if (!buc){
		buc = bucket_get_or_create(table, source_addr);
		number_of_ips++;
		LOG("NEW IP\n");
	}else{/*printf("we've seen this one already\n");*/}

	int now = time(NULL);
	//seconds since refill (per ip bucket)
	int ssr = now - buc->last_refill;
	if (ssr > 0){ //MUST WAIT AT LEAST ONE SECOND BEFORE TOKEN REFILL
		if (buc->tokens + ssr > MAX_TOKENS){ //AT MOST, MAX TOKENS
			buc->tokens = MAX_TOKENS;
		}else{
			buc->tokens += ssr;
		}
		buc->last_refill = now;
	}

	//update global bucket
	now = time(NULL);
	ssr = now - global_bucket->last_refill;
	if (ssr > 0){
		if (global_bucket->tokens + ssr > GLOBAL_TOKENS){
			global_bucket->tokens = GLOBAL_TOKENS;
		} else {
			global_bucket->tokens += ssr;
		}
		global_bucket->last_refill = now;
	}
	//printf("seconds since refill: %d\n", ssr);
	//printf("bucket has %d tokens\n", buc->tokens);
	if (buc->tokens <= 0 || global_bucket->tokens <= 0) {
		verdict = NF_DROP;
		LOG("DROPPING THIS PACKET - NO TOKENS FOR IP\n\n\n");
		dropped_packets++;
	} else{
		LOG("\n\n");
		buc->tokens--;
		global_bucket->tokens--;
	}
	total_packets++;
    return nfq_set_verdict(q_handle, id, verdict, 0, NULL);
}

void handle_sigint(int sig) {
    running = 0;
}


int main(int argc, char **argv) {
    struct nfq_handle *handle;       // handle for library (create queue)
    struct nfq_q_handle *queue_handle; // handle for our specific queue
    struct nfq_nfnl_handle *socket_handler;
    int fd;
    int bytes_received;
    char buf[4096 * 128];

    for (int i = 1; i < argc; i++) {
	    if (strncmp(argv[i], "-verbose", 8) == 0 || strncmp(argv[i], "-v", 2) == 0) {
    	    verbose = 1;
   		}
    }

    if (verbose){
        printf("\n ~ Verbose mode enabled ~ \n\n");
	}

    printf("Opening library handle\n");
    handle = nfq_open();
    if (!handle) {
        perror("nfq_open");
        exit(1);
    }

    printf("Check for existing nf_queue_handler using AF_INET\n");
    if (nfq_unbind_pf(handle, AF_INET) < 0) {
        // we don't do anything if it didn't exist, just continue!
    }

    printf("Bind queue handler for AF_INET\n");
    if (nfq_bind_pf(handle, AF_INET) < 0) {
        perror("nfq_bind_pf");
        exit(1);
    }

    printf("Create queue\n");
    queue_handle = nfq_create_queue(handle, 0, &handle_packet, NULL);
    if (!queue_handle) {
        perror("nfq_create_queue");
        exit(1);
    }

    printf("Send ALL PACKET DATA to userspace\n");
    if (nfq_set_mode(queue_handle, NFQNL_COPY_PACKET, 0xffff) < 0) {
        perror("nfq_set_mode");
        exit(1);
    }

    fd = nfq_fd(handle);

    signal(SIGINT, handle_sigint); // set running to false on Ctrl+C so we may clean up

    //create ip table
    table = create_table(TABLE_SIZE);
    //create global bucket
    global_bucket = (bucket *)malloc(sizeof(bucket));
    global_bucket->tokens = GLOBAL_TOKENS;
    global_bucket->last_refill = time(NULL);

    printf("Listening for packets...\n");
    while (running && (bytes_received = recv(fd, buf, sizeof(buf), 0)) >= 0) {
        nfq_handle_packet(handle, buf, bytes_received);
    }

    printf("Cleaning up...\n\n");
    nfq_destroy_queue(queue_handle);
    nfq_close(handle);

    print_statistics();

    return 0;
}
