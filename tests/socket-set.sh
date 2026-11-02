#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/test"; rmdir "$tmp"' EXIT HUP INT TERM
cat > "$tmp/test.c" <<'EOF'
#include <assert.h>
#include <string.h>
#define FD_SETSIZE 4
typedef unsigned int Socklen_t;
typedef struct {
  unsigned int fd_count;
  Socklen_t fd_array[FD_SETSIZE];
} Fd_set;
EOF
awk '/^#define FD_SET_T/ {p=1} p {print} p && /^#define FD_ZERO_T/ {exit}' \
  include/l3sock.h >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
int main(void)
{
  Fd_set set;
  memset(&set, 0xff, sizeof(set));
  FD_ZERO_T(&set);
  assert(set.fd_count == 0);
  for (unsigned i = 0; i < FD_SETSIZE; ++i)
    assert(set.fd_array[i] == 0);
  FD_SET_T(7, &set);
  FD_SET_T(7, &set);
  assert(set.fd_count == 1 && set.fd_array[0] == 7);
  FD_SET_T(0, &set);
  FD_SET_T(2, &set);
  FD_SET_T(3, &set);
  FD_SET_T(9, &set);
  assert(set.fd_count == FD_SETSIZE);
  assert(set.fd_array[1] == 0 && set.fd_array[3] == 3);
  FD_ZERO_T(&set);
  FD_SET_T(42, &set);
  assert(set.fd_count == 1 && set.fd_array[0] == 42);
  return 0;
}
EOF
${CC:-cc} -Wall -Wextra -Werror -Wmaybe-uninitialized \
  -fsanitize=address,undefined -g "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
echo "Socket-set regression passed"
