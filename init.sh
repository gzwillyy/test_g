# 更新系统
bash <(curl -sSL https://linuxmirrors.cn/main.sh)

# 更新系统包列表
apt update && apt upgrade -y

# 安装基础开发工具
apt install -y build-essential cmake git wget curl vim

# 安装网络和开发库
apt install -y libnetfilter-queue-dev libnl-3-dev libnl-genl-3-dev
apt install -y libboost-all-dev libasio-dev pkg-config

# 安装系统工具
apt install -y net-tools tcpdump iptables-persistent

# 验证安装
gcc --version
pkg-config --list-all | grep netfilter



rm -rf /opt/tcp_redirect
mv ./opt/tcp_redirect /opt/tcp_redirect
