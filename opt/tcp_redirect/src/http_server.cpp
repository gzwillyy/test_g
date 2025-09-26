// /opt/tcp_redirect/src/http_server.cpp
#include "common.h"
#include <asio.hpp>

class HTTPServer {
private:
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::string> redirect_map_;
    
public:
    HTTPServer(short port) : acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
        init_redirect_map();
        start_accept();
    }
    
    void init_redirect_map() {
        redirect_map_ = {
            {"example.com", "https://new-example.com"},
            {"www.example.com", "https://new-example.com"},
            {"blog.example.com", "https://blog.new-site.com"},
            {"shop.example.com", "https://shop.new-site.com"},
            {"test.com", "https://secure-test.com"},
            {"api.example.com", "https://api.new-platform.com"}
        };
    }
    
    void start_accept() {
        auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
        
        acceptor_.async_accept(*socket, [this, socket](std::error_code ec) {
            if (!ec) {
                std::thread([socket, this]() {
                    handle_client(socket);
                }).detach();
            }
            start_accept();
        });
    }
    
    void handle_client(std::shared_ptr<asio::ip::tcp::socket> socket) {
        try {
            asio::streambuf request_buf;
            asio::read_until(*socket, request_buf, "\r\n\r\n");
            
            std::istream request_stream(&request_buf);
            std::string request_line;
            std::getline(request_stream, request_line);
            
            std::string host;
            std::string line;
            while (std::getline(request_stream, line) && line != "\r") {
                if (line.find("Host:") == 0) {
                    host = line.substr(6);
                    host.erase(0, host.find_first_not_of(" \t"));
                    host.erase(host.find_last_not_of(" \r") + 1);
                    break;
                }
            }
            
            std::string response = generate_smart_redirect(host);
            asio::write(*socket, asio::buffer(response));
            
            std::cout << "[HTTP] Handled request for host: " << host << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Client handling error: " << e.what() << std::endl;
        }
    }
    
    std::string generate_smart_redirect(const std::string& host) {
        std::string javascript = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Redirecting...</title>
    <script>
        const host = window.location.hostname;
        const path = window.location.pathname + window.location.search;
        
        const redirectMap = {
            "example.com": "https://new-example.com",
            "www.example.com": "https://new-example.com", 
            "blog.example.com": "https://blog.new-site.com",
            "shop.example.com": "https://shop.new-site.com",
            "test.com": "https://secure-test.com",
            "api.example.com": "https://api.new-platform.com"
        };
        
        const pathPreservation = {
            "blog.example.com": true,
            "shop.example.com": true
        };
        
        function smartRedirect() {
            let target = redirectMap[host];
            
            if (target) {
                if (pathPreservation[host] && path !== '/') {
                    target += path;
                }
                
                const separator = target.includes('?') ? '&' : '?';
                target += separator + 'ref=smart_redirect&src=' + encodeURIComponent(host);
                
                console.log('Redirecting from', host, 'to', target);
                window.location.href = target;
            } else {
                window.location.href = 'https://default-destination.com?from=' + encodeURIComponent(host);
            }
        }
        
        smartRedirect();
        
        setTimeout(() => {
            if (window.location.hostname === host) {
                window.location.href = 'https://fallback-site.com';
            }
        }, 3000);
    </script>
</head>
<body>
    <div style="text-align: center; margin-top: 100px;">
        <h2>正在为您跳转...</h2>
        <p>如果页面没有自动跳转，请 <a href="javascript:smartRedirect()">点击这里</a></p>
        <div id="countdown">3秒后跳转</div>
    </div>
    
    <script>
        let seconds = 3;
        setInterval(() => {
            seconds--;
            if (seconds > 0) {
                document.getElementById('countdown').textContent = seconds + '秒后跳转';
            }
        }, 1000);
    </script>
</body>
</html>
)";

        std::string response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(javascript.length()) + "\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"
            "\r\n" + javascript;
            
        return response;
    }
    
    void run() {
        std::cout << "[HTTP] Server running on port " << SERVER_PORT << std::endl;
        io_context_.run();
    }
};