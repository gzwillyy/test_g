// /opt/tcp_redirect/src/tcp_window_controller.cpp
#include "common.h"

class TCPWindowController {
private:
    std::atomic<bool> running_{true};
    std::thread workers_[3];
    
public:
    ~TCPWindowController() {
        stop();
    }

    // 完整的TCP校验和计算
    unsigned short compute_tcp_checksum(struct iphdr* iph, struct tcphdr* tcph, unsigned char* payload, int payload_len) {
        unsigned long sum = 0;
        unsigned char* tcp_ptr = (unsigned char*)tcph;

        struct pseudo_header {
            unsigned int src_addr;
            unsigned int dest_addr;
            unsigned char placeholder;
            unsigned char protocol;
            unsigned short tcp_length;
        } psh;

        psh.src_addr = iph->saddr;
        psh.dest_addr = iph->daddr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(ntohs(iph->tot_len) - iph->ihl * 4);

        unsigned char pseudo_buf[12];
        memcpy(pseudo_buf, &psh, sizeof(psh));

        for (int i = 0; i < 12; i += 2) {
            unsigned short word = (pseudo_buf[i] << 8) + pseudo_buf[i + 1];
            sum += word;
        }

        tcph->check = 0;
        for (int i = 0; i < tcph->doff * 4; i += 2) {
            if (i + 1 < tcph->doff * 4) {
                unsigned short word = (tcp_ptr[i] << 8) + tcp_ptr[i + 1];
                sum += word;
            } else {
                sum += (tcp_ptr[i] << 8);
            }
        }

        if (payload && payload_len > 0) {
            for (int i = 0; i < payload_len; i += 2) {
                if (i + 1 < payload_len) {
                    unsigned short word = (payload[i] << 8) + payload[i + 1];
                    sum += word;
                } else {
                    sum += (payload[i] << 8);
                }
            }
        }

        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        return (unsigned short)(~sum);
    }

    void modify_tcp_window(struct iphdr* iph, struct tcphdr* tcph, uint16_t new_window) {
        uint16_t old_window = ntohs(tcph->window);
        tcph->window = htons(new_window);
        
        unsigned char* payload = nullptr;
        int payload_len = 0;
        
        int ip_header_len = iph->ihl * 4;
        int tcp_header_len = tcph->doff * 4;
        int total_len = ntohs(iph->tot_len);
        
        if (total_len > ip_header_len + tcp_header_len) {
            payload = (unsigned char*)iph + ip_header_len + tcp_header_len;
            payload_len = total_len - ip_header_len - tcp_header_len;
        }
        
        tcph->check = compute_tcp_checksum(iph, tcph, payload, payload_len);
        
        std::cout << "[DEBUG] Modified TCP window from " << old_window << " to " << new_window 
                  << " for packet with flags: 0x" << std::hex << (int)tcph->flags 
                  << std::dec << std::endl;
    }

    static int packet_handler(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
                             struct nfq_data *nfa, void *data) {
        TCPWindowController* controller = static_cast<TCPWindowController*>(data);
        int id = 0;
        struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
        
        if (ph) {
            id = ntohl(ph->packet_id);
        }
        
        unsigned char *packet_data;
        int len = nfq_get_payload(nfa, &packet_data);
        
        if (len < (int)sizeof(struct iphdr)) {
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
        }
        
        struct iphdr *iph = (struct iphdr*)packet_data;
        
        if (len < ntohs(iph->tot_len)) {
            std::cerr << "[ERROR] Packet truncated: expected " << ntohs(iph->tot_len) 
                      << " bytes, got " << len << " bytes" << std::endl;
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
        }
        
        if (iph->protocol == IPPROTO_TCP) {
            int ip_header_len = iph->ihl * 4;
            
            if (len < ip_header_len + (int)sizeof(struct tcphdr)) {
                return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
            }
            
            struct tcphdr *tcph = (struct tcphdr*)(packet_data + ip_header_len);
            
            if (ntohs(tcph->source) == SERVER_PORT) {
                uint16_t new_window = TARGET_WINDOW_SIZE;
                
                if ((tcph->syn && tcph->ack) || (tcph->ack && !tcph->syn) || (tcph->psh && tcph->ack)) {
                    controller->modify_tcp_window(iph, tcph, new_window);
                    return nfq_set_verdict(qh, id, NF_ACCEPT, len, packet_data);
                }
            }
        }
        
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
    }

