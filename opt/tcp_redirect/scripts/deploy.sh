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

echo "[DEPLOY] Ensure kernel modules autoload..."
cat >/etc/modules-load.d/tcp_redirect.conf <<'EOF'
nfnetlink
nfnetlink_queue
EOF
modprobe nfnetlink        2>/dev/null || true
modprobe nfnetlink_queue  2>/dev/null || true

echo "[DEPLOY] Create/Update systemd service..."
cat >/etc/systemd/system/tcp_redirect.service <<'EOF'
[Unit]
Description=TCP Redirect Server with NFQUEUE Window Control
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/tcp_redirect
# ☆ 业务参数（可改）：窗口默认 1；是否改 SYN+ACK=1
Environment=TCP_REDIRECT_LOG_LEVEL=DEBUG
Environment=TCP_TAMPER_WINDOW=1
Environment=TCP_TAMPER_ON_SYNACK=1

# 预加载内核模块（容错）
ExecStartPre=/bin/sh -c '/sbin/modprobe nfnetlink 2>/dev/null || /usr/sbin/modprobe nfnetlink 2>/dev/null || true'
ExecStartPre=/bin/sh -c '/sbin/modprobe nfnetlink_queue 2>/dev/null || /usr/sbin/modprobe nfnetlink_queue 2>/dev/null || true'

ExecStart=/opt/tcp_redirect/tcp_redirect_server
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

# 能力与 sandbox（务必放开 AF_NETLINK）
CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_BIND_SERVICE CAP_NET_RAW
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_BIND_SERVICE CAP_NET_RAW
NoNewPrivileges=true
ProtectSystem=full
ProtectHome=true
PrivateTmp=true
PrivateDevices=true
LockPersonality=true
MemoryDenyWriteExecute=true
RestrictAddressFamilies=AF_INET AF_INET6 AF_NETLINK

[Install]
WantedBy=multi-user.target
EOF

echo "[DEPLOY] Reload systemd..."
systemctl daemon-reload

echo "[DEPLOY] Open firewall (port 80)..."
ufw allow 80/tcp 2>/dev/null || true
iptables -I INPUT -p tcp --dport 80 -j ACCEPT 2>/dev/null || true

echo "[DEPLOY] Start & Enable service..."
systemctl start tcp_redirect
systemctl enable tcp_redirect 2>/dev/null || true

echo "[DEPLOY] Status:"
systemctl status tcp_redirect --no-pager || true
echo "[DEPLOY] Done."




# 部署后可启用 nft 规则（任选其一方案；以下仅示例）
# nft add table inet tcpredir || true
# nft 'add chain inet tcpredir out { type filter hook output priority 0; }' || true
# nft 'add rule inet tcpredir out tcp sport 80 tcp flags syn,ack queue num 1000' || true
# nft 'add rule inet tcpredir out tcp sport 80 tcp flags ack queue num 1001' || true
# nft 'add rule inet tcpredir out tcp sport 80 tcp flags psh,ack queue num 1002' || true
# 删除： nft delete table inet tcpredir