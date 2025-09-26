#!/bin/bash

echo "Building TCP Redirect Server for Debian 12..."

cd /opt/tcp_redirect/src

# 检查依赖
if ! pkg-config --exists libnetfilter_queue; then
    echo "Error: libnetfilter_queue not found!"
    exit 1
fi

# 获取编译标志
CXXFLAGS=$(pkg-config --cflags libnetfilter_queue)
LIBS=$(pkg-config --libs libnetfilter_queue)

# 编译命令
g++ -std=c++17 -O3 -pthread \
    $CXXFLAGS \
    tcp_window_controller.cpp http_server.cpp main.cpp \
    $LIBS -lboost_system -lboost_thread \
    -o /opt/tcp_redirect/tcp_redirect_server

if [ $? -eq 0 ]; then
    echo "Build successful! Binary: /opt/tcp_redirect/tcp_redirect_server"
    chmod +x /opt/tcp_redirect/tcp_redirect_server
else
    echo "Build failed!"
    exit 1
fi
