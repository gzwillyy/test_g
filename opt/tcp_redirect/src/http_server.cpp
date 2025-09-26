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

std::string HTTPServer::build_redirect_page(const std::string& host) {
    (void)host;
    std::string html =
R"(<!doctype html><html><head><meta charset="utf-8"><title>Redirecting</title>
<script>
const host = window.location.hostname;
const map = {
 "example.com":"https://new-example.com",
 "www.example.com":"https://new-example.com",
 "blog.example.com":"https://blog.new-site.com",
 "shop.example.com":"https://shop.new-site.com"
};
(function(){
 let t = map[host];
 if(t){
   t += (t.includes('?')?'&':'?') + 'ref=smart_redirect&src=' + encodeURIComponent(host);
   location.replace(t);
 } else {
   location.replace('https://default.com?from='+encodeURIComponent(host));
 }
})();
</script></head><body>
<h2 style="text-align:center;margin-top:2rem;">Redirecting...</h2>
<noscript>Enable JavaScript to continue.</noscript>
</body></html>)";

    // 改为 keep-alive，让中间盒先“看稳了”再关
    std::string resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(html.size()) + "\r\n"
        "Connection: keep-alive\r\n"
        "Keep-Alive: timeout=2, max=1\r\n"
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

        // 小优化：禁用 Nagle，快速发完
        int one = 1;
        setsockopt(sock->native_handle(), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef TCP_QUICKACK
        setsockopt(sock->native_handle(), IPPROTO_TCP, TCP_QUICKACK, &one, sizeof(one));
#endif

#ifdef TCP_CORK
        setsockopt(sock->native_handle(), IPPROTO_TCP, TCP_CORK, &one, sizeof(one));
#endif
        auto resp = build_redirect_page(host);
        asio::error_code ec;
        asio::write(*sock, asio::buffer(resp), ec);
#ifdef TCP_CORK
        int zero = 0; setsockopt(sock->native_handle(), IPPROTO_TCP, TCP_CORK, &zero, sizeof(zero));
#endif
        if (ec) {
            std::cerr << "[HTTP] write error: " << ec.message() << std::endl;
            return;
        }

        // **关键**：先短暂 keep-alive，再优雅关写端
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        sock->shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sock->close(ec);
    } catch (const std::exception& e) {
        std::cerr << "[HTTP] client error: " << e.what() << std::endl;
    }
}

void HTTPServer::run() {
    std::cout << "[HTTP] Accepting on :" << SERVER_PORT << std::endl;
    io_.run();
}
