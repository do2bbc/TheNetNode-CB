#!/bin/sh
set -eu
cd "${1:-$(dirname "$0")/..}"
tmp=$(mktemp -d)
trap 'rm -f "$tmp/tcp.c" "$tmp/tcp" "$tmp/convers.c" "$tmp/convers"; rmdir "$tmp"' EXIT HUP INT TERM

extract()
{
  PATTERN="$2" awk '
    BEGIN { pattern = ENVIRON["PATTERN"] }
    $0 ~ pattern { found = 1 }
    found { print }
    found && /^}/ { exit }
  ' "$1"
}

cat > "$tmp/tcp.c" <<'EOF'
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef enum { FALSE, TRUE } BOOLEAN;
typedef enum { ERRORS = -1, NO, YES } TRILLIAN;
#define TXLEN 2048
#define TCP_TX_FREE 0
#define TCP_TX_BUSY 1
typedef struct MBHEAD {
  unsigned mbgc, mbpc;
  char data[4096];
} MBHEAD;
typedef struct SENDTX {
  struct SENDTX *next;
  MBHEAD *Data;
  int Sock, Interface, Mode;
} SENDTX;
typedef SENDTX LEHEAD;
typedef struct {
  int sock, Interface, disflg, txstatus, login, txc, sum;
  char txbuf[TXLEN + 1], ip[16];
} TCPIP;
static TCPIP state[2], *tcppoi;
static struct { SENDTX *head; } rxflTX;
static void *ifpp;
static int freed_data, freed_segments, log_count, real_socket;
enum { READY, BLOCKED, PARTIAL, AGAIN, INTERRUPTED, BROKEN, ZERO, BAD_SELECT,
       INTERRUPTED_SELECT };
