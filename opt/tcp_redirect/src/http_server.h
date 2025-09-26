#pragma once

class HTTPServer {
public:
    explicit HTTPServer(short port);
    void run();
};