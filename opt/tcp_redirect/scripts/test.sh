#!/bin/bash

echo "=== Testing TCP Redirect Server on Debian 12 ==="

# 测试HTTP服务
echo "1. Testing HTTP server on port 80..."
curl -s -I http://localhost/ > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ HTTP Server: OK"
else
    echo "✗ HTTP Server: FAILED"
fi

# 测试iptables规则
echo "2. Checking iptables rules..."
if iptables -L OUTPUT -n 2>/dev/null | grep -q "NFQUEUE"; then
    echo "✓ iptables rules: OK"
else
    echo "✗ iptables rules: MISSING"
fi

# 测试进程运行
echo "3. Checking running processes..."
if pgrep -f tcp_redirect_server > /dev/null; then
    echo "✓ Process: RUNNING"
else
    echo "✗ Process: STOPPED"
fi

# 测试网络连接
echo "4. Testing network connectivity..."
if netstat -tln 2>/dev/null | grep -q ":80 "; then
    echo "✓ Port 80: LISTENING"
else
    echo "✗ Port 80: NOT LISTENING"
fi

# 测试TCP窗口修改功能
echo "5. Testing TCP packet capture..."
if tcpdump -i any -c 1 port 80 2>/dev/null | grep -q "tcp"; then
    echo "✓ TCP packet capture: WORKING"
else
    echo "✗ TCP packet capture: NOT WORKING"
fi

echo "=== Test completed ==="
