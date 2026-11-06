#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/test"; rmdir "$tmp"' EXIT HUP INT TERM

extract()
{
  return_type=$1
  function_name=$2
  file=$3
  awk -v return_type="$return_type" -v function_name="$function_name" '
    $0 == function_name "(void)" {
      print return_type
      copy = 1
    }
    copy {
      print
      if ($0 == "}") exit
    }
  ' "$file"
}

cat > "$tmp/test.c" <<'EOF'
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define FALSE 0
#define HMSCHNL 0
#define CONS_SOCKET_CONNECTED 5
#define CONS_SOCKET_WAITING 4
typedef int BOOLEAN;
typedef short WORD;
typedef unsigned char UBYTE;

struct hostqueue {
  struct hostqueue *next;
};

static struct hostqueue *hostq_root;
static struct hostqueue *hostq_last;
static int hostq_len;
static int consockfd = -1;
static UBYTE console_type;

BOOLEAN ishmod;
int console_login_status;
WORD actch;
BOOLEAN hlixfl;
int incnt;
char hinbuf[256];
char *inbufp;
char *blipoi;
WORD blicnt;
static int hmstat;
static int hmch;
static int hmcmd;
static int hmlen;
EOF
extract void reset_hostmode src/l7host.c >> "$tmp/test.c"
extract "static void" clear_host_queue os/linux/linux.c >> "$tmp/test.c"
extract "static void" disconnect_socket_console os/linux/linux.c >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
int main(void)
{
  int sockets[2];
  struct hostqueue *first = calloc(1, sizeof(*first));
  struct hostqueue *second = calloc(1, sizeof(*second));
  char byte;

  assert(first != NULL && second != NULL);
  first->next = second;
  hostq_root = first;
  hostq_last = second;
  hostq_len = 2;
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
  consockfd = sockets[0];
  console_type = CONS_SOCKET_CONNECTED;
  ishmod = 1;
  console_login_status = 1;
  actch = 7;
  hlixfl = 1;
  incnt = 12;
  inbufp = hinbuf + 12;
  blipoi = hinbuf + 4;
  blicnt = 8;
  hmstat = 3;
  hmch = 7;
  hmcmd = 1;
  hmlen = 42;

  disconnect_socket_console();

  assert(consockfd == -1);
  assert(console_type == CONS_SOCKET_WAITING);
  assert(hostq_root == NULL && hostq_last == NULL && hostq_len == 0);
  assert(!ishmod && !console_login_status);
  assert(actch == 1 && !hlixfl && incnt == 0);
  assert(inbufp == hinbuf && blipoi == hinbuf && blicnt == 0);
  assert(hmstat == HMSCHNL && hmch == 0 && hmcmd == 0 && hmlen == 0);
  assert(read(sockets[1], &byte, 1) == 0);
  close(sockets[1]);
  return 0;
}
EOF

calls=$(grep -c 'disconnect_socket_console();' os/linux/linux.c)
test "$calls" -eq 2
${CC:-cc} -std=c99 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -g "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
echo "Hostmode reconnect regression passed"
