#!/bin/bash
set -e
echo "=== Performance Monitoring ==="

echo "[CPU Top 5]"
ps aux --sort=-%cpu | head -5
echo

echo "[MEM Top 5]"
ps aux --sort=-%mem | head -5
echo

echo "[Network :80]"
ss -tlnp | grep ':80' || echo "No listener"
echo

echo "[NFQUEUE kernel stats]"
cat /proc/net/netfilter/nfnetlink_queue 2>/dev/null || echo "NFQUEUE stats not available"
