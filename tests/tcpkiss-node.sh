#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
cleanup()
{
  rm -f "$tmp/tnn.ini" "$tmp/tnn179.pas" "$tmp/tnn179.tnb" \
    "$tmp/TNN179.PAS" "$tmp/TNN179.TNB" "$tmp/console.log" "$tmp/tnn.pid" \
    "$tmp/TNN179.STA" "$tmp/GRAPH.STA" "$tmp/startup.log" "$tmp/STARTUP.LOG" \
    "$tmp/debug.log" "$tmp/tnn-runtime.log" "$tmp/tnn-crash.log" \
    "$tmp/MHEARD.TAB" "$tmp/L3HEARD.TAB"
  for directory in msg pacsat speech sysexe textcmd userexe; do
    if test -d "$tmp/$directory"; then
      rmdir "$tmp/$directory"
    fi
  done
  rmdir "$tmp"
}
trap cleanup EXIT HUP INT TERM
mode=${1:-native}
case "$mode" in
native)
cp "$root/packaging/debian/tnn179.pas" "$tmp/"
cat > "$tmp/tnn.ini" <<EOF
buffers 10000
rounds 100
tnn_dir $tmp/
tnn_procfile $tmp/tnn.pid
EOF
config=tnn179.tnb
startup=startup.log
;;
win32)
cp "$root/os/win32/portable/TNN179.PAS" "$tmp/"
cp "$root/os/win32/portable/tnn.ini" "$tmp/"
config=TNN179.TNB
startup=STARTUP.LOG
;;
*) echo "Usage: sh tests/tcpkiss-node.sh [native|win32]" >&2; exit 1 ;;
esac
cat > "$tmp/$config" <<EOF
ATTACH PORT=7 KTYP=TCPKISS DEVICE=127.0.0.1:${DIREWOLF_TEST_PORT:-8001} CHANNEL=0
ATTACH PORT=3 KTYP=TCPKISS DEVICE=127.0.0.1:${DIREWOLF_TEST_PORT:-8001} CHANNEL=1
PORT 7 NAME=DW0 ON MODE=1200
PORT 3 NAME=DW1 ON MODE=1200
EOF
cd "$tmp"
result=0
if test "$mode" = win32; then
  timeout -k 5 8 wine "$root/os/win32/tnn32/tnn32.exe" -v > console.log 2>&1 || result=$?
else
  timeout -k 5 5 "$root/bin/tnn" -v -i "$tmp/tnn.ini" > console.log 2>&1 || result=$?
fi
cat console.log
test "$result" -eq 124
grep -q 'TCPKISS .*: connected' console.log
grep -q 'Port 7 Attached' "$startup"
grep -q 'Port 3 Attached' "$startup"
echo "Full TNN ($mode): TCPKISS startup configuration and Dire Wolf connection passed"
