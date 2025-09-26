#ifndef TCP_WINDOW_CONTROLLER_H
#define TCP_WINDOW_CONTROLLER_H

#include "common.h"
#include <unordered_map>
#include <chrono>

// 简单日志宏（如果你已有日志宏，保留你原来的）
#ifndef LOGD
#define LOGD(x) std::cout << "[" << now_time() << "] [DEBUG] " << x << std::endl
#endif
#ifndef LOGI
#define LOGI(x) std::cout << "[" << now_time() << "] [INFO]  " << x << std::endl
#endif
#ifndef LOGW
#define LOGW(x) std::cout << "[" << now_time() << "] [WARN]  " << x << std::endl
#endif
#ifndef LOGE
#define LOGE(x) std::cerr << "[" << now_time() << "] [ERROR] " << x << std::endl
#endif
#ifndef LOGF
#define LOGF(x) std::cerr << "[" << now_time() << "] [FATAL] " << x << std::endl
#endif

inline std::string now_time() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    std::time_t t = system_clock::to_time_t(tp);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%F %T", std::localtime(&t));
    return buf;
}

class TCPWindowController {
public:
    TCPWindowController();
    ~TCPWindowController();

    void start();
    void stop();

private:
    // ==== 运行时参数 ====
    static uint16_t get_target_win();   // 目标窗口（收紧窗口）
    static bool     tamper_on_synack(); // 是否在 SYN+ACK 阶段改窗（建议 0 以减少公网 RST）
    static uint32_t get_warmup_bytes(); // 预热阈值：客户端请求累计 ACK 到达多少字节后切换到 target_win
    static uint16_t get_warmup_win();   // 预热阶段的较大窗口（如 4096）
    static uint32_t get_conn_idle_sec();// 会话状态过期时间（秒）
    static size_t   get_state_cap();    // 状态表容量上限，超出时做简单 GC

    // ==== NFQUEUE ====
    static int packet_handler(struct nfq_q_handle *qh, struct nfgenmsg*, struct nfq_data *nfa, void *data);
    void worker_thread(int queue_num, int idx);
    void setup_iptables_rules();
    void cleanup_iptables_rules();

    // ==== 校验和工具 ====
    static uint16_t ip_checksum16(const uint8_t* data, size_t len);
    static uint16_t tcp_checksum(struct iphdr* iph, struct tcphdr* tcph, const uint8_t* payload, int payload_len);
    static void rewrite_wscale_option(struct tcphdr* tcph);
    static void apply_window_tamper(struct iphdr* iph, struct tcphdr* tcph, uint16_t new_win);

    // ==== 自适应状态（按连接跟踪） ====
    struct ConnKey {
        uint32_t saddr; // 服务器地址（网络序）
        uint32_t daddr; // 客户端地址（网络序）
        uint16_t sport; // 服务器端口（网络序）
        uint16_t dport; // 客户端端口（网络序）
        bool operator==(const ConnKey& o) const {
            return saddr==o.saddr && daddr==o.daddr && sport==o.sport && dport==o.dport;
        }
    };
    struct ConnKeyHash {
        std::size_t operator()(const ConnKey& k) const noexcept {
            // 简单混合
            uint64_t a = (static_cast<uint64_t>(k.saddr) << 32) | k.daddr;
            uint32_t b = (static_cast<uint32_t>(k.sport) << 16) | k.dport;
            std::hash<uint64_t> h64; std::hash<uint32_t> h32;
            return h64(a) ^ (h32(b) + 0x9e3779b97f4a7c15ULL + (h64(a)<<6) + (h64(a)>>2));
        }
    };
    struct ConnState {
        uint32_t base_ack = 0;    // 第一次看到的 ack 序号（对端请求基线）
        bool     base_set = false;
        uint64_t acked    = 0;    // 已累计 ACK 的字节数（客户端请求已被我们 ACK 的大小）
        bool     warmed   = false;
        std::chrono::steady_clock::time_point last_seen = std::chrono::steady_clock::now();
    };

    uint16_t pick_window_for_packet(struct iphdr* iph, struct tcphdr* tcph, bool is_synack, bool is_ack_or_psh);

    void gc_states_if_needed();

private:
    std::atomic<bool> running_{true};
    static constexpr int NFQUEUE_COUNT = 3;
    std::thread workers_[NFQUEUE_COUNT];

    struct QueueStats {
        std::atomic<unsigned long> packets{0};
        std::atomic<unsigned long> modified{0};
        std::atomic<unsigned long> non_tcp{0};
        std::atomic<unsigned long> errors{0};
    } queues_stats_[NFQUEUE_COUNT];

    // 状态表
    std::unordered_map<ConnKey, ConnState, ConnKeyHash> states_;
    std::mutex states_mu_;
};

#endif // TCP_WINDOW_CONTROLLER_H
