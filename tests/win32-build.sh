#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-i686-w64-mingw32-gcc}
objdump=${OBJDUMP:-i686-w64-mingw32-objdump}
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/headers"; rmdir "$tmp"' EXIT HUP INT TERM
cat > "$tmp/test.c" <<'EOF'
#include "tnn.h"
_Static_assert(sizeof(void *) == 4, "Win32 package must be 32-bit");
_Static_assert(sizeof(BYTE) == 1 && (BYTE)-1 < 0, "TNN BYTE must be signed");
_Static_assert(sizeof(WORD) == 2 && (WORD)-1 < 0, "TNN WORD must be signed");
_Static_assert(sizeof(LONG) == 4, "TNN LONG must be 32-bit");
_Static_assert(sizeof(WIN32_BYTE) == 1 && (WIN32_BYTE)-1 > 0,
               "Win32 BYTE must remain unsigned");
_Static_assert(sizeof(WIN32_WORD) == 2 && (WIN32_WORD)-1 > 0,
               "Win32 WORD must remain unsigned");
EOF
"$compiler" -std=c11 -DWIN32 -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 \
  -Iinclude -Ios/win32 -Ios/linux -Isrc -fsyntax-only "$tmp/test.c"
for binary in os/win32/tnn32/tnn32.exe os/win32/tnn32/help.exe \
              os/win32/tnn32/msg.exe os/win32/tnn32/msy.exe \
              os/win32/tnn32/top.exe; do
  "$objdump" -p "$binary" > "$tmp/headers"
  grep -Eq '^MajorSubsystemVersion[[:space:]]+5$' "$tmp/headers"
  grep -Eq '^MinorSubsystemVersion[[:space:]]+1$' "$tmp/headers"
  awk '
    /DLL Name:/ {
      dll = tolower($3)
      if (dll != "kernel32.dll" && dll != "msvcrt.dll" &&
          dll != "user32.dll" && dll != "ws2_32.dll") {
        print "Unexpected runtime dependency: " $3
        exit 1
      }
      count++
    }
    END { if (!count) exit 1 }
  ' "$tmp/headers"
done
printf 'Win32 ABI, subsystem 5.1 and system-only DLL checks passed.\n'
