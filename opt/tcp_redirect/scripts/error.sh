# 实时日志
journalctl -u tcp_redirect -f

# 近1小时日志
journalctl -u tcp_redirect --since "1 hour ago"

# 最近100行
journalctl -u tcp_redirect -n 100 --no-pager

# NFQUEUE 统计（iptables）
iptables -L OUTPUT -v -n | grep NFQUEUE

# 网络流量监控
tcpdump -ni any port 80 -vv
