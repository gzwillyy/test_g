#include "tcp_window_controller.h"

TCPWindowController::TCPWindowController() = default;
TCPWindowController::~TCPWindowController(){ stop(); }

uint16_t TCPWindowController::get_target_win() {
    const char* e = std::getenv("TCP_TAMPER_WINDOW");
    long v = (e ? std::strtol(e, nullptr, 10) : 1);
    if (v < 0) v = 0; if (v > 65535) v = 65535;
    return (uint16_t)v;
}
bool TCPWindowController::tamper_on_synack() {
    const char* e = std::getenv("TCP_TAMPER_ON_SYNACK");
    if (!e) return true; // 默认启用
    std::string s(e);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return !(s=="0"||s=="false"||s=="no");
}

void TCPWindowController::start() {
    LOGI("Starting TCPWindowController...");
    for (int i = 0; i < NFQUEUE_COUNT; ++i) {
        workers_[i] = std::thread(&TCPWindowController::worker_thread, this, NFQUEUE_NUM[i], i);
    }
    setup_iptables_rules();
    LOGI("TCPWindowController started.");
}

void TCPWindowController::stop() {
    if (!running_) return;
    LOGI("Stopping TCPWindowController...");
    running_ = false;
    for (int i = 0; i < NFQUEUE_COUNT; ++i) {
        if (workers_[i].joinable()) workers_[i].join();
    }
    cleanup_iptables_rules();
    LOGI("Controller stopped.");
}

// ========== 16-bit 反码和 ==========
uint16_t TCPWindowController::ip_checksum16(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    const uint16_t* p = reinterpret_cast<const uint16_t*>(data);
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(reinterpret_cast<const uint8_t*>(p));
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return uint16_t(~sum);
}

// ========== TCP 校验和（伪首部 + 头 + 选项 + 载荷） ==========
uint16_t TCPWindowController::tcp_checksum(struct iphdr* iph, struct tcphdr* tcph, const uint8_t* payload, int payload_len) {
    struct Pseudo {
        uint32_t saddr;
        uint32_t daddr;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t len;
    } __attribute__((packed)) psh;

    const int ip_hl = iph->ihl * 4;
    const int total_len = ntohs(iph->tot_len);
    const int tcp_len = total_len - ip_hl;

    psh.saddr = iph->saddr;
    psh.daddr = iph->daddr;
    psh.zero  = 0;
    psh.proto = IPPROTO_TCP;
    psh.len   = htons((uint16_t)tcp_len);

    uint32_t sum = 0;

    // 伪首部
    const uint8_t* p1 = reinterpret_cast<const uint8_t*>(&psh);
    for (size_t i=0;i<sizeof(Pseudo);i+=2) {
        uint16_t w = (p1[i] << 8) + (i+1 < sizeof(Pseudo) ? p1[i+1] : 0);
        sum += w;
    }

    // TCP 头 + 选项 + 载荷
    const uint8_t* tcp_bytes = reinterpret_cast<const uint8_t*>(tcph);
    int remain = tcp_len;
    while (remain > 1) {
        sum += (tcp_bytes[0] << 8) + tcp_bytes[1];
        tcp_bytes += 2; remain -= 2;
    }
    if (remain) sum += (*tcp_bytes) << 8;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return htons(uint16_t(~sum));
}

// ========== 改写 Window Scale（SYN/ACK） ==========
void TCPWindowController::rewrite_wscale_option(struct tcphdr* tcph) {
    const int hdr_len = tcph->doff * 4;
    const int bare = sizeof(struct tcphdr);
    if (hdr_len <= bare) return;

    uint8_t* opt = reinterpret_cast<uint8_t*>(tcph) + bare;
    int opt_len = hdr_len - bare;

    for (int i=0; i<opt_len; ) {
        uint8_t kind = opt[i];
        if (kind == 0) break;        // EOL
        if (kind == 1) { ++i; continue; } // NOP
        if (i+1 >= opt_len) break;
        uint8_t len = opt[i+1];
        if (len < 2 || i + len > opt_len) break;

        // Window Scale: kind=3, len=3
        if (kind == 3 && len == 3) {
            opt[i+2] = 0; // WS=0
            LOGD("[WSCALE] Rewrote Window Scale to 0 on SYN-ACK");
            return;
        }
        i += len;
    }
}

