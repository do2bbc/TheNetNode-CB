#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
container="tnn-direwolf-test-$$"
tmp=$(mktemp -d)
trap 'if podman container exists "$container"; then podman rm -f "$container" >/dev/null; fi; rm -f "$tmp/direwolf.log"; rmdir "$tmp"' EXIT HUP INT TERM
podman build -t localhost/tnn-direwolf-test -f "$root/tests/direwolf.Containerfile" "$root/tests"
# No host ports, audio devices or radio hardware are passed into the container.
podman run -d --name "$container" -v "$root:/source:ro" localhost/tnn-direwolf-test
podman exec "$container" sh -eu -c '
  mkdir /tmp/linux /tmp/win32
  tar -C /source --exclude=.git --exclude="*.o" --exclude=bin -cf - . |
    tar -C /tmp/linux -xf -
  cp -a /tmp/linux/. /tmp/win32/
  cd /tmp/linux
  make tnn -j2
  DIREWOLF_TEST_PORT=8001 sh tests/tcpkiss.sh
  DIREWOLF_TEST_PORT=8001 sh tests/tcpkiss-node.sh
  cd /tmp/win32
  make -f tnn32.mak all PREFIX=i686-w64-mingw32- -j2
  sh tests/win32-build.sh
  WINEPREFIX=/tmp/tnn-wine WINEDEBUG=-all DIREWOLF_TEST_PORT=8001 \
    xvfb-run -a sh tests/tcpkiss.sh win32
  WINEPREFIX=/tmp/tnn-wine WINEDEBUG=-all DIREWOLF_TEST_PORT=8001 \
    xvfb-run -a sh tests/tcpkiss-node.sh win32
'
podman logs "$container" > "$tmp/direwolf.log" 2>&1
cat "$tmp/direwolf.log"
grep -q '\[0L\] N0CALL-1>N0CALL:TNN-TCPKISS' "$tmp/direwolf.log"
grep -q '\[1L\] N0CALL-1>N0CALL:TNN-TCPKISS' "$tmp/direwolf.log"
test "$(grep -c 'Attached to KISS TCP client' "$tmp/direwolf.log")" -ge 4
if grep -Ei 'invalid|unrecognized|unrecognised|bad KISS|KISS.*error' "$tmp/direwolf.log"; then
  echo "Dire Wolf reported a protocol error" >&2
  exit 1
fi
echo "Podman Dire Wolf integration passed (Linux and Win32/Wine nodes and transport)"
