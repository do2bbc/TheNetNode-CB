#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
output=${1:?Usage: sh tests/package-win32.sh ABSOLUTE_OUTPUT_DIRECTORY}
case "$output" in /*) ;; *) echo "Output directory must be absolute" >&2; exit 1 ;; esac
mkdir -p "$output"
revision=$(git rev-parse --short=12 HEAD)
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
source="$tmp/source"
package="$tmp/TNN-CB-Win32-XP-test"
mkdir "$source" "$package"
# Only committed sources belong in a release, never private working files.
git archive HEAD | tar -C "$source" -xf -
cd "$source"
grep -q '^#define CALLCHECK$' include/allmodif.h
grep -q '^#define HOSTMYCALL$' include/allmodif.h
version=$(awk '$2 == "__VER__" || $2 == "__AUTHOR__" || $2 == "__PATCH__" {
  gsub(/"/, "", $3); printf "%s", $3
}' src/version.c)
test -n "$version"
zip -qr "$package/SOURCE.zip" .
make -f tnn32.mak all PREFIX=i686-w64-mingw32- -j2
sh tests/win32-build.sh
sh tests/gcc-regressions.sh
sh tests/runtime-regressions.sh
sh tests/startup-banner.sh
sh tests/socket-set.sh
sh tests/monitor-hostmode.sh
sh tests/hostmode-reconnect.sh
i686-w64-mingw32-gcc -DWIN32 -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 \
  -Iinclude -Ios/win32 -Ios/linux -Isrc -static \
  -Wl,--subsystem,console:5.01 tests/win32-diagnostics.c \
  os/win32/diagnostics.c -o "$tmp/diagnostics-test.exe"
mkdir "$tmp/diagnostics"
cd "$tmp/diagnostics"
export WINEPREFIX="$tmp/wine" WINEDEBUG=-all
timeout 90 wine "$tmp/diagnostics-test.exe" clean
grep -q 'event=CLEAN_EXIT' tnn-runtime.log
timeout 20 wine "$tmp/diagnostics-test.exe" error
grep -q 'event=ERROR_EXIT.*code=0x00000007' tnn-runtime.log
grep -q 'event=CLEAN_EXIT' tnn-runtime.previous.log
timeout 20 wine "$tmp/diagnostics-test.exe" unclean
grep -q 'event=EXIT_WITHOUT_CLEAN_SHUTDOWN' tnn-runtime.log
result=0
timeout 20 wine "$tmp/diagnostics-test.exe" signal || result=$?
test "$result" -eq 139
grep -q 'event=FATAL_C_SIGNAL' tnn-crash.log
result=0
timeout 20 wine "$tmp/diagnostics-test.exe" exception || result=$?
test "$result" -ne 0
test "$result" -ne 124
grep -q 'event=UNHANDLED_EXCEPTION.*code=0xe042544e' tnn-crash.log
grep -q 'event=FATAL_C_SIGNAL' tnn-crash.previous.log
timeout 90 wine "$tmp/diagnostics-test.exe" heartbeat
grep -q 'event=HEARTBEAT' tnn-runtime.log
mkdir "$tmp/node-test"
cp "$source"/os/win32/portable/* "$tmp/node-test/"
cd "$tmp/node-test"
result=0
timeout 68 wine "$source/os/win32/tnn32/tnn32.exe" -v > console.log 2>&1 || result=$?
test "$result" -eq 124
grep -q '(Win32) (CB)' console.log
grep -Fq "$version" console.log
grep -q 'password:' console.log
grep -q 'event=HEARTBEAT' tnn-runtime.log
test ! -s tnn-crash.log
cd "$source"
mkdir -p "$package/USEREXE" "$package/DOC" "$package/TEXTCMD" \
  "$package/SYSEXE" "$package/PACSAT" "$package/HTTPD" \
  "$package/SPEECH" "$package/MSG"
cp os/win32/portable/* "$package/"
cp os/win32/tnn32/tnn32.exe "$package/"
cp os/win32/tnn32/help.exe os/win32/tnn32/msg.exe \
  os/win32/tnn32/msy.exe os/win32/tnn32/top.exe "$package/USEREXE/"
cp alas.txt "$package/ALAS.txt"
cp doc/tnn178.pdf doc/tnn179cb.pdf "$package/DOC/"
cp contrib/onlhelp/ohu.txt contrib/onlhelp/ohs.txt "$package/"
printf 'Repository: https://github.com/do2bbc/TheNetNode-CB\nSource commit: %s\nTarget: Win32 CB, XP hardware/ports unverified\n' \
  "$(git -C "$root" rev-parse HEAD)" > "$package/BUILD.txt"
perl -pi -e 's/\r?\n/\r\n/g' "$package/README.txt" "$package/tnn.ini" \
  "$package/TNN179.PAS" "$package/TNN179.TNB" "$package/START-TNN.cmd"
cd "$package"
sha256sum tnn32.exe USEREXE/*.exe SOURCE.zip > SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
cd "$tmp"
archive="TNN-CB-Win32-XP-test-$revision.zip"
zip -qr "$output/$archive" TNN-CB-Win32-XP-test
cd "$output"
unzip -tq "$archive"
sha256sum "$archive" > "$archive.sha256"
