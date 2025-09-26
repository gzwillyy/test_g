# 更新系统
yum update -y

# 安装基础开发工具
yum groupinstall -y "Development Tools"
yum install -y epel-release
yum install -y cmake wget git vim


# 安装网络库
yum install -y libnetfilter_queue-devel libnl3-devel

# 安装Boost库（ASIO需要）
yum install -y boost-devel boost-system

# 安装其他依赖
yum install -y kernel-devel-$(uname -r) kernel-headers-$(uname -r)


# 下载最新ASIO（如果Boost版本较老）
# cd /usr/local/src
# wget https://sourceforge.net/projects/asio/files/asio/1.28.0%20%28Stable%29/asio-1.28.0.tar.gz
# tar -xzf asio-1.28.0.tar.gz
# cd asio-1.28.0
# ./configure --prefix=/usr/local
# make && make install

# # 设置库路径
# echo "/usr/local/lib" >> /etc/ld.so.conf.d/local.conf
# ldconfig


# 创建项目目录结构
# mkdir -p /opt/tcp_redirect/{src,scripts,logs,config}
rm -rf /opt/tcp_redirect
mv ./tcp_redirect /opt/tcp_redirect
