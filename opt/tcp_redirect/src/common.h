#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <ctime>
#include <errno.h>

// 网络
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

// NFQUEUE
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/netfilter.h>

// 常量与配置
static const int NFQUEUE_NUM[] = {1000, 1001, 1002};
static const int NFQUEUE_COUNT = 3;
static const int SERVER_PORT   = 80;

// 设为 0 等于“零窗口”（强背压/阻断）。建议实验从小非零开始（如 4096）
static const uint16_t TARGET_WINDOW_SIZE = 4096;

// 简单日志（带时间戳与线程ID）
enum class LogLevel { TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, FATAL=5 };

inline LogLevel current_log_level() {
    const char* env = std::getenv("TCP_REDIRECT_LOG_LEVEL");
    if (!env) return LogLevel::DEBUG; // 默认 DEBUG
    std::string s(env ? env : "");
    if (s=="TRACE") return LogLevel::TRACE;
    if (s=="DEBUG") return LogLevel::DEBUG;
    if (s=="INFO")  return LogLevel::INFO;
    if (s=="WARN")  return LogLevel::WARN;
    if (s=="ERROR") return LogLevel::ERROR;
    if (s=="FATAL") return LogLevel::FATAL;
    return LogLevel::DEBUG;
}

inline const char* lvl_name(LogLevel l) {
    switch(l){
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "UNK";
}

inline std::string now_ts() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    auto t  = system_clock::to_time_t(tp);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::tm tm{}; localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%F %T") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

inline void log_print(LogLevel lvl, const std::string& msg) {
    static LogLevel g = current_log_level();
    if ((int)lvl < (int)g) return;
    std::ostringstream oss;
    oss << "[" << now_ts() << "] [" << lvl_name(lvl) << "] [tid:" << std::this_thread::get_id() << "] " << msg << "\n";
    if (lvl >= LogLevel::ERROR) std::cerr << oss.str();
    else std::cout << oss.str();
}

#define LOGT(msg) log_print(LogLevel::TRACE, msg)
#define LOGD(msg) log_print(LogLevel::DEBUG, msg)
#define LOGI(msg) log_print(LogLevel::INFO,  msg)
#define LOGW(msg) log_print(LogLevel::WARN,  msg)
#define LOGE(msg) log_print(LogLevel::ERROR, msg)
#define LOGF(msg) log_print(LogLevel::FATAL, msg)

#endif
