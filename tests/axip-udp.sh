#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -f "$tmp/ax25ip.o" "$tmp/axip-udp"; rmdir "$tmp"' EXIT HUP INT TERM

${CC:-cc} -std=gnu99 -funsigned-char -ffunction-sections -fdata-sections \
  -Iinclude -Ios/linux -pthread -c os/linux/ax25ip.c -o "$tmp/ax25ip.o"
${CC:-cc} -std=gnu99 -Wall -Wextra -Werror -Wl,--gc-sections -Wl,--wrap=socket \
  tests/axip-udp.c "$tmp/ax25ip.o" -o "$tmp/axip-udp"
"$tmp/axip-udp"
