#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
output=${1:?Usage: sh tests/package-win64.sh ABSOLUTE_OUTPUT_DIRECTORY}
case "$output" in /*) ;; *) echo "Output directory must be absolute" >&2; exit 1 ;; esac
mkdir -p "$output"
revision=$(git rev-parse --short=12 HEAD)
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
source="$tmp/source"
package="$tmp/TNN-CB-Win64-experimental"
mkdir "$source" "$package"
# Only committed sources belong in a release, never private working files.
git archive HEAD | tar -C "$source" -xf -
cd "$source"
version=$(awk '$2 == "__VER__" || $2 == "__AUTHOR__" || $2 == "__PATCH__" {
  gsub(/"/, "", $3); printf "%s", $3
}' src/version.c)
test -n "$version"
zip -qr "$package/SOURCE.zip" .
make -f tnn64.mak all -j2
sh tests/win64-build.sh
mkdir -p "$package/USEREXE" "$package/DOC" "$package/TEXTCMD" \
  "$package/SYSEXE" "$package/PACSAT" "$package/HTTPD" \
  "$package/SPEECH" "$package/MSG"
cp os/win32/portable/TNN179.PAS os/win32/portable/TNN179.TNB \
  os/win32/portable/tnn.ini "$package/"
cp packaging/win64/README.txt packaging/win64/START-TNN.cmd "$package/"
cp os/win32/tnn64/tnn64.exe "$package/"
cp os/win32/tnn64/help.exe os/win32/tnn64/msg.exe \
  os/win32/tnn64/msy.exe os/win32/tnn64/top.exe "$package/USEREXE/"
cp alas.txt "$package/ALAS.txt"
cp doc/tnn178.pdf doc/tnn179cb.pdf "$package/DOC/"
cp contrib/onlhelp/ohu.txt contrib/onlhelp/ohs.txt "$package/"
printf 'Repository: https://github.com/do2bbc/TheNetNode-CB\nSource commit: %s\nTarget: experimental Win64 package, Windows 7 x64 and AXIP/UDP tested; physical modem testing is external.\n' \
  "$(git -C "$root" rev-parse HEAD)" > "$package/BUILD.txt"
perl -pi -e 's/\r?\n/\r\n/g' "$package/README.txt" "$package/tnn.ini" \
  "$package/TNN179.PAS" "$package/TNN179.TNB" "$package/START-TNN.cmd"
cd "$package"
sha256sum tnn64.exe USEREXE/*.exe SOURCE.zip > SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
cd "$tmp"
archive="TNN-CB-Win64-experimental-$revision.zip"
zip -qr "$output/$archive" TNN-CB-Win64-experimental
cd "$output"
unzip -tq "$archive"
sha256sum "$archive" > "$archive.sha256"
