#include "common.h"
#include <memory>

// 前置声明（与 cpp 在同目录编译即可）
class TCPWindowController;
extern void __linker_hint(); // 空函数占位避免 LTO 误报（可无）

// 直接包含实现或在编译命令里列出 cpp 即可
// 这里我们依赖编译命令包含 tcp_window_controller.cpp / http_server.cpp

// 为了简单，这里再声明 HTTPServer（与源文件同名类）
class HTTPServer {
public:
    explicit HTTPServer(short port);
    void run();
};

static std::unique_ptr<TCPWindowController> g_controller;

void sig_handler(int sig) {
    LOGW(std::string("Signal received: ") + std::to_string(sig) + ", stopping...");
    if (g_controller) g_controller->~TCPWindowController();
    _exit(0);
}

int main() {
    LOGI("TCP Redirect Server booting...");
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    if (getuid() != 0) {
        LOGE("This program must be run as root!");
        return 1;
    }

    try {
        // 允许环境变量控制日志级别：TCP_REDIRECT_LOG_LEVEL=INFO/DEBUG/TRACE/...
        LOGI("LOG LEVEL = " + std::string(lvl_name(current_log_level())));

        g_controller = std::unique_ptr<TCPWindowController>(new TCPWindowController());
        std::thread ctrl([&]{ g_controller->start(); });

        std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待 NFQUEUE 线程起来
        HTTPServer server(SERVER_PORT);

        LOGI("Server started. Press Ctrl+C to stop.");
        server.run();

        ctrl.join();
    } catch (const std::exception& e) {
        LOGF(std::string("Fatal: ") + e.what());
        return 1;
    }
    return 0;
}