    void worker_thread(int queue_num) {
        std::cout << "[INFO] Starting NFQUEUE worker for queue " << queue_num << std::endl;
        
        struct nfq_handle *h = nfq_open();
        if (!h) {
            std::cerr << "[ERROR] Error opening NFQUEUE handle for queue " << queue_num << std::endl;
            return;
        }
        
        if (nfq_unbind_pf(h, AF_INET) < 0) {
            std::cerr << "[WARN] Error unbinding existing handler for queue " << queue_num << std::endl;
        }
        
        if (nfq_bind_pf(h, AF_INET) < 0) {
            std::cerr << "[ERROR] Error binding to AF_INET for queue " << queue_num << std::endl;
            nfq_close(h);
            return;
        }
        
        struct nfq_q_handle *qh = nfq_create_queue(h, queue_num, &packet_handler, this);
        if (!qh) {
            std::cerr << "[ERROR] Error creating queue " << queue_num << std::endl;
            nfq_close(h);
            return;
        }
        
        if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xFFFF) < 0) {
            std::cerr << "[ERROR] Error setting packet copy mode for queue " << queue_num << std::endl;
            nfq_destroy_queue(qh);
            nfq_close(h);
            return;
        }
        
        nfq_set_queue_maxlen(qh, 1024);
        
        int fd = nfq_fd(h);
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        
        std::cout << "[INFO] NFQUEUE worker " << queue_num << " started successfully" << std::endl;
        
        char buffer[65536];
        struct timeval timeout;
        fd_set read_set;
        
        while (running_) {
            FD_ZERO(&read_set);
            FD_SET(fd, &read_set);
            
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int rv = select(fd + 1, &read_set, NULL, NULL, &timeout);
            
            if (rv > 0 && FD_ISSET(fd, &read_set)) {
                int recv_len = recv(fd, buffer, sizeof(buffer), 0);
                if (recv_len > 0) {
                    nfq_handle_packet(h, buffer, recv_len);
                } else if (recv_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "[ERROR] Error receiving packet in queue " << queue_num << ": " 
                              << strerror(errno) << std::endl;
                    break;
                }
            } else if (rv < 0) {
                if (errno != EINTR) {
                    std::cerr << "[ERROR] Select error in queue " << queue_num << ": " 
                              << strerror(errno) << std::endl;
                    break;
                }
            }
        }
        
        std::cout << "[INFO] NFQUEUE worker " << queue_num << " shutting down" << std::endl;
        nfq_destroy_queue(qh);
        nfq_close(h);
    }

    void start() {
        std::cout << "[INFO] Starting TCP Window Controller with 3 NFQUEUE workers..." << std::endl;
        
        for (int i = 0; i < 3; ++i) {
            workers_[i] = std::thread(&TCPWindowController::worker_thread, this, NFQUEUE_NUM[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        setup_iptables_rules();
        std::cout << "[INFO] TCP Window Controller started successfully" << std::endl;
    }

    void stop() {
        std::cout << "[INFO] Stopping TCP Window Controller..." << std::endl;
        running_ = false;
        
        for (int i = 0; i < 3; ++i) {
            if (workers_[i].joinable()) {
                workers_[i].join();
                std::cout << "[INFO] Worker " << i << " stopped" << std::endl;
            }
        }
        
        cleanup_iptables_rules();
        std::cout << "[INFO] TCP Window Controller stopped" << std::endl;
    }

private:
    void setup_iptables_rules() {
        std::cout << "[INFO] Setting up iptables rules..." << std::endl;
        
        // 清理可能冲突的规则
        system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null");
        system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null");
        system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null");
        
        // 设置精确规则
        system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000");
        system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001");
        system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002");
        
        std::cout << "[INFO] Iptables rules set up successfully" << std::endl;
    }

    void cleanup_iptables_rules() {
        std::cout << "[INFO] Cleaning up iptables rules..." << std::endl;
        
        system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null");
        system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null");
        system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null");
        
        std::cout << "[INFO] Iptables rules cleaned up" << std::endl;
    }
};