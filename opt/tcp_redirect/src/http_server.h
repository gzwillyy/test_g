#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "common.h"
#include <boost/asio.hpp>

class HTTPServer {
public:
    explicit HTTPServer(short port);
    void run();

private:
    boost::asio::io_context io_;
    boost::asio::ip::tcp::acceptor acceptor_;

    void start_accept();
    void handle_client(std::shared_ptr<boost::asio::ip::tcp::socket> sock);
    std::string build_redirect_page(const std::string& host);
};

#endif
