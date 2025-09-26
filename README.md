# test_g

# 设置脚本权限
chmod +x /opt/tcp_redirect/scripts/*.sh

# 编译程序
/opt/tcp_redirect/scripts/build.sh

# 部署服务
/opt/tcp_redirect/scripts/deploy.sh

# 确认服务与 NFQUEUE 规则
systemctl status tcp_redirect --no-pager
iptables -S OUTPUT | grep NFQUEUE


# curl 功能测试（应返回 200 + HTML）

curl -v http://127.0.0.1/ --max-time 5 | head -20

```
如果你把 TCP_TAMPER_WINDOW 设为极小（比如 0 或 1），传输会非常慢是预期的；先用 TCP_TAMPER_WINDOW=4096 验证速度，再改回 1/0 做“抢答”模式更直观。
```

# 永久参数可在 unit 里：/etc/systemd/system/tcp_redirect.service
```
Environment=TCP_TAMPER_WINDOW=20
Environment=TCP_TAMPER_ON_SYNACK=1
```
然后：systemctl daemon-reload && systemctl restart tcp_redirect



# tcpdump 抓包验证（看到 wscale=0 & win=目标值）

```sh
# 抓 SYN+ACK（应看到 wscale 0 和 win 1（或你的设定））：
timeout 6 tcpdump -i lo -nn -vvv \
  'tcp src port 80 and (tcp[13] & 0x12 == 0x12)' \
  -c 3

#再抓已建立连接方向的 ACK / PSH,ACK（窗口应始终是你的目标值）：
timeout 6 tcpdump -i lo -nn -vvv \
  'tcp src port 80 and (tcp[13] & 0x02 == 0) and (tcp[13] & 0x10 != 0)' \
  -c 10

# 触发流量（另一个终端）：
curl -s http://127.0.0.1/ >/dev/null

# 你应该看到的关键字段（示例）：
握手：Flags [S.], ... options [..., wscale 0] ... win 1
后续：Flags [.], ... win 1 或 Flags [P.], ... win 1

# 同时查看应用日志（会有 WSCALE 与 Modify win 行）：
journalctl -u tcp_redirect -n 80 --no-pager
```


```sh
# 切换更“激进”的窗口值（例如 1 或 0）：
sed -i 's/Environment=TCP_TAMPER_WINDOW=.*/Environment=TCP_TAMPER_WINDOW=1/' /etc/systemd/system/tcp_redirect.service
systemctl daemon-reload && systemctl restart tcp_redirect
# 然后再 curl -v http://127.0.0.1/ + tcpdump，你会看到 win 1（或 win 0，零窗口探测会很慢是正常现象）。


# 只在会话阶段限窗（不改握手）：
sed -i 's/Environment=TCP_TAMPER_ON_SYNACK=.*/Environment=TCP_TAMPER_ON_SYNACK=0/' /etc/systemd/system/tcp_redirect.service
systemctl daemon-reload && systemctl restart tcp_redirect
# 再抓 SYN+ACK，你会看到 wscale 不再被改为 0；而 ACK/PSH-ACK 仍是目标 win。


# 查看 NFQUEUE 规则命中计数（应随 curl 增长）：
iptables -L OUTPUT -v -n | grep NFQUEUE

# 如果你要把这套逻辑应用到非回环/公网接口，tcpdump 的 -i lo 替换成实际网卡（比如 -i eth0）即可。
# tcpdump -i eth0 -nn -vvv 'host 175.160.154.159 and tcp port 80' -c 50
```