#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
output=${1:?Usage: sh tests/package-go32.sh ABSOLUTE_OUTPUT_DIRECTORY}
case "$output" in /*) ;; *) echo "Output directory must be absolute" >&2; exit 1 ;; esac
: "${CWSDPMI_ZIP:?Set CWSDPMI_ZIP to the official csdpmi7b.zip}"
compiler=${DJGPP_CC:-i586-pc-msdosdjgpp-gcc}
command -v "$compiler" >/dev/null
test "$(sha256sum "$CWSDPMI_ZIP" | cut -d ' ' -f 1)" = \
  deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c

revision=$(git rev-parse --short=12 HEAD)
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
source="$tmp/source"
package="$tmp/package"
smoke="$tmp/smoke"
mkdir -p "$output" "$source" "$package" "$smoke/TNN"
git archive HEAD | tar -C "$source" -xf -
version=$(awk '$2 == "__VER__" || $2 == "__AUTHOR__" || $2 == "__PATCH__" {
  gsub(/"/, "", $3); printf "%s", $3
}' "$source/src/version.c")
test -n "$version"
(
  cd "$source"
  zip -qr "$package/SOURCE.ZIP" .
  make -j1 bin/tnngo32 CC_GO32="$compiler"
)
cp "$source/bin/tnngo32" "$package/TNN.EXE"
cp "$source/packaging/go32/README.TXT" "$package/README.TXT"
unzip -p "$CWSDPMI_ZIP" bin/CWSDPMI.EXE > "$package/CWSDPMI.EXE"
unzip -p "$CWSDPMI_ZIP" bin/cwsdpmi.doc > "$package/CWSDPMI.DOC"
mkdir -p "$package/TEXTCMD" "$package/USEREXE" "$package/SYSEXE" "$package/PACSAT"
cp "$package/TNN.EXE" "$package/CWSDPMI.EXE" "$smoke/TNN/"
mkdir -p "$smoke/TNN/TEXTCMD" "$smoke/TNN/USEREXE" "$smoke/TNN/SYSEXE" "$smoke/TNN/PACSAT"
status=0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 25s dosbox -noconsole \
  -c "mount c $smoke" -c 'c:' -c 'set path=c:\tnn' \
  -c 'tnn\tnn.exe > tnn\run.log' -c 'exit' > "$tmp/dosbox.log" 2>&1 || status=$?
if [ "$status" -ne 124 ] || ! grep -Fq "TheNetNode (GO32) (CB), $version" "$smoke/TNN/RUN.LOG" \
  || ! grep -Fq 'password:' "$smoke/TNN/RUN.LOG"; then
  cat "$tmp/dosbox.log"
  test ! -f "$smoke/TNN/RUN.LOG" || cat "$smoke/TNN/RUN.LOG"
  echo "GO32 DOSBox startup test failed (status $status)" >&2
  exit 1
fi

archive="TNN-CB-GO32-test-${revision}.zip"
(
  cd "$package"
  zip -qr "$output/$archive" .
)
(
  cd "$output"
  sha256sum "$archive" > "$archive.sha256"
)
echo "GO32 build and DOSBox startup passed: $archive"
