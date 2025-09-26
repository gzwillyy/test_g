#ifndef TCP_WINDOW_CONTROLLER_H
#define TCP_WINDOW_CONTROLLER_H

#include "common.h"

class TCPWindowController {
public:
    TCPWindowController();
    ~TCPWindowController();

    void start();
    void stop();

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

    // NFQUEUE worker + callback
    void worker_thread(int queue_num, int idx);
    static int packet_handler(struct nfq_q_handle *qh, struct nfgenmsg *, struct nfq_data *nfa, void *data);

    // 16-bit 反码和 / TCP 校验和（含伪首部）
    static uint16_t ip_checksum16(const uint8_t* data, size_t len);
    static uint16_t tcp_checksum(struct iphdr* iph, struct tcphdr* tcph, const uint8_t* payload, int payload_len);

    // 改写 Window Scale 选项（SYN/ACK 中）
    static void rewrite_wscale_option(struct tcphdr* tcph);

    // 改窗口并重算校验和
    static void apply_window_tamper(struct iphdr* iph, struct tcphdr* tcph, uint16_t new_win);

    // 读取配置
    static uint16_t get_target_win();     // TCP_TAMPER_WINDOW，默认 1
    static bool     tamper_on_synack();   // TCP_TAMPER_ON_SYNACK，默认 true

    void setup_iptables_rules();
    void cleanup_iptables_rules();
};

#endif
