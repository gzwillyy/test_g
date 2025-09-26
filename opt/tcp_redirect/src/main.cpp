#include "common.h"

// 直接把控制器实现并入本编译单元，避免不完整类型问题
#include "tcp_window_controller.cpp"

// HTTP 服务器在另一个编译单元，声明即可（链接时会找到）
class HTTPServer {
public:
    explicit HTTPServer(short port);
    void run();
};

static std::unique_ptr<TCPWindowController> g_controller;

void sig_handler(int sig) {
    LOGW(std::string("Signal received: ") + std::to_string(sig) + ", stopping...");
    if (g_controller) {
        g_controller->stop();   // 正常停止
        g_controller.reset();
    }
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
        LOGI(std::string("LOG LEVEL = ") + lvl_name(current_log_level()));

        g_controller = std::make_unique<TCPWindowController>();
        std::thread ctrl([&]{ g_controller->start(); });

        std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待 NFQUEUE 线程起来
        HTTPServer server(SERVER_PORT);

        LOGI("Server started. Press Ctrl+C to stop.");
        server.run();

        if (ctrl.joinable()) ctrl.join();
    } catch (const std::exception& e) {
        LOGF(std::string("Fatal: ") + e.what());
        return 1;
    }
    return 0;
}
