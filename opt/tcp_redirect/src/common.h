// /opt/tcp_redirect/src/common.h
#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

// 网络头文件
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

// NFQUEUE
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/netfilter.h>

// 定义常量
const int NFQUEUE_NUM[] = {1000, 1001, 1002};
const int SERVER_PORT = 80;
const uint16_t TARGET_WINDOW_SIZE = 0;

#endif