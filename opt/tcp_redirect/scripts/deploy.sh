# /opt/tcp_redirect/scripts/deploy.sh
#!/bin/bash

echo "=== TCP Redirect Server Deployment ==="

# 1. 停止现有服务
echo "Stopping existing service..."
systemctl stop tcp_redirect 2>/dev/null
pkill -f tcp_redirect_server

# 2. 清理iptables规则
echo "Cleaning existing iptables rules..."
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null

# 3. 编译程序
echo "Building the application..."
chmod +x /opt/tcp_redirect/scripts/build.sh
/opt/tcp_redirect/scripts/build.sh

if [ $? -ne 0 ]; then
    echo "Build failed! Deployment aborted."
    exit 1
fi

# 4. 安装服务
echo "Installing systemd service..."
cp /opt/tcp_redirect/scripts/tcp_redirect.service /etc/systemd/system/
systemctl daemon-reload

# 5. 配置防火墙（如果需要）
echo "Configuring firewall..."
firewall-cmd --permanent --add-port=80/tcp 2>/dev/null
firewall-cmd --reload 2>/dev/null

# 6. 设置SELinux（如果启用）
if sestatus | grep -q "enabled"; then
    echo "Configuring SELinux..."
    setsebool -P nis_enabled 1
    semanage port -a -t http_port_t -p tcp 80 2>/dev/null || true
fi

echo "Deployment completed successfully!"
echo "Start the service with: systemctl start tcp_redirect"
echo "Enable auto-start with: systemctl enable tcp_redirect"