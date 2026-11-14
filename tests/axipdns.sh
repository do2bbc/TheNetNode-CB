#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test"; rmdir "$tmp"' EXIT HUP INT TERM

${CC:-cc} -std=gnu99 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ios/linux tests/axipdns.c os/linux/axipdns.c \
  -Wl,--wrap=getaddrinfo -o "$tmp/test"
timeout 10 "$tmp/test"
