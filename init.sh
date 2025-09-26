# 更新系统
# bash <(curl -sSL https://linuxmirrors.cn/main.sh)

# 更新系统
apt update && apt upgrade -y

# 基础与网络依赖
apt install -y build-essential cmake git wget curl vim pkg-config \
               libnetfilter-queue-dev libnl-3-dev libnl-genl-3-dev \
               libboost-all-dev iptables-persistent tcpdump iproute2 nftables

# 可选：安装 ufw（若你用它）
# apt install -y ufw || true

# 验证
gcc --version
pkg-config --list-all | grep -E 'netfilter|nfqueue'




rm -rf /opt/tcp_redirect
mv ./opt/tcp_redirect /opt/tcp_redirect
cd /opt/tcp_redirect
/opt/tcp_redirect/scripts/deploy.sh

