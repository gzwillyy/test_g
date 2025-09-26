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
# 永久加载（开机自动）
cat >/etc/modules-load.d/tcp_redirect.conf <<'EOF'
nfnetlink
nfnetlink_queue
EOF

# 立即尝试加载（即使已加载或加载失败也不阻塞）
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
Environment=TCP_REDIRECT_LOG_LEVEL=DEBUG

# 容错式预加载模块（即使失败也不终止启动）
ExecStartPre=/bin/sh -c '/sbin/modprobe nfnetlink 2>/dev/null || /usr/sbin/modprobe nfnetlink 2>/dev/null || true'
ExecStartPre=/bin/sh -c '/sbin/modprobe nfnetlink_queue 2>/dev/null || /usr/sbin/modprobe nfnetlink_queue 2>/dev/null || true'

ExecStart=/opt/tcp_redirect/tcp_redirect_server
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

# 必要能力：NFQUEUE 需要 NETLINK，绑定80要 BIND_SERVICE，改iptables要 NET_ADMIN，抓包路径用到 NET_RAW
CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_BIND_SERVICE CAP_NET_RAW
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_BIND_SERVICE CAP_NET_RAW
NoNewPrivileges=true

# systemd sandboxing —— 关键是放开 AF_NETLINK
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
