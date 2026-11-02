#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/test"; rmdir "$tmp"' EXIT HUP INT TERM
cat > "$tmp/test.c" <<'EOF'
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define huge
#define NUL 0
#define ALLOC_MBHEAD 0
#define L2CNOIM 1
#define MONF 16
#define MONT 32
#define MONL 64
#define HMRMONH 4
#define HMRMONIH 5
#define HMRMONI 6
#define US_CCP 0
typedef unsigned short UWORD;
typedef struct MBHEAD {
  char *mbbp;
  unsigned mbgc, mbpc;
  int tx, repeated, type;
  char data[512];
} MBHEAD;
typedef MBHEAD LEHEAD;
typedef struct { int Mpar; } MONBUF;
typedef struct USRBLK {
  struct USRBLK *unext;
  int status;
  MONBUF *monitor;
} USRBLK;
static MONBUF consmon;
static struct { void *head; } usccpl;
static struct { void *tail; } smonfl;
static USRBLK *userpo;
static int tracnt, monlin, ishmod, rxfctl, sys_time, tic10;
static unsigned decoded_length;
static MBHEAD *queue[2];
static unsigned queued;
static MBHEAD *allocb(int kind) {
  MBHEAD *p = calloc(1, sizeof(*p));
  (void)kind;
  assert(p);
  p->mbbp = p->data;
  return p;
}
static MBHEAD *getmbp(void) { return allocb(0); }
static void dealmb(MBHEAD *p) { free(p); }
static unsigned char getchr(MBHEAD *p) {
  assert(p->mbgc < p->mbpc);
  p->mbgc++;
  return (unsigned char)*p->mbbp++;
}
static void putchr(unsigned char c, MBHEAD *p) {
  assert(p->mbpc < sizeof(p->data));
  p->data[p->mbpc++] = c;
}
static void rwndmb(MBHEAD *p) { p->mbgc = 0; p->mbbp = p->data; }
static void putmb(MBHEAD *dst, MBHEAD *src) {
  rwndmb(src);
  while (src->mbgc < src->mbpc) putchr(getchr(src), dst);
}
static void frimon(MBHEAD *dst, MBHEAD *src) {
  while (src->mbgc < src->mbpc) putchr(getchr(src), dst);
}
static void frhmon(MBHEAD *dst, MBHEAD *src) {
  (void)dst;
  if (!(rxfctl & L2CNOIM)) getchr(src);
}
static void nethmon(MBHEAD *dst, MBHEAD *src) {
  unsigned i;
  (void)dst;
  for (i = 0; i < decoded_length; i++) getchr(src);
}
static int ismonf(MBHEAD *p, MONBUF *m) { (void)p; (void)m; return 1; }
static void relink(LEHEAD *p, LEHEAD *tail) {
  (void)tail;
  assert(queued < 2);
  queue[queued++] = p;
}
static void statsv(int a, int b, int c, int d) {
  (void)a; (void)b; (void)c; (void)d;
}
static void putstr(const char *s, MBHEAD *p) { (void)s; (void)p; }
static void puttim(int *t, MBHEAD *p) { (void)t; (void)p; }
static void putlong(int n, int b, MBHEAD *p) { (void)n; (void)b; (void)p; }
static void putprintf(MBHEAD *p, const char *f, ...) { (void)p; (void)f; }
static int send_msg(int b, MBHEAD *p) { (void)b; (void)p; return 0; }
#define FALSE 0
EOF
awk '/^void monitor\(MBHEAD \*fbp\)/ { copy=1 }
     copy && /^static BOOLEAN/ { exit }
     copy { print }' "${1:-src/l7moni.c}" >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
static void check(const char *name, unsigned length, unsigned decoded,
                  int host, int full, int supervisory) {
  MBHEAD input = {0};
  unsigned i, expected = host ? length : length - decoded;
  input.data[0] = (char)0xcf;
  for (i = 0; i < length; i++) input.data[i + 1] = (char)(i + 1);
  input.mbbp = input.data;
  input.mbpc = length + (supervisory ? 0 : 1);
  decoded_length = decoded;
  ishmod = host;
  rxfctl = supervisory ? L2CNOIM : 0;
  consmon.Mpar = full ? MONF : 1;
  queued = monlin = 0;
  monitor(&input);
  if (!full || supervisory) expected = 0;
  assert(queued == (expected ? 2u : 1u));
  assert(queue[0]->type == (expected ? HMRMONIH : HMRMONH));
  if (expected) {
    assert(queue[1]->type == HMRMONI);
    assert(queue[1]->mbpc == (expected > 256 ? 256 : expected));
    assert(!memcmp(queue[1]->data, input.data + 1 + (host ? 0 : decoded),
                   queue[1]->mbpc));
  }
  assert(input.mbgc == 0 && input.mbbp == input.data);
  for (i = 0; i < queued; i++) free(queue[i]);
  puts(name);
}
int main(void) {
  check("ConReq", 35, 35, 1, 1, 0);
  check("ConAck", 21, 21, 1, 1, 0);
  check("DiscReq", 20, 20, 1, 1, 0);
  check("DiscAck", 20, 20, 1, 1, 0);
  check("InfoAck", 20, 20, 1, 1, 0);
  check("Info with payload", 27, 20, 1, 1, 0);
  check("Plain I/UI payload", 12, 0, 1, 1, 0);
  check("Hostmode length limit", 300, 20, 1, 1, 0);
  check("Header-only monitor", 20, 20, 1, 0, 0);
  check("Supervisory frame", 0, 0, 1, 1, 1);
  check("Empty information field", 0, 0, 1, 1, 0);
  check("Text console control frame", 20, 20, 0, 1, 0);
  check("Text console payload", 27, 20, 0, 1, 0);
  return 0;
}
EOF
${CC:-cc} -std=c99 -Wall -Wextra -Werror "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
