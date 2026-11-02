#!/bin/sh
set -eu
cd "${1:-$(dirname "$0")/..}"
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/test"; rmdir "$tmp"' EXIT HUP INT TERM
cat > "$tmp/test.c" <<'EOF'
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define L2PNUM 16
#define LINKNMBR 400
#define NUMCIR 200
#define REV "$Revision: 3.14 $"
static char output[2048];
static void hputs(const char *text)
{
  assert(strlen(output) + strlen(text) < sizeof(output));
  strcat(output, text);
}
EOF
awk '/^char author\[\]/ {p=1} p {print} p && /;$/ {exit}' \
  src/version.c >> "$tmp/test.c"
printf 'void\n' >> "$tmp/test.c"
awk '/^hprintf\(const char \*format/ {p=1} p {print} p && /^}/ {exit}' \
  src/l7host.c >> "$tmp/test.c"
printf 'int main(void)\n{\n' >> "$tmp/test.c"
awk '
  /^static void mainf\(void\)/ {in_main=1}
  in_main && /^  hprintf\("\)/ {p=1}
  p && /^#ifdef AXIPR_UDP/ {exit}
  p {print}
' src/main.c >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
  assert(strstr(output, "16 Ports"));
  assert(strstr(output, "400 L2-Links and 200 Circuits"));
#ifdef PPCONVERS
  assert(strstr(output, "continued by DO2BBC"));
  assert(strstr(output, "https://github.com/do2bbc/TheNetNode-CB"));
#endif
  puts("Startup banner passed");
  return 0;
}
EOF
for flags in '' '-DPPCONVERS'; do
  ${CC:-cc} -Wall -Werror -fstack-protector-all \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g $flags \
    "$tmp/test.c" -o "$tmp/test"
  "$tmp/test"
done
