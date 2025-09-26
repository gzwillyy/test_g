#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "common.h"
#include <asio.hpp>
#include <memory>
#include <string>

class HTTPServer {
public:
    explicit HTTPServer(short port);
    void run();

private:
    asio::io_context io_;
    asio::ip::tcp::acceptor acc_;

    void start_accept();
    void handle_client(std::shared_ptr<asio::ip::tcp::socket> sock);

    // 保持一致：声明与实现都是 build_redirect_page
    std::string build_redirect_page(const std::string& host);
};

#endif // HTTP_SERVER_H
