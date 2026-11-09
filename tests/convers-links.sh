#!/bin/sh
set -eu
cd "${1:-$(dirname "$0")/..}"
tmp=$(mktemp -d)
trap 'rm -f "$tmp/test.c" "$tmp/test"; rmdir "$tmp"' EXIT HUP INT TERM

extract()
{
  PATTERN="$2" awk '
    BEGIN { pattern = ENVIRON["PATTERN"] }
    $0 ~ pattern { found = 1 }
    found { print }
    found && /^}/ { exit }
  ' "$1" >> "$tmp/test.c"
}

cat > "$tmp/test.c" <<'EOF'
#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
typedef short WORD;
typedef unsigned short UWORD;
typedef long LONG;
typedef enum { FALSE, TRUE } BOOLEAN;
typedef enum { ERRORS = -1, NO, YES } TRILLIAN;
#define NUL '\0'
#define L2CALEN 6
#define L2IDLEN 7
#define L2PNUM 16
#define MAXCVSHOST 10
#define NAMESIZE 16
#define OS_IPLINK
#define L1IPCONV
#define CONVERS_HOSTNAME
#define CONVERS_LINKS
#define NULLPERMLINK NULL
#define min(a, b) ((a) < (b) ? (a) : (b))
typedef struct { int unused; } CONNECTION;
typedef struct {
  char cname[NAMESIZE + 1], call[L2IDLEN], via[57], HostName[65];
  CONNECTION *connection;
  time_t statetime, waittime, testnexttime, testwaittime, retrytime;
  int tries, rxtime, txtime, locked, nlcks, TcpLink;
  UWORD port;
} PERMLINK;
typedef struct destination {
  struct destination *next;
  PERMLINK *link;
  char name[NAMESIZE];
  int rtt;
} DESTINATION;
typedef struct { char text[1024]; size_t len; } MBHEAD;
static PERMLINK *permarray[MAXCVSHOST];
static DESTINATION *destinations;
static time_t currtime = 100;
static char myhostname[] = "local";
static char nullid[L2IDLEN] = {' ', ' ', ' ', ' ', ' ', ' ', 0x60};
static char myid[L2IDLEN] = {'C', 'B', '0', 'X', 'X', ' ', 0x60};
static char *strlwr(char *s) {
  for (char *p = s; *p; ++p) *p = (char)tolower((unsigned char)*p);
  return s;
}
static char *strupr(char *s) {
  for (char *p = s; *p; ++p) *p = (char)toupper((unsigned char)*p);
  return s;
}
static void cpyid(char *dest, const char *src) {
  memcpy(dest, src, L2CALEN); dest[L2CALEN] = src[L2CALEN] & 0x7e;
}
static int cmpcal(const char *a, const char *b) {
  return !strncmp(a, b, L2CALEN);
}
static TRILLIAN fvalca(char *call) {
  return call[0] == ' ' ? NO : YES;
}
static void getdig(WORD *n, char **p, int flag, char *via) {
  (void)n; (void)p; (void)flag; *via = NUL;
}
static void IPConvAddTBL(char *call, unsigned char *ip,
                         struct hostent *he, unsigned short port, BOOLEAN link) {
  (void)call; (void)ip; (void)he; (void)port; (void)link;
  assert(!"Unexpected TCP route");
}
static BOOLEAN IPConvDelTBL(char *name) { (void)name; return TRUE; }
static void update_destinations(PERMLINK *p, char *name, time_t rtt, char *rev) {
  (void)p; (void)name; (void)rtt; (void)rev;
  assert(!"Unexpected destination update");
}
static void putchr(int c, MBHEAD *b) {
  assert(b->len + 1 < sizeof(b->text));
  b->text[b->len++] = c; b->text[b->len] = NUL;
}
static void putstr(const char *s, MBHEAD *b) {
  while (*s) putchr(*s++, b);
}
static void putprintf(MBHEAD *b, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(b->text + b->len, sizeof(b->text) - b->len, fmt, ap);
  va_end(ap);
  assert(n >= 0 && (size_t)n < sizeof(b->text) - b->len);
  b->len += n;
}
static void putnum(int n, MBHEAD *b) { putprintf(b, "%d", n); }
EOF
extract src/l7utils.c '^BOOLEAN skipsp\('
extract src/callstr.c '^callss2str\('
# callss2str and getcal have their return types on the preceding line.
sed -i '/^callss2str(/i void' "$tmp/test.c"
extract src/l7utils.c '^getcal\('
sed -i '/^getcal(/i TRILLIAN' "$tmp/test.c"
extract src/l7utils.c '^UWORD nxtnum\('
extract src/l7utils.c '^LONG nxtlong\('
extract src/l1ipconv.c '^BOOLEAN IPConvGetName\('
extract src/l1ipconv.c '^BOOLEAN IPConvGetIP\('
extract src/cvs_cmds.c '^static BOOLEAN scan_linksarg\('
extract src/cvs_cvsd.c '^PERMLINK \*update_permlinks\('
extract src/l7utils.c '^void putid\('
extract src/l7utils.c '^void dump_convc\('
cat >> "$tmp/test.c" <<'EOF'
int main(void) {
  char input[] = "+ cb0hk", hostname[17] = {0}, call[L2IDLEN], via[57];
  unsigned char ip[65] = {0};
  WORD act = 0, port = 0;
  assert(scan_linksarg(input, hostname, &act, call, via, &port, ip));
  assert(!strcmp(hostname, "cb0hk") && act == 0 && port == 255);
  assert(!memcmp(call, "CB0HK \140", L2IDLEN) && !via[0] && !ip[0]);

  PERMLINK *first = update_permlinks(hostname, NULL, 1);
  assert(first && permarray[0] == first);
  assert(!memcmp(first->call, "CB0HK \140", L2IDLEN));
  cpyid(first->call, call);
  first->port = port;
  PERMLINK *second = update_permlinks("cb0xyz", NULL, 1);
  assert(second && second != first && permarray[1] == second);
  assert(permarray[0] == first);
  assert(!memcmp(second->call, "CB0XYZ\140", L2IDLEN));
  second->port = 255;
  assert(update_permlinks("cb0hk", NULL, 0) == first);

  MBHEAD b = {{0}, 0};
  dump_convc(&b);
  assert(!strcmp(b.text, ";\r; Convers Interlinks\r;\r"
                        "CONV HOSTNAME local\r"
                        "CONV C cb0hk CB0HK\r"
                        "CONV C cb0xyz CB0XYZ\r"));
  char explicit_call[] = "+ cb0hk CB0HK-2";
  assert(scan_linksarg(explicit_call, hostname, &act, call, via, &port, ip));
  assert(!memcmp(call, "CB0HK \144", L2IDLEN) && port == 255);
  free(first); free(second);
  puts("Convers link parsing and saved configuration regressions passed");
}
EOF
${CC:-cc} -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter \
  -funsigned-char -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