static int mode[2];
static char output[2][16384];
static size_t output_len[2];
static int test_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
  assert(tv && tv->tv_sec == 0 && tv->tv_usec == 0);
  if (real_socket)
    return select(n, r, w, e, tv);
  tv->tv_usec = 1; /* A subsequent call must initialize its own timeout. */
  if (mode[tcppoi->sock] == BAD_SELECT) { errno = EBADF; return -1; }
  if (mode[tcppoi->sock] == INTERRUPTED_SELECT) { errno = EINTR; return -1; }
  return mode[tcppoi->sock] == BLOCKED ? 0 : 1;
}
static ssize_t test_send(int fd, const void *buf, size_t count, int flags)
{
  if (real_socket)
    return send(fd, buf, count, flags);
  if (mode[fd] == AGAIN) { errno = EAGAIN; return -1; }
  if (mode[fd] == INTERRUPTED) { errno = EINTR; return -1; }
  if (mode[fd] == BROKEN) { errno = EPIPE; return -1; }
  if (mode[fd] == ZERO) return 0;
  if (mode[fd] == PARTIAL && count > 3) count = 3;
  assert(output_len[fd] + count <= sizeof(output[fd]));
  memcpy(output[fd] + output_len[fd], buf, count);
  output_len[fd] += count;
  return count;
}
static TCPIP *FdReadSockTCP(int sock, int interface, int stack)
{
  (void)interface; (void)stack;
  return sock < 0 ? NULL : &state[sock];
}
static void *SetInterface(int interface) { (void)interface; return NULL; }
#define T_LOGL1(...) ((void)ifpp, ++log_count)
#define T_LOGL3(...) ((void)0)
#define select test_select
#define send test_send
EOF
extract os/linux/ostcpip.c '^int PutflushSockOS\(void\)' >> "$tmp/tcp.c"
cat >> "$tmp/tcp.c" <<'EOF'
#undef select
#undef send
static BOOLEAN PutflushSockTCP(void) { return PutflushSockOS(); }
static void MhUpdateTCP(MBHEAD *b, BOOLEAN v) { (void)b; (void)v; }
static int getchr(MBHEAD *b) {
  assert(b->mbgc < b->mbpc);
  return (unsigned char)b->data[b->mbgc++];
}
static void dealmb(MBHEAD *b) { ++freed_data; free(b); }
static LEHEAD *ulink(LEHEAD *b) {
  SENDTX **p = &rxflTX.head;
  while (*p != b) {
    assert(*p != (SENDTX *)&rxflTX);
    p = &(*p)->next;
  }
  *p = b->next;
  return b;
}
static void dealoc(MBHEAD *b) { ++freed_segments; free(b); }
EOF
extract src/l1tcpip.c '^static (BOOLEAN|TRILLIAN) PutvSockTCP\(' >> "$tmp/tcp.c"
extract src/l1tcpip.c '^void TcpipTX\(void\)' >> "$tmp/tcp.c"
cat >> "$tmp/tcp.c" <<'EOF'
static void reset(void) {
  memset(state, 0, sizeof(state));
  memset(mode, 0, sizeof(mode));
  memset(output_len, 0, sizeof(output_len));
  for (int i = 0; i < 2; ++i) { state[i].sock = i; state[i].login = TRUE; }
  rxflTX.head = (SENDTX *)&rxflTX;
  freed_data = freed_segments = log_count = 0;
}
static SENDTX *enqueue(int fd, const char *data, size_t len) {
  SENDTX *p = calloc(1, sizeof(*p));
  assert(p);
  p->Data = calloc(1, sizeof(*p->Data));
  assert(p->Data && len <= sizeof(p->Data->data));
  memcpy(p->Data->data, data, len);
  p->Data->mbpc = len;
  p->Sock = fd;
  SENDTX **tail = &rxflTX.head;
  while (*tail != (SENDTX *)&rxflTX) tail = &(*tail)->next;
  *tail = p;
  p->next = (SENDTX *)&rxflTX;
  return p;
}
int main(void) {
  reset();
  memset(state[0].txbuf, 'a', TXLEN);
  state[0].txc = TXLEN;
  mode[0] = BROKEN;
  enqueue(0, "x", 1);
  enqueue(1, "next", 4);
  TcpipTX();
  assert(freed_data == 2 && freed_segments == 2);
  assert(state[0].disflg && output_len[1] == 4);
  assert(!memcmp(output[1], "next", 4));
  assert(log_count == 1);

  reset();
  memset(state[0].txbuf, 'a', TXLEN);
  state[0].txc = TXLEN;
  mode[0] = BLOCKED;
  SENDTX *p = enqueue(0, "xyz", 3);
  enqueue(1, "other", 5);
  TcpipTX();
  assert(rxflTX.head == p && p->Data->mbgc == 0);
  assert(state[0].txc == TXLEN && !state[0].disflg);
  assert(output_len[1] == 5 && freed_segments == 1);
  TcpipTX();
  assert(p->Data->mbgc == 0 && freed_segments == 1);
  mode[0] = READY;
  tcppoi = &state[0];
  assert(!PutflushSockOS());
  TcpipTX();
  assert(freed_segments == 2 && output_len[0] == TXLEN + 3);
  for (int i = 0; i < TXLEN; ++i) assert(output[0][i] == 'a');
  assert(!memcmp(output[0] + TXLEN, "xyz", 3));

  reset();
  mode[0] = PARTIAL;
  enqueue(0, "abcdefgh", 8);
  TcpipTX();
  assert(freed_segments == 1 && state[0].sum == 3 && state[0].txc == 8);
  tcppoi = &state[0];
  assert(!PutflushSockOS() && state[0].sum == 6);
  assert(!PutflushSockOS() && state[0].txc == 0);
  assert(output_len[0] == 8 && !memcmp(output[0], "abcdefgh", 8));

  reset();
  char payload[4096];
  for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = 'a' + i % 26;
  mode[0] = PARTIAL;
  enqueue(0, payload, sizeof(payload));
  int rounds = 0;
  while (rxflTX.head != (SENDTX *)&rxflTX || state[0].txc) {
    assert(++rounds < 4000);
    TcpipTX();
    if (state[0].txstatus == TCP_TX_BUSY) {
      tcppoi = &state[0];
      assert(!PutflushSockOS());
    }
  }
  assert(output_len[0] == sizeof(payload));
  assert(!memcmp(output[0], payload, sizeof(payload)));
  assert(freed_segments == 1 && freed_data == 1);

  const int temporary[] = { AGAIN, INTERRUPTED, INTERRUPTED_SELECT, BLOCKED };
  for (unsigned i = 0; i < sizeof(temporary) / sizeof(*temporary); ++i) {
    reset();
    mode[0] = temporary[i];
    enqueue(0, "retry", 5);
    TcpipTX();
    assert(!state[0].disflg && state[0].txstatus == TCP_TX_BUSY);
    assert(state[0].txc == 5 && state[0].sum == 0 && log_count == 0);
    mode[0] = READY;
    tcppoi = &state[0];
    assert(!PutflushSockOS());
    assert(output_len[0] == 5 && !memcmp(output[0], "retry", 5));
  }
  const int fatal[] = { BROKEN, ZERO, BAD_SELECT };
  for (unsigned i = 0; i < sizeof(fatal) / sizeof(*fatal); ++i) {
    reset();
    mode[0] = fatal[i];
    enqueue(0, "failed", 6);
    TcpipTX();
    assert(state[0].disflg && log_count == 1);
    assert(freed_segments == 1 && freed_data == 1);
  }
  reset();
  enqueue(-1, "gone", 4);
  TcpipTX();
  assert(freed_segments == 1 && freed_data == 1 && log_count == 1);

  /* Actual select/send with a local socket pair, never a remote node. */
  reset();
  int sockets[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
  state[0].sock = sockets[0];
  tcppoi = &state[0];
  memcpy(state[0].txbuf, "local", 5);
  state[0].txc = 5;
  real_socket = 1;
  assert(!PutflushSockOS());
  char received[5];
  assert(recv(sockets[1], received, sizeof(received), MSG_WAITALL) == 5);
  assert(!memcmp(received, "local", 5));
  close(sockets[0]); close(sockets[1]);
  puts("TCP runtime regressions passed");
}
EOF

cat > "$tmp/convers.c" <<'EOF'
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRUE 1
#define FALSE 0
#define NUL '\0'
#define MAXCHANNEL 32767
#define US_UPWD 16
#define US_CHST 17
#define CT_UNKNOWN 0
#define ALLOC_USRBLK1 0
#define L2IDLEN 7
#define NULLCONNECTION NULL
#define OS_IPLINK
typedef int BOOLEAN;
typedef struct USRBLK USRBLK;
typedef struct CONNECTION {
  struct CONNECTION *next;
  char name[17];
  USRBLK *up;
  int channel, type;
} CONNECTION;
struct USRBLK {
  int uid, status, sysflg, errcnt, auditlevel, convflag;
  char talkcall[8];
  void *mbhd, *monitor, *fp, *fname;
  CONNECTION *convers;
};
typedef USRBLK MBHEAD;
typedef struct { int value; } CIRBLK;
typedef struct { int unused; } NODE;
typedef struct { int unused; } PTCENT;
typedef struct {
  char cname[17], call[7];
  int TcpLink;
  CONNECTION *connection;
} PERMLINK;
static USRBLK original, *userpo = &original;
static CIRBLK circuit, *cirpoi = &circuit;
static CONNECTION *connections;
static char myid[7], cnvinbuf[128];
static int notices, deallocated;
static void appenddirect(CONNECTION *cp, const char *s) {
  (void)cp; assert(strstr(s, "***")); ++notices;
}
static void check_name(CONNECTION *cp) {
EOF
awk '
  /^  if \(userpo->status == US_UPWD\)/ {p=1}
  p && /^    strncpy\(clipoi/ {exit}
  p {print}
' src/cvs_cmds.c >> "$tmp/convers.c"
cat >> "$tmp/convers.c" <<'EOF'
  }
}
static CONNECTION *alloc_connection(void *up) {
  (void)up;
  CONNECTION *cp = calloc(1, sizeof(*cp));
  assert(cp);
  cp->next = connections; connections = cp;
  return cp;
}
static void *allocb(int kind) {
  (void)kind;
  void *p = calloc(1, sizeof(USRBLK));
  assert(p);
  return p;
}
static void dealoc(MBHEAD *p) { ++deallocated; free(p); }
#define send_proto(...) ((void)0)
static int IPConvConnect(char *call, char *id, int flag) {
  (void)call; (void)id; (void)flag;
  cirpoi = NULL;
  return TRUE; /* Exercise failure after the global context changed. */
}
EOF
# Extract the real TCP failure path including its connection/user allocation.
awk '
  /^BOOLEAN connect_cvshost\(PERMLINK \*p\)/ {p=1}
  p && /^      relink\(/ {exit}
  p {print}
' src/cvs_serv.c >> "$tmp/convers.c"
cat >> "$tmp/convers.c" <<'EOF'
    }
#endif
  }
  (void)node; (void)ptcp; (void)convcall;
  assert(!"Unexpected non-TCP path in this test");
  return FALSE;
}
int main(void) {
  CONNECTION cp = {0}, previous = {0};
  cp.channel = 42; userpo->status = US_UPWD;
  const char *invalid[] = {
    "/NAME", "/NAME ", "/NAME dno484", "/NAME dno484 -1",
    "/NAME dno484 32768", "/NAME dno484 3junk",
    "/NAME dno484 9999999999999999999999999999"
  };
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
    strcpy(cnvinbuf, invalid[i]);
    int before = notices;
    check_name(&cp);
    assert(cp.channel == 42 && notices == before + 1);
  }
  strcpy(cnvinbuf, "/NAME dno484 32767\r");
  check_name(&cp);
  assert(cp.channel == 32767);
  strcpy(cnvinbuf, "/NAME dno484 0");
  check_name(&cp);
  assert(cp.channel == 0);
  PERMLINK p = {"dno284", "DNO284", TRUE, NULL};
  connections = &previous;
  assert(!connect_cvshost(&p));
  assert(userpo == &original && cirpoi == &circuit);
  assert(connections == &previous && p.connection == NULL && deallocated == 1);
  userpo->status = 123;
  assert(original.status == 123);
  puts("Convers runtime regressions passed");
}
EOF
for test in tcp convers; do
  ${CC:-cc} -std=gnu99 -Wall -Werror -funsigned-char \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$tmp/$test.c" -o "$tmp/$test"
  "$tmp/$test"
done
