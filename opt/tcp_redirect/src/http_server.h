#pragma once
#include "common.h"
#include <asio.hpp>
#include <memory>
#include <string>

class HTTPServer {
public:
    explicit HTTPServer(short port);
    void run();

private:
    // 仅声明，实现在 .cpp
    void start_accept();
    void handle_client(std::shared_ptr<asio::ip::tcp::socket> sock);
    std::string build_redirect_page(const std::string& host);

private:
    asio::io_context io_;
    asio::ip::tcp::acceptor acc_;
};
