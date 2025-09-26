#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="tcp_redirect"
BIN="/opt/tcp_redirect/tcp_redirect_server"
SRC_DIR="/opt/tcp_redirect/src"
SCRIPT_DIR="/opt/tcp_redirect/scripts"

log()  { echo "[DEPLOY] $*"; }
err()  { echo "[DEPLOY][ERROR] $*" >&2; }
ok()   { echo "[DEPLOY][OK] $*"; }

# 0) 以 root 运行检查
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  err "Please run as root"; exit 1
fi

echo "=== TCP Redirect Server Deployment (Debian 12) ==="

# 1) 停止现有服务
log "Stop existing service (if any)..."
systemctl stop "${SERVICE_NAME}" 2>/dev/null || true
pkill -f "${BIN}" 2>/dev/null || true

# 2) 清理旧的 iptables 规则（避免重复）
log "Clean iptables NFQUEUE rules..."
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags SYN,ACK SYN,ACK -j NFQUEUE --queue-num 1000 2>/dev/null || true
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags ACK ACK         -j NFQUEUE --queue-num 1001 2>/dev/null || true
iptables -D OUTPUT -p tcp --sport 80 --tcp-flags PSH,ACK PSH,ACK -j NFQUEUE --queue-num 1002 2>/dev/null || true

# 3) 编译
log "Build..."
"${SCRIPT_DIR}/build.sh"
ok "OK -> ${BIN}"

# 4) 生成 systemd unit（包含自适应预热相关环境变量）
log "Create/Update systemd service..."
cat > /etc/systemd/system/${SERVICE_NAME}.service <<'UNIT'
[Unit]
Description=TCP Redirect Server with NFQUEUE Window Control (adaptive warmup)
Wants=network-online.target
After=network-online.target

[Service]
WorkingDirectory=/
Type=simple
User=root
WorkingDirectory=/opt/tcp_redirect

# 启动前确保内核模块就绪（NFQUEUE 依赖 AF_NETLINK）
ExecStartPre=/sbin/modprobe nfnetlink
ExecStartPre=/sbin/modprobe nfnetlink_queue

# 程序路径
ExecStart=/opt/tcp_redirect/tcp_redirect_server

# —— 环境变量：自适应预热 & 日志 & 行为控制 ——
Environment=TCP_REDIRECT_LOG_LEVEL=DEBUG

# 是否在 握手阶段的 SYN+ACK 包 里修改窗口大小（方案A默认关闭，避免公网 RST）
Environment=TCP_TAMPER_ON_SYNACK=1

# 预热阈值（累计 ACK 到多少字节后收紧） 0 – 10MB。
# 作用：在客户端累计发送超过这个字节数之前，服务器保持一个比较宽松的“预热窗口”；一旦超过这个阈值，就切换到“收紧窗口”
Environment=TCP_WARMUP_BYTES=20

# 预热窗口（较大一些，促使请求头尽快推完）预热窗口大小（单位：字节）。 范围：0–65535
# 作用：当连接还没达到 TCP_WARMUP_BYTES 阈值时，用这个较大的窗口值来回报给客户端，让它能尽快把请求头推送过来。
Environment=TCP_WARMUP_WINDOW=0

# 收紧后的目标窗口（“抢答模式”） 范围：0–65535
# 也就是在抢答模式里，所有符合条件的 ACK / PSH+ACK（以及 SYN+ACK，如果开启 tamper）都被改写为这个大小
Environment=TCP_TAMPER_WINDOW=20

# 单个 TCP 会话的空闲超时时间 会话空闲回收、状态容量
# 作用：如果某个连接在这段时间内没有任何数据包流动，就从状态表里清理掉，避免状态膨胀。
Environment=TCP_CONN_IDLE_SEC=30

# 状态表最大容量（活跃连接数上限）
# 作用：用于防止内存被大量连接耗尽。当活跃连接数超过这个值时，可能会丢弃旧的或新的状态
Environment=TCP_STATE_CAP=50000

# 允许我们设置 iptables / 使用原始套接字 / 绑定 80 端口
CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_RAW CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_RAW CAP_NET_BIND_SERVICE
NoNewPrivileges=true

# NFQUEUE 需要 AF_NETLINK；HTTP 需要 AF_INET/AF_INET6
RestrictAddressFamilies=AF_INET AF_INET6 AF_NETLINK

# 一些合理的约束（不要过度收紧以免影响 NFQUEUE）
PrivateTmp=true
ProtectHostname=true
ProtectKernelTunables=true
ProtectControlGroups=true
ProtectClock=true
ProtectKernelLogs=true
ProtectSystem=full
ProtectHome=true
LockPersonality=true
MemoryDenyWriteExecute=true

# 资源与重启策略
LimitNOFILE=1048576
TasksMax=infinity
Restart=always
RestartSec=2

StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
UNIT

# 5) 重新加载 systemd
log "Reload systemd..."
systemctl daemon-reload

# 6) 基础防火墙放行 80（有 ufw 用 ufw，没装则用 iptables 兜底）
log "Open firewall (port 80)..."
if command -v ufw >/dev/null 2>&1; then
  ufw allow 80/tcp || true
else
  iptables -C INPUT -p tcp --dport 80 -j ACCEPT 2>/dev/null || iptables -I INPUT -p tcp --dport 80 -j ACCEPT
fi

# 7) 完成提示
ok "Done."
echo "Start:   systemctl start ${SERVICE_NAME}"
echo "Enable:  systemctl enable ${SERVICE_NAME}"
echo "Status:  systemctl status ${SERVICE_NAME} --no-pager"
