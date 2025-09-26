# 检查内核模块
lsmod | grep nfnetlink

# 检查库依赖
ldd /opt/tcp_redirect/tcp_redirect_server

# 检查系统日志
dmesg | tail

# 手动运行测试（调试模式）
/opt/tcp_redirect/tcp_redirect_server