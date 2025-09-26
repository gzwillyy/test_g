#!/bin/bash

echo "=== Real-time Monitoring ==="
echo "Service Status: $(systemctl is-active tcp_redirect)"
echo "Process Count: $(pgrep -f tcp_redirect_server | wc -l)"
echo "TCP Connections on port 80:"
netstat -tlnp | grep :80 || echo "None"

echo ""
echo "Recent Logs:"
journalctl -u tcp_redirect -n 10 --no-pager

echo ""
echo "NFQUEUE Rules:"
iptables -L OUTPUT -n -v | grep NFQUEUE || echo "No NFQUEUE rules found"
