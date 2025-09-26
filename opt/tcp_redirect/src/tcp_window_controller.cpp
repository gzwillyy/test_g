#include "common.h"

class TCPWindowController {
public:
    TCPWindowController() = default;
    ~TCPWindowController(){ stop(); }

    void start() {
        LOGI("Starting TCPWindowController...");
        // ❌ 不要对含 atomic 的结构体做赋值：queues_stats_[i] = {};
        // 让其保持默认构造的零值即可。
        for (int i = 0; i < NFQUEUE_COUNT; ++i) {
            workers_[i] = std::thread(&TCPWindowController::worker_thread, this, NFQUEUE_NUM[i], i);
        }
        setup_iptables_rules();
        LOGI("TCPWindowController started.");
    }

    void stop() {
        if (!running_) return;
        LOGI("Stopping TCPWindowController...");
        running_ = false;
        for (int i = 0; i < NFQUEUE_COUNT; ++i) {
            if (workers_[i].joinable()) workers_[i].join();
        }
        cleanup_iptables_rules();
        LOGI("Controller stopped.");
    }

private:
    struct QueueStats {
        std::atomic<uint64_t> packets{0};
        std::atomic<uint64_t> modified{0};
        std::atomic<uint64_t> errors{0};
        std::atomic<uint64_t> non_tcp{0};
    };

    std::atomic<bool> running_{true};
    std::thread workers_[NFQUEUE_COUNT];
    QueueStats queues_stats_[NFQUEUE_COUNT];

    static unsigned short compute_tcp_checksum(struct iphdr* iph, struct tcphdr* tcph,
                                        unsigned char* payload, int payload_len) {
        unsigned long sum = 0;

        struct pseudo_header {
            uint32_t src_addr;
            uint32_t dest_addr;
            uint8_t  placeholder;
            uint8_t  protocol;
            uint16_t tcp_length;
        } psh;

        const int ip_header_len  = iph->ihl * 4;
        const int tcp_header_len = tcph->doff * 4;
        const int total_len      = ntohs(iph->tot_len);
        const int tcp_len        = std::max(0, total_len - ip_header_len);

        psh.src_addr   = iph->saddr;
        psh.dest_addr  = iph->daddr;
        psh.placeholder= 0;
        psh.protocol   = IPPROTO_TCP;
        psh.tcp_length = htons((uint16_t)tcp_len);

        // 伪首部
        const uint16_t* p = reinterpret_cast<uint16_t*>(&psh);
        for (size_t i = 0; i < sizeof(psh)/2; ++i) sum += ntohs(p[i]);

        // TCP 头
        tcph->check = 0;
        const unsigned char* tcp_ptr = reinterpret_cast<unsigned char*>(tcph);
        for (int i = 0; i < tcp_header_len; i += 2) {
            uint16_t word = (tcp_ptr[i] << 8) + (i + 1 < tcp_header_len ? tcp_ptr[i + 1] : 0);
            sum += word;
        }

        // 载荷
        if (payload && payload_len > 0) {
            for (int i = 0; i < payload_len; i += 2) {
                uint16_t word = (payload[i] << 8) + (i + 1 < payload_len ? payload[i + 1] : 0);
                sum += word;
            }
        }

        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        return htons((uint16_t)(~sum));
    }

    static int packet_handler(struct nfq_q_handle *qh, struct nfgenmsg *,
                              struct nfq_data *nfa, void *data) {
        auto* ctx = static_cast<std::pair<TCPWindowController*,int>*>(data);
        auto* self = ctx->first;
        const int idx = ctx->second;

        int id = 0;
        if (auto* ph = nfq_get_msg_packet_hdr(nfa)) id = ntohl(ph->packet_id);

        unsigned char *packet_data = nullptr;
        const int len = nfq_get_payload(nfa, &packet_data);
        if (len < (int)sizeof(struct iphdr)) {
            self->queues_stats_[idx].errors++;
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
        }

        self->queues_stats_[idx].packets++;

        auto* iph = reinterpret_cast<struct iphdr*>(packet_data);
        if (iph->protocol != IPPROTO_TCP) {
            self->queues_stats_[idx].non_tcp++;
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
        }

        const int ip_header_len = iph->ihl * 4;
        if (len < ip_header_len + (int)sizeof(struct tcphdr)) {
            self->queues_stats_[idx].errors++;
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
        }

        auto* tcph = reinterpret_cast<struct tcphdr*>(packet_data + ip_header_len);

