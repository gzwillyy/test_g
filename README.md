# test_g

# 验证完整流程
echo "=== Final Verification ==="

# 1. 检查服务状态
systemctl status tcp_redirect

# 2. 测试HTTP访问
curl -s http://localhost/ | head -20

# 3. 检查iptables规则
iptables -L OUTPUT -n | grep NFQUEUE

# 4. 验证进程运行
ps aux | grep tcp_redirect

# 5. 测试外部访问（从另一台机器）
# curl -H "Host: example.com" http://YOUR_SERVER_IP/

echo "=== Setup Complete ==="