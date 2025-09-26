#!/bin/bash

echo "=== Performance Monitoring ==="
echo "CPU Usage:"
ps aux --sort=-%cpu | head -5

echo ""
echo "Memory Usage:"
ps aux --sort=-%mem | head -5

echo ""
echo "Network Connections:"
ss -tlnp | grep :80

echo ""
echo "Packet Queue Status:"
cat /proc/net/netfilter/nfnetlink_queue 2>/dev/null || echo "NFQUEUE stats not available"
