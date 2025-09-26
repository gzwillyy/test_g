#include "http_server.h"

HTTPServer::HTTPServer(short port)
    : acceptor_(io_, {boost::asio::ip::tcp::v4(), static_cast<unsigned short>(port)}) {
    LOGI("[HTTP] Accepting on :80");
    start_accept();
}

void HTTPServer::run() {
    LOGI("[HTTP] io_context.run()");
    io_.run();
}

void HTTPServer::start_accept() {
    auto sock = std::make_shared<boost::asio::ip::tcp::socket>(io_);
    acceptor_.async_accept(*sock, [this, sock](const boost::system::error_code& ec){
        if (!ec) {
            try {
                auto ep = sock->remote_endpoint();
                LOGD("[HTTP] New connection from " + ep.address().to_string() + ":" + std::to_string(ep.port()));
            } catch (...) {
                LOGD("[HTTP] New connection (endpoint unknown)");
            }
            std::thread([sock,this]{ handle_client(sock); }).detach();
        } else {
            LOGW(std::string("[HTTP] accept error: ") + ec.message());
        }
        start_accept();
    });
}

std::string HTTPServer::build_redirect_page(const std::string& host) {
    (void)host;
    const std::string body = R"(<!doctype html><html><head><meta charset="utf-8"><title>Redirecting</title>
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
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: text/html; charset=utf-8\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n\r\n" << body;
    return resp.str();
}

void HTTPServer::handle_client(std::shared_ptr<boost::asio::ip::tcp::socket> sock) {
    try {
        boost::asio::streambuf req;
        boost::asio::read_until(*sock, req, "\r\n\r\n");
        std::istream is(&req);
        std::string request_line; std::getline(is, request_line);

        std::string host, line;
        while (std::getline(is, line) && line != "\r") {
            if (line.rfind("Host:", 0) == 0) {
                host = line.substr(5);
                while (!host.empty() && (host.front()==' '||host.front()=='\t')) host.erase(host.begin());
                while (!host.empty() && (host.back()=='\r'||host.back()=='\n'||host.back()==' ')) host.pop_back();
            }
        }

        LOGD(std::string("[HTTP] Request line: ") + request_line + " Host: " + host);
        auto resp = build_redirect_page(host);
        boost::asio::write(*sock, boost::asio::buffer(resp));
        LOGI(std::string("[HTTP] Served redirect to host '") + host + "'");
    } catch (const std::exception& e) {
        LOGE(std::string("[HTTP] client error: ") + e.what());
    }
}
