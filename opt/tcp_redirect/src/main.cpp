// /opt/tcp_redirect/src/main.cpp
#include "common.h"
#include <memory>

std::unique_ptr<TCPWindowController> g_controller;

void signal_handler(int signal) {
    std::cout << "[INFO] Received signal " << signal << ", shutting down..." << std::endl;
    if (g_controller) {
        g_controller->stop();
    }
    exit(0);
}

int main() {
    std::cout << "[INFO] Starting TCP Redirect Server..." << std::endl;
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // 检查权限
        if (getuid() != 0) {
            std::cerr << "[ERROR] This program must be run as root!" << std::endl;
            return 1;
        }
        
        // 启动TCP窗口控制器
        g_controller = std::make_unique<TCPWindowController>();
        std::thread controller_thread([&]() {
            g_controller->start();
        });
        
        // 等待NFQUEUE设置完成
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 启动HTTP服务器
        HTTPServer server(SERVER_PORT);
        
        std::cout << "[INFO] TCP Redirect Server started successfully!" << std::endl;
        std::cout << "[INFO] Press Ctrl+C to stop the server." << std::endl;
        
        server.run();
        
        controller_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}