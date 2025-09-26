#!/bin/bash

echo "=== TCP Redirect Server Deployment on Debian 12 ==="

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
/opt/tcp_redirect/scripts/build.sh

if [ $? -ne 0 ]; then
    echo "Build failed! Deployment aborted."
    exit 1
fi

# 4. 创建系统服务文件
echo "Creating systemd service..."
cat > /etc/systemd/system/tcp_redirect.service << 'SERVICEEOF'
[Unit]
Description=TCP Redirect Server with Window Control
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/tcp_redirect
ExecStart=/opt/tcp_redirect/tcp_redirect_server
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SERVICEEOF

# 5. 重新加载systemd
systemctl daemon-reload

# 6. 配置防火墙（如果需要）
echo "Configuring firewall..."
ufw allow 80/tcp 2>/dev/null || iptables -I INPUT -p tcp --dport 80 -j ACCEPT

echo "Deployment completed successfully!"
echo "Start the service with: systemctl start tcp_redirect"
echo "Enable auto-start with: systemctl enable tcp_redirect"