        // 仅处理本机 80 源端口（响应方向）
        if (ntohs(tcph->source) == SERVER_PORT) {
            const bool is_synack = tcph->syn && tcph->ack;
            const bool is_ack    = tcph->ack && !tcph->syn;
            const bool is_pshack = tcph->psh && tcph->ack;

            if (is_synack || is_ack || is_pshack) {
                const uint16_t old_window = ntohs(tcph->window);
                // payload 计算
                const int tcp_header_len = tcph->doff * 4;
                const int total_len      = ntohs(iph->tot_len);

                unsigned char* payload = nullptr;
                int payload_len = 0;
                if (total_len > ip_header_len + tcp_header_len) {
                    payload     = reinterpret_cast<unsigned char*>(iph) + ip_header_len + tcp_header_len;
                    payload_len = total_len - ip_header_len - tcp_header_len;
                }

                tcph->window = htons(TARGET_WINDOW_SIZE);
                tcph->check  = compute_tcp_checksum(iph, tcph, payload, payload_len);

                self->queues_stats_[idx].modified++;

                std::ostringstream oss;
                oss << "[Q" << NFQUEUE_NUM[idx] << "] "
                    << "Modify win " << old_window << " -> " << TARGET_WINDOW_SIZE
                    << " flags["
                    << (tcph->syn ? "SYN " : "")
                    << (tcph->ack ? "ACK " : "")
                    << (tcph->psh ? "PSH " : "")
                    << (tcph->fin ? "FIN " : "")
                    << (tcph->rst ? "RST " : "")
                    << "] s=" << inet_ntoa(*(in_addr*)&iph->saddr)
                    << ":" << ntohs(tcph->source)
                    << " d=" << inet_ntoa(*(in_addr*)&iph->daddr)
                    << ":" << ntohs(tcph->dest);
                LOGD(oss.str());

                return nfq_set_verdict(qh, id, NF_ACCEPT, len, packet_data);
            }
        }
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
    }

    void worker_thread(int queue_num, int idx) {
        std::ostringstream oss; oss << "NFQUEUE worker start for queue " << queue_num << " (idx " << idx << ")";
        LOGI(oss.str());

        struct nfq_handle *h = nfq_open();
        if (!h) { LOGE("nfq_open failed"); return; }

        if (nfq_unbind_pf(h, AF_INET) < 0) LOGW("nfq_unbind_pf (ignore if none)");

        if (nfq_bind_pf(h, AF_INET) < 0) {
            LOGE("nfq_bind_pf failed");
            nfq_close(h);
            return;
        }

        // 为 packet_handler 附带 idx
        auto* ctx = new std::pair<TCPWindowController*,int>(this, idx);
        struct nfq_q_handle *qh = nfq_create_queue(h, queue_num, &packet_handler, ctx);
        if (!qh) {
            LOGE("nfq_create_queue failed");
            nfq_close(h);
            delete ctx;
            return;
        }

        if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xFFFF) < 0) {
            LOGE("nfq_set_mode failed");
            nfq_destroy_queue(qh);
            nfq_close(h);
            delete ctx;
            return;
        }

        int fd = nfq_fd(h);
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        auto last_report = std::chrono::steady_clock::now();
        char buffer[65536];

        while (running_) {
            fd_set rset; FD_ZERO(&rset); FD_SET(fd, &rset);
            struct timeval tv{1,0};
            const int rv = select(fd + 1, &rset, nullptr, nullptr, &tv);
            if (rv > 0 && FD_ISSET(fd, &rset)) {
                int n = recv(fd, buffer, sizeof(buffer), 0);
                if (n > 0) nfq_handle_packet(h, buffer, n);
            } else if (rv < 0 && errno != EINTR) {
                LOGE("select error on NFQUEUE fd");
                break;
            }

            // 周期性队列统计
            auto now = std::chrono::steady_clock::now();
            if (now - last_report > std::chrono::seconds(5)) {
                last_report = now;
                std::ostringstream s;
                s << "[Q" << queue_num << "] Stats: packets=" << queues_stats_[idx].packets
                  << " modified=" << queues_stats_[idx].modified
                  << " non_tcp="  << queues_stats_[idx].non_tcp
                  << " errors="   << queues_stats_[idx].errors;
                LOGI(s.str());
            }
        }

        nfq_destroy_queue(qh);
        nfq_close(h);
        delete ctx;
        LOGI("NFQUEUE worker exit for queue " + std::to_string(queue_num));
    }

    void setup_iptables_rules() {
        LOGI("Setting iptables NFQUEUE rules...");
        int r1 = system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null");
        int r2 = system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null");
        int r3 = system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null");
        std::ostringstream oss;
        oss << "iptables set results: SYN-ACK="<< WEXITSTATUS(r1)
            << " ACK="<< WEXITSTATUS(r2)
            << " PSH-ACK="<< WEXITSTATUS(r3);
        LOGI(oss.str());
    }

    void cleanup_iptables_rules() {
        LOGI("Cleaning iptables NFQUEUE rules...");
        int r1 = system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null");
        int r2 = system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null");
        int r3 = system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null");
        std::ostringstream oss;
        oss << "iptables del results: SYN-ACK="<< WEXITSTATUS(r1)
            << " ACK="<< WEXITSTATUS(r2)
            << " PSH-ACK="<< WEXITSTATUS(r3);
        LOGI(oss.str());
    }
};
