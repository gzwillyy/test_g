#!/bin/bash
set -e
echo "=== Real-time Monitoring ==="
echo "Service Status: $(systemctl is-active tcp_redirect || echo inactive)"
echo "Process Count: $(pgrep -f tcp_redirect_server | wc -l)"
echo
echo "TCP connections on :80 (LISTEN + ESTABLISHED):"
ss -tlpn | grep ':80' || echo "None (LISTEN)"
echo
echo "Established:"
ss -tnp state established '( sport = :80 or dport = :80 )' || echo "None"
echo
echo "Recent Logs:"
journalctl -u tcp_redirect -n 20 --no-pager || true
echo
echo "NFQUEUE Rules (iptables):"
iptables -L OUTPUT -v -n | grep NFQUEUE || echo "No NFQUEUE rules found"
