#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-x86_64-w64-mingw32-gcc}
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}
strings=${STRINGS:-x86_64-w64-mingw32-strings}
bindir=${WIN32_BINDIR:-os/win32/tnn64}
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/headers"; rmdir "$tmp"' EXIT HUP INT TERM
cat > "$tmp/test.c" <<'EOF'
#include "tnn.h"
_Static_assert(sizeof(void *) == 8, "Win64 package must be 64-bit");
_Static_assert(sizeof(SOCKET) == sizeof(void *), "Winsock SOCKET must be pointer-sized");
_Static_assert(sizeof(TNN_SOCKET) == sizeof(SOCKET), "TNN sockets must not truncate Winsock handles");
_Static_assert(sizeof(((TCPIP *)0)->sock) == sizeof(SOCKET), "TCP socket storage must be pointer-sized");
_Static_assert(sizeof(((T_INTERFACE *)0)->OsSock) == sizeof(SOCKET), "listener socket must be pointer-sized");
_Static_assert(sizeof(((READRX *)0)->Sock) == sizeof(SOCKET), "RX queue socket must be pointer-sized");
_Static_assert(sizeof(((SENDTX *)0)->Sock) == sizeof(SOCKET), "TX queue socket must be pointer-sized");
_Static_assert(sizeof(((DEVICE *)0)->kisslink) == sizeof(void *), "serial handles must be pointer-sized");
_Static_assert(sizeof(BYTE) == 1 && (BYTE)-1 < 0, "TNN BYTE must be signed");
_Static_assert(sizeof(WORD) == 2 && (WORD)-1 < 0, "TNN WORD must be signed");
_Static_assert(sizeof(LONG) == 4, "TNN LONG must remain 32-bit");
_Static_assert(sizeof(WIN32_BYTE) == 1 && (WIN32_BYTE)-1 > 0,
               "Win32 BYTE must remain unsigned");
_Static_assert(sizeof(WIN32_WORD) == 2 && (WIN32_WORD)-1 > 0,
               "Win32 WORD must remain unsigned");
EOF
"$compiler" -std=c11 -DWIN32 -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 \
  -Iinclude -Ios/win32 -Ios/linux -Isrc -fsyntax-only "$tmp/test.c"
"$strings" "$bindir/tnn64.exe" | grep -Fq 'TheNetNode (Win64) (CB)'
for binary in "$bindir/tnn64.exe" "$bindir/help.exe" "$bindir/msg.exe" \
              "$bindir/msy.exe" "$bindir/top.exe"; do
  "$objdump" -f "$binary" > "$tmp/headers"
  grep -Eq 'architecture: i386:x86-64|file format pe-x86-64' "$tmp/headers"
  "$objdump" -p "$binary" > "$tmp/headers"
  grep -Eq '^MajorSubsystemVersion[[:space:]]+6$' "$tmp/headers"
  grep -Eq '^MinorSubsystemVersion[[:space:]]+0$' "$tmp/headers"
  awk '
    /DLL Name:/ {
      dll = tolower($3)
      if (dll != "advapi32.dll" && dll != "comdlg32.dll" &&
          dll != "gdi32.dll" && dll != "kernel32.dll" &&
          dll != "msvcrt.dll" && dll != "ole32.dll" &&
          dll != "oleaut32.dll" && dll != "shell32.dll" &&
          dll != "user32.dll" && dll != "winspool.drv" &&
          dll != "ws2_32.dll" && dll != "wsock32.dll" &&
          dll != "uuid.dll" && dll != "odbc32.dll" &&
          dll != "odbccp32.dll") {
        print "Unexpected runtime dependency: " $3
        exit 1
      }
      count++
    }
    END { if (!count) exit 1 }
  ' "$tmp/headers"
done
printf 'Win64 ABI, PE architecture, subsystem 6.0 and DLL checks passed.\n'
