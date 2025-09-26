#!/bin/bash
set -e

echo "[BUILD] Building TCP Redirect Server (Debian 12)..."
cd /opt/tcp_redirect/src

if ! pkg-config --exists libnetfilter_queue; then
  echo "[BUILD] Error: libnetfilter_queue not found!"
  exit 1
fi

CXXFLAGS="$(pkg-config --cflags libnetfilter_queue)"
LIBS="$(pkg-config --libs libnetfilter_queue)"

# 注意：main.cpp 里已经 #include "tcp_window_controller.cpp"
# 这里不再把 tcp_window_controller.cpp 作为独立翻译单元编译，避免重复定义/链接冲突
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -pthread \
    $CXXFLAGS \
    http_server.cpp main.cpp \
    $LIBS -lboost_system \
    -o /opt/tcp_redirect/tcp_redirect_server

chmod +x /opt/tcp_redirect/tcp_redirect_server
echo "[BUILD] OK -> /opt/tcp_redirect/tcp_redirect_server"
