#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/ip.h>

static int running = 1;

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
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_hdr->ip_src, src, sizeof(src));
    inet_ntop(AF_INET, &ip_hdr->ip_dst, dst, sizeof(dst));
    //TCP
    if (ip_hdr->ip_p == IPPROTO_TCP){
        //tcp header
        //try looking only for dns!
        struct tcphdr *tcp_hdr = (struct tcphdr *)(payload + sizeof(struct ether_header) + ip_hdr->ip_hl*4);
        //if (ntohs(tcp_hdr->th_dport) != 53 && ntohs(tcp_hdr->th_sport) != 53){continue;}
        printf("TCP\t%s:%d → %s:%d\n",src, ntohs(tcp_hdr->th_sport), dst, ntohs(tcp_hdr->th_dport));
        unsigned char *tcp_payload = (unsigned char *)tcp_hdr + (4*tcp_hdr->th_off);
        int tcp_payload_len = ntohs(ip_hdr->ip_len) - (ip_hdr->ip_hl * 4) - (tcp_hdr->th_off * 4);
        printf("payload:\n");
        /*
        for (int i = 0; i < tcp_payload_len; i++){
            unsigned char c = tcp_payload[i];
            if (c >= 32 && c <= 126){
                putchar(c);
            }else {
                putchar('.');
            }
        }
        */
        //print_strings(tcp_payload, tcp_payload_len, 5);
        printf("\n\n");
    }
    //UDP
    else if (ip_hdr->ip_p == IPPROTO_UDP) {
        struct udphdr *udp_hdr = (struct udphdr *)(payload + sizeof(struct ether_header) + ip_hdr->ip_hl*4);
        //if (ntohs(udp_hdr->uh_dport) != 53 && ntohs(udp_hdr->uh_sport) != 53){continue;}
        printf("UDP\t%s:%d → %s:%d\n", src, ntohs(udp_hdr->uh_sport), dst, ntohs(udp_hdr->uh_dport));
        unsigned char *udp_payload = (unsigned char *)udp_hdr + sizeof(struct udphdr);
        int udp_payload_len = ntohs(ip_hdr->ip_len) - (ip_hdr->ip_hl * 4) - sizeof(struct udphdr);
        printf("payload:\n");
        /*for (int i = 0; i < udp_payload_len; i++){
            unsigned char c = udp_payload[i];
            if (c >= 32 && c <= 126){
                putchar(c);
            }else {
                putchar('.');
            }
        }*/
        //print_strings(udp_payload, udp_payload_len, 5);
        printf("\n\n");
    }


    // for now, accept all packets
    return nfq_set_verdict(q_handle, id, NF_ACCEPT, 0, NULL);
}

void handle_sigint(int sig) {
    running = 0;
}

int main() {
    struct nfq_handle *handle;       // handle for library (create queue)
    struct nfq_q_handle *queue_handle; // handle for our specific queue
    struct nfq_nfnl_handle *socket_handler;
    int fd;
    int bytes_received;
    char buf[4096];

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

    printf("Listening for packets...\n");
    while (running && (bytes_received = recv(fd, buf, sizeof(buf), 0)) >= 0) {
        nfq_handle_packet(handle, buf, bytes_received);
    }

    printf("Cleaning up...\n");
    nfq_destroy_queue(queue_handle);
    nfq_close(handle);

    return 0;
}
