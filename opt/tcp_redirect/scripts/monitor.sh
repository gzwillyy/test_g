# 查看服务日志
journalctl -u tcp_redirect -f

# 实时监控网络包
tcpdump -i any port 80 -n

# 检查NFQUEUE统计
iptables -L OUTPUT -v -n