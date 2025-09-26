# 设置脚本权限
chmod +x /opt/tcp_redirect/scripts/*.sh

# 编译程序
/opt/tcp_redirect/scripts/build.sh

# 部署服务
/opt/tcp_redirect/scripts/deploy.sh

# 启动服务
systemctl start tcp_redirect
systemctl enable tcp_redirect

# 检查服务状态
systemctl status tcp_redirect

# 测试服务
/opt/tcp_redirect/scripts/test.sh