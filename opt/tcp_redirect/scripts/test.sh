#!/bin/bash
set -e
echo "=== Testing TCP Redirect Server ==="

echo "[1] HTTP service on :80"
if curl -s -I http://127.0.0.1/ >/dev/null; then
  echo " ✓ HTTP Server: OK"
else
  echo " ✗ HTTP Server: FAILED"
fi

echo "[2] iptables NFQUEUE rules"
if iptables -L OUTPUT -n 2>/dev/null | grep -q NFQUEUE; then
  echo " ✓ iptables rules: OK"
else
  echo " ✗ iptables rules: MISSING"
fi

echo "[3] Process running?"
if pgrep -f tcp_redirect_server >/dev/null; then
  echo " ✓ Process: RUNNING"
else
  echo " ✗ Process: STOPPED"
fi

echo "[4] Port :80 listening"
if ss -tln 2>/dev/null | grep -q ":80 "; then
  echo " ✓ Port 80: LISTENING"
else
  echo " ✗ Port 80: NOT LISTENING"
fi

echo "[5] tcpdump capture 1 packet (3s timeout)"
if timeout 3 tcpdump -ni any -c 1 port 80 >/dev/null 2>&1; then
  echo " ✓ tcpdump: OK"
else
  echo " ✗ tcpdump: NO PACKET"
fi

echo "[6] NFQUEUE counters (iptables)"
iptables -L OUTPUT -v -n 2>/dev/null | grep NFQUEUE || echo " (no counters)"

echo "=== Test done ==="
