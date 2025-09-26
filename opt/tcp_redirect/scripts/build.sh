#!/usr/bin/env bash
set -euo pipefail

echo "[BUILD] Building TCP Redirect Server (Debian 12)..."

cd /opt/tcp_redirect/src

# 依赖检查（libnetfilter_queue）
if ! pkg-config --exists libnetfilter_queue; then
    echo "[BUILD][ERROR] libnetfilter_queue not found!"
    exit 1
fi

CXXFLAGS="$(pkg-config --cflags libnetfilter_queue) -std=c++17 -O3 -pthread -DASIO_STANDALONE -I/opt/tcp_redirect/src"
LIBS="$(pkg-config --libs libnetfilter_queue)"  # 一般会带上 -lnetfilter_queue -lmnl

# 编译（确保包含 http_server.cpp）
g++ ${CXXFLAGS} \
    tcp_window_controller.cpp http_server.cpp main.cpp \
    ${LIBS} \
    -o /opt/tcp_redirect/tcp_redirect_server

echo "[BUILD] OK -> /opt/tcp_redirect/tcp_redirect_server"
chmod +x /opt/tcp_redirect/tcp_redirect_server