void TCPWindowController::apply_window_tamper(struct iphdr* iph, struct tcphdr* tcph, uint16_t new_win) {
    const int ip_hl  = iph->ihl * 4;
    const int tcp_hl = tcph->doff * 4;
    const int tot    = ntohs(iph->tot_len);

    uint8_t* payload = nullptr;
    int payload_len = 0;
    if (tot > ip_hl + tcp_hl) {
        payload     = reinterpret_cast<uint8_t*>(iph) + ip_hl + tcp_hl;
        payload_len = tot - ip_hl - tcp_hl;
    }

    uint16_t old = ntohs(tcph->window);
    tcph->window = htons(new_win);
    tcph->check  = 0;
    tcph->check  = tcp_checksum(iph, tcph, payload, payload_len);

    std::ostringstream oss;
    oss << "Modify win " << old << " -> " << new_win
        << " flags[" << (tcph->syn ? "SYN " : "") << (tcph->ack ? "ACK " : "") << (tcph->psh ? "PSH " : "")
        << (tcph->fin ? "FIN " : "") << (tcph->rst ? "RST " : "") << "]";
    LOGD(oss.str());
}

int TCPWindowController::packet_handler(struct nfq_q_handle *qh, struct nfgenmsg *,
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

    const int ip_hl = iph->ihl * 4;
    if (len < ip_hl + (int)sizeof(struct tcphdr)) {
        self->queues_stats_[idx].errors++;
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
    }

    auto* tcph = reinterpret_cast<struct tcphdr*>(packet_data + ip_hl);

    // 仅处理“本机 80 端口发出的响应方向”
    if (ntohs(tcph->source) != SERVER_PORT) {
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
    }

    const bool is_syn    = tcph->syn;
    const bool is_ack    = tcph->ack;
    const bool is_psh    = tcph->psh;
    const bool is_synack = is_syn && is_ack;
    const bool is_ack_or_psh = (!is_syn && (is_ack || (is_psh && is_ack)));

    const uint16_t new_win = get_target_win();

    if (is_synack && tamper_on_synack()) {
        // 握手阶段：先把 WSCALE=0，再压窗口
        rewrite_wscale_option(tcph);
        apply_window_tamper(iph, tcph, new_win);
        self->queues_stats_[idx].modified++;
        return nfq_set_verdict(qh, id, NF_ACCEPT, len, packet_data);
    }

    if (is_ack_or_psh) {
        // 已建立会话：持续压窗口
        apply_window_tamper(iph, tcph, new_win);
        self->queues_stats_[idx].modified++;
        return nfq_set_verdict(qh, id, NF_ACCEPT, len, packet_data);
    }

    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
}

void TCPWindowController::worker_thread(int queue_num, int idx) {
    LOGI("NFQUEUE worker start for queue " + std::to_string(queue_num) + " (idx " + std::to_string(idx) + ")");

    struct nfq_handle *h = nullptr;
    for (int attempt=1; attempt<=5 && !h; ++attempt) {
        h = nfq_open();
        if (!h) {
            LOGE(std::string("nfq_open failed (attempt ") + std::to_string(attempt) +
                 "): errno=" + std::to_string(errno) + " (" + strerror(errno) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(200 * attempt));
        }
    }
    if (!h) { LOGF("nfq_open failed after retries"); return; }

    if (nfq_unbind_pf(h, AF_INET) < 0) LOGW("nfq_unbind_pf (ignore if none)");
    if (nfq_bind_pf(h, AF_INET) < 0) { LOGE("nfq_bind_pf failed"); nfq_close(h); return; }

    auto* ctx = new std::pair<TCPWindowController*,int>(this, idx);
    struct nfq_q_handle *qh = nfq_create_queue(h, queue_num, &packet_handler, ctx);
    if (!qh) { LOGE("nfq_create_queue failed"); nfq_close(h); delete ctx; return; }

    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xFFFF) < 0) {
        LOGE("nfq_set_mode failed"); nfq_destroy_queue(qh); nfq_close(h); delete ctx; return;
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

void TCPWindowController::setup_iptables_rules() {
    LOGI("Setting iptables NFQUEUE rules...");
    // 与 Geneva 建议一致：三条（SYN+ACK / ACK / PSH+ACK）
    int r1 = system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null");
    int r2 = system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null");
    int r3 = system("iptables -I OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null");
    std::ostringstream oss;
    oss << "iptables set results: SA="<< WEXITSTATUS(r1)
        << " A="<< WEXITSTATUS(r2)
        << " PA="<< WEXITSTATUS(r3);
    LOGI(oss.str());
}

void TCPWindowController::cleanup_iptables_rules() {
    LOGI("Cleaning iptables NFQUEUE rules...");
    int r1 = system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null");
    int r2 = system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null");
    int r3 = system("iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null");
    std::ostringstream oss;
    oss << "iptables del results: SA="<< WEXITSTATUS(r1)
        << " A="<< WEXITSTATUS(r2)
        << " PA="<< WEXITSTATUS(r3);
    LOGI(oss.str());
}
