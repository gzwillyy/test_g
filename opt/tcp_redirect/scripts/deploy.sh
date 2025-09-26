#!/bin/bash
set -e

echo "=== TCP Redirect Server Deployment (Debian 12) ==="

mkdir -p /opt/tcp_redirect/{logs,backup}

echo "[DEPLOY] Stop existing service (if any)..."
systemctl stop tcp_redirect 2>/dev/null || true
pkill -f tcp_redirect_server 2>/dev/null || true

echo "[DEPLOY] Clean iptables NFQUEUE rules..."
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null || true
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK -j NFQUEUE --queue-num 1001 2>/dev/null || true
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null || true

echo "[DEPLOY] Build..."
/opt/tcp_redirect/scripts/build.sh

echo "[DEPLOY] Create systemd service..."
cat >/etc/systemd/system/tcp_redirect.service <<'EOF'
[Unit]
Description=TCP Redirect Server with NFQUEUE Window Control
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/tcp_redirect
Environment=TCP_REDIRECT_LOG_LEVEL=DEBUG
ExecStart=/opt/tcp_redirect/tcp_redirect_server
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

# 最小能力与常见硬化
CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_BIND_SERVICE
NoNewPrivileges=true
ProtectSystem=full
ProtectHome=true
RestrictAddressFamilies=AF_INET AF_INET6
LockPersonality=true
PrivateTmp=true
PrivateDevices=true
MemoryDenyWriteExecute=true

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload

echo "[DEPLOY] Open port 80..."
ufw allow 80/tcp 2>/dev/null || true
iptables -I INPUT -p tcp --dport 80 -j ACCEPT 2>/dev/null || true

echo "[DEPLOY] Done."
echo "Use: systemctl start tcp_redirect && systemctl enable tcp_redirect"



# 部署后可启用 nft 规则（任选其一方案；以下仅示例）
# nft add table inet tcpredir || true
# nft 'add chain inet tcpredir out { type filter hook output priority 0; }' || true
# nft 'add rule inet tcpredir out tcp sport 80 tcp flags syn,ack queue num 1000' || true
# nft 'add rule inet tcpredir out tcp sport 80 tcp flags ack queue num 1001' || true
# nft 'add rule inet tcpredir out tcp sport 80 tcp flags psh,ack queue num 1002' || true
# 删除： nft delete table inet tcpredir