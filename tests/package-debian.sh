#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
arch=${1:?Usage: sh tests/package-debian.sh amd64|arm64|armhf ABSOLUTE_OUTPUT}
output=${2:?Missing absolute output directory}
case "$output" in /*) ;; *) echo "Output path must be absolute" >&2; exit 1 ;; esac
case "$arch" in
  amd64) compiler=x86_64-linux-gnu-gcc; machine='Advanced Micro Devices X86-64' ;;
  arm64) compiler=aarch64-linux-gnu-gcc; machine=AArch64 ;;
  armhf) compiler=arm-linux-gnueabihf-gcc; machine=ARM ;;
  *) echo "Unsupported architecture: $arch" >&2; exit 1 ;;
esac
test "$(dpkg --print-architecture)" = amd64
test "$(getconf GNU_LIBC_VERSION)" = 'glibc 2.36'
revision=$(git rev-parse --short=12 HEAD)
export SOURCE_DATE_EPOCH=$(git show -s --format=%ct HEAD)
version=$(awk '$2 == "__VER__" || $2 == "__AUTHOR__" || $2 == "__PATCH__" {
  gsub(/"/, "", $3); printf "%s", $3
}' src/version.c)
debversion="${version}+git$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y%m%d).$revision-1"
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
mkdir -p "$output" "$tmp/source" "$tmp/package"
git archive HEAD | tar -C "$tmp/source" -xf -
source="$tmp/source"
package="$tmp/package"
doc="$package/usr/share/doc/tnn-cb"
mkdir -p "$package/DEBIAN" "$package/usr/bin" "$doc/examples" \
  "$package/usr/lib/tnn-cb/userexe" "$package/usr/lib/tnn-cb/sysexe" \
  "$package/usr/share/tnn-cb"
git archive --format=tar HEAD | gzip -n > "$doc/SOURCE.tar.gz"
cd "$source"
make -j2 tnn help msg msy top CC="$compiler" HOSTCC=gcc \
  TNNDIR=/var/lib/tnn-cb/ INSTCFG=/var/lib/tnn-cb/
install -m 755 bin/tnn "$package/usr/bin/tnn-cb"
for helper in help msg top; do
  install -m 755 "bin/$helper" "$package/usr/lib/tnn-cb/userexe/$helper"
done
install -m 755 bin/msy "$package/usr/lib/tnn-cb/sysexe/msy"
ln -s ../userexe/help "$package/usr/lib/tnn-cb/sysexe/syshelp"
for binary in "$package/usr/bin/tnn-cb" \
              "$package"/usr/lib/tnn-cb/userexe/* \
              "$package/usr/lib/tnn-cb/sysexe/msy"; do
  LC_ALL=C readelf -h "$binary" | grep -F "Machine:" | grep -Fq "$machine"
  LC_ALL=C readelf -d "$binary" | awk '
    /NEEDED/ {
      if ($NF != "[libc.so.6]") {
        print "Unexpected shared library: " $NF > "/dev/stderr"; exit 1
      }
      count++
    }
    END { if (!count) exit 1 }
  '
done
if [ "$arch" = armhf ]; then
  readelf -A "$package/usr/bin/tnn-cb" | grep -q 'Tag_CPU_arch: v7'
  readelf -A "$package/usr/bin/tnn-cb" | grep -q 'Tag_ABI_VFP_args: VFP registers'
fi
cp packaging/debian/tnn.ini packaging/debian/tnn179.pas \
  packaging/debian/tnn179.tnb "$doc/examples/"
cp packaging/debian/README.Debian "$doc/"
cp alas.txt "$doc/copyright"
cp doc/tnn178.pdf doc/tnn179cb.pdf "$doc/"
cp contrib/onlhelp/ohu.txt contrib/onlhelp/ohs.txt \
  contrib/msgmsy/msg.usr contrib/msgmsy/msg.sys contrib/top/config.top \
  "$package/usr/share/tnn-cb/"
printf 'Repository: https://github.com/do2bbc/TheNetNode-CB\nSource commit: %s\nTarget: Debian 12 %s\n' \
  "$(git -C "$root" rev-parse HEAD)" "$arch" > "$doc/BUILD.txt"
cat > "$package/DEBIAN/control" <<EOF
Package: tnn-cb
Version: $debversion
Architecture: $arch
Maintainer: TheNetNode-CB contributors <tnn-cb@users.noreply.github.com>
Section: hamradio
Priority: optional
Depends: libc6 (>= 2.36)
Installed-Size: $(du -sk "$package/usr" | cut -f1)
Homepage: https://github.com/do2bbc/TheNetNode-CB
Description: TheNetNode CB packet-radio node and helper programs
 Native CB source tree with offline example configuration.
 No service or live configuration is installed or started automatically.
EOF
cd "$package"
find usr -type f -print0 | LC_ALL=C sort -z | xargs -0 md5sum > DEBIAN/md5sums
archive="tnn-cb_${debversion}_${arch}.deb"
dpkg-deb --root-owner-group --build "$package" "$output/$archive"
test "$(dpkg-deb -f "$output/$archive" Architecture)" = "$arch"
cd "$output"
sha256sum "$archive" > "$archive.sha256"
printf 'Debian package built: %s\n' "$archive"
