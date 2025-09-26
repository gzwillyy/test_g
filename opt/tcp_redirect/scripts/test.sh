# /opt/tcp_redirect/scripts/test.sh
#!/bin/bash

echo "=== Testing TCP Redirect Server ==="

# 测试HTTP服务
echo "1. Testing HTTP server on port 80..."
curl -I http://localhost/ 2>/dev/null && echo "HTTP Server: OK" || echo "HTTP Server: FAILED"

# 测试iptables规则
echo "2. Checking iptables rules..."
iptables -L OUTPUT -n | grep NFQUEUE && echo "iptables rules: OK" || echo "iptables rules: MISSING"

# 测试进程运行
echo "3. Checking running processes..."
pgrep -f tcp_redirect_server && echo "Process: RUNNING" || echo "Process: STOPPED"

# 测试网络连接
echo "4. Testing network connectivity..."
netstat -tlnp | grep :80 && echo "Port 80: LISTENING" || echo "Port 80: NOT LISTENING"

echo "=== Test completed ==="