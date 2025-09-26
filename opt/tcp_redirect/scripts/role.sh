# 设置脚本权限
chmod +x /opt/tcp_redirect/scripts/*.sh

# 编译程序
/opt/tcp_redirect/scripts/build.sh

# 部署服务
/opt/tcp_redirect/scripts/deploy.sh

# 启动服务
systemctl start tcp_redirect

# 设置开机自启
systemctl enable tcp_redirect

# 检查服务状态
systemctl status tcp_redirect --no-pager

# 测试服务
/opt/tcp_redirect/scripts/test.sh



# 实时查看服务日志
journalctl -u tcp_redirect -f

# 查看最近的服务日志
journalctl -u tcp_redirect --since "1 hour ago"

# 查看完整的服务日志
journalctl -u tcp_redirect -n 100

# 监控网络流量
tcpdump -i any port 80 -n -v

# 检查NFQUEUE统计
iptables -L OUTPUT -v -n | grep NFQUEUE

# 监控脚本
/opt/tcp_redirect/scripts/monitor.sh
# 性能监控脚本
/opt/tcp_redirect/scripts/performance.sh


# 重启服务
systemctl restart tcp_redirect

# 停止服务
systemctl stop tcp_redirect

# 查看服务日志
journalctl -u tcp_redirect

# 重新部署（当代码更新时）
/opt/tcp_redirect/scripts/deploy.sh

# 备份iptables规则
iptables-save > /opt/tcp_redirect/backup/iptables.backup

# 恢复iptables规则（如果需要）
iptables-restore < /opt/tcp_redirect/backup/iptables.backup