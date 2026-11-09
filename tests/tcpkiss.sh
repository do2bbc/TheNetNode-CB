#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -f "$tmp/tcpkiss.o" "$tmp/test" "$tmp/test.exe" "$tmp/send.h"; rmdir "$tmp"' EXIT HUP INT TERM
case "${1:-native}" in
  native)
    flags="-Iinclude -Ios/linux -Isrc -Dstricmp=strcasecmp -Wall -Wextra -Werror -funsigned-char -g -fsanitize=address,undefined"
    ${CC:-cc} $flags -Dsend=tcpkiss_test_send -c src/tcpkiss.c -o "$tmp/tcpkiss.o"
    ${CC:-cc} $flags tests/tcpkiss.c "$tmp/tcpkiss.o" -o "$tmp/test"
    "$tmp/test"
    if test -n "${DIREWOLF_TEST_PORT:-}"; then
      "$tmp/test" "$DIREWOLF_TEST_PORT"
    fi
    ;;
  win32)
    flags="-Iinclude -Ios/win32 -Ios/linux -Isrc -DWIN32 -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -Wall -Wextra -Wno-unknown-pragmas -funsigned-char -g"
    cat > "$tmp/send.h" <<'EOF'
#include "winclude.h"
int WSAAPI tcpkiss_test_send(SOCKET, const char *, int, int);
#define send tcpkiss_test_send
EOF
    ${CC:-i686-w64-mingw32-gcc} $flags -include "$tmp/send.h" -c src/tcpkiss.c -o "$tmp/tcpkiss.o"
    ${CC:-i686-w64-mingw32-gcc} $flags tests/tcpkiss.c "$tmp/tcpkiss.o" -static -lws2_32 -o "$tmp/test.exe"
    timeout 90 wine "$tmp/test.exe"
    if test -n "${DIREWOLF_TEST_PORT:-}"; then
      timeout 90 wine "$tmp/test.exe" "$DIREWOLF_TEST_PORT"
    fi
    ;;
  *) echo "Usage: sh tests/tcpkiss.sh [native|win32]" >&2; exit 1 ;;
esac
