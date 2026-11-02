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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef short WORD;
typedef unsigned long ULONG;
typedef enum { FALSE, TRUE } BOOLEAN;
typedef enum { ERRORS = -1, NO, YES } TRILLIAN;
#define NUL '\0'
#define L2IDLEN 7
#define NAMESIZE 16
typedef struct {
  char name[NAMESIZE + 1], nickname[NAMESIZE + 1];
} CONNECTION;
typedef struct {
  char text[128];
  int l2port, l2fflg;
} MBHEAD;
static void putprintf(MBHEAD *p, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(p->text, sizeof(p->text), fmt, ap);
  va_end(ap);
  assert(n >= 0 && (size_t)n < sizeof(p->text));
}
static void skipsp(WORD *n, char **p)
{
  while (*n > 0 && **p == ' ') { --*n; ++*p; }
}
EOF
grep '^#define DEFAULT_PASS ' include/profil.h >> "$tmp/test.c"
grep '^#define speech_mode' include/speech.h >> "$tmp/test.c"
grep '^#define IP_FORWARDING ' include/ip.h >> "$tmp/test.c"
grep '^#define L2CIP ' include/l2.h >> "$tmp/test.c"
extract src/cvs_cmds.c '^void makeName\('
extract src/cvs_cmds.c '^static BOOLEAN host_ok\('
printf 'TRILLIAN\n' >> "$tmp/test.c"
extract src/prof_pwd.c '^GetPasswd\('
extract src/prof_pwd.c '^void PasswdKodierung\('
extract src/prof_pwd.c '^void PasswdDekodierung\('
extract src/profil.c '^void call3str\('
printf 'static TRILLIAN\n' >> "$tmp/test.c"
extract src/profil.c '^GetName\('
extract src/l7utils.c '^char \*ConnectTime\('
printf 'static void\n' >> "$tmp/test.c"
extract src/l7ccp.c '^percent \('
extract src/speech.c '^static char \*lese_meldung\('
extract src/speech.c '^static unsigned set_substitute_symbols\(.*mode\)$'
awk '/^struct speech_msg$/ { found=1 } found {print} found && /^};/ {exit}' \
  src/speech.c >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
#define MAXZEILEN 1
#define MAXPATH 4096
static struct speech_msg speech_tbl[MAXZEILEN + 1];
static int meldungen;
static char speechpath[] = "", speech[] = "test";
static FILE *xfopen(const char *path, const char *mode)
{
  (void)path;
  (void)mode;
  FILE *fp = tmpfile();
  assert(fp);
  fputs("\"value %lX\"\n", fp);
  rewind(fp);
  return fp;
}
static void speech_default(void) { assert(!"Unexpected speech fallback"); }
EOF
extract src/speech.c '^int speech_load\('
cat >> "$tmp/test.c" <<'EOF'
static void channel_filter(char *fromname, char *text, char *out)
{
  char buffer[2048], namefilt[68], *c;
  int cvd;
EOF
awk '
  /^  cvd = strcmp\(fromname, "conversd"\);/ { found=1 }
  found && /^  for \(ch = channels/ { exit }
  found { print }
' src/cvs_cvsd.c >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
  strcpy(out, namefilt);
}
static struct { int ipMode; } IPpar[1];
static int accepted;
static void forwarding(MBHEAD *mbhd)
{
EOF
awk '
  /^ip_route\(MBHEAD/ { function_found=1 }
  function_found && /^#ifdef KERNELIF/ { found=1 }
  found && /^  rxnxt =/ { exit }
  found { print }
' src/l3ip.c >> "$tmp/test.c"
cat >> "$tmp/test.c" <<'EOF'
  accepted = 1;
}
int main(void)
{
  CONNECTION con = {{0}, {0}};
  char *short_name = malloc(1);
  assert(short_name);
  makeName(&con, short_name);
  assert(short_name[0] == '\0');
  free(short_name);
  memset(con.name, 'A', NAMESIZE);
  memset(con.nickname, 'B', NAMESIZE);
  char name[2 * NAMESIZE + 2];
  makeName(&con, name);
  assert(strlen(name) == 2 * NAMESIZE + 1);
  assert(name[NAMESIZE] == ':');
  assert(host_ok("dno484") && !host_ok("1234") && !host_ok("ab/c"));

  char call[L2IDLEN + 1];
  memset(call, '?', sizeof(call));
  call3str(call, "ABC");
  assert(memcmp(call, "ABC   \0\0", sizeof(call)) == 0);
  char input[82], password[81], encoded[81], decoded[81];
  memset(input, 'A', 80);
  input[80] = '\0';
  WORD count = 80;
  char *cursor = input;
  assert(GetPasswd(&count, &cursor, sizeof(password), password) == YES);
  assert(strlen(password) == 80 && count == 0 && cursor == input + 80);
  PasswdKodierung(password, encoded);
  PasswdDekodierung(encoded, decoded);
  assert(strcmp(password, decoded) == 0);
  input[80] = 'A'; input[81] = '\0';
  count = 81; cursor = input;
  assert(GetPasswd(&count, &cursor, sizeof(password), password) == ERRORS);
  PasswdKodierung(input, encoded);
  PasswdDekodierung(input, decoded);
  assert(encoded[0] == '\0' && decoded[0] == '\0');
  char nick[17] = "old";
  cursor = "Nickname"; count = 8;
  assert(GetName(&count, &cursor, NAMESIZE, nick) == YES);
  assert(strcmp(nick, "Nickname") == 0);

  MBHEAD output = {{0}, 0, L2CIP};
  percent(1, 3, 1, 7, &output);
  assert(strcmp(output.text, "   33.3%") == 0);
  percent(1, 1, 1, 0, &output);
  assert(strcmp(output.text, "100.0%") == 0);
  percent(0, 0, 1, 0, &output);
  assert(strcmp(output.text, "0.0%") == 0);
  percent(10, 3, 1, 0, &output);
  assert(strcmp(output.text, "333.3%") == 0);
  assert(strcmp(ConnectTime(0), " ") == 0);
  assert(strcmp(ConnectTime(3661), " 1h, 1m") == 0);
  char expected[80];
  snprintf(expected, sizeof(expected), "%2lud,%2luh",
           ULONG_MAX / 86400, ULONG_MAX / 3600 % 24);
  assert(strcmp(ConnectTime(ULONG_MAX), expected) == 0);

  char line[255] = "\"a\\nb\\r\\\"c\\\\d\"";
  assert(lese_meldung(line, NULL) == line);
  assert(strcmp(line, "a\nb\r\"c\\d") == 0);
  strcpy(speech_tbl[1].default_mode_num, "00000001");
  speech_tbl[1].default_mode = speech_mode_lX;
  assert(speech_load("test"));
  assert(strcmp(speech_tbl[1].mode_num, "00000001") == 0);

  char long_text[4096], filter[68];
  memset(long_text, 'a', sizeof(long_text));
  memcpy(long_text, "*** ", 4);
  long_text[sizeof(long_text) - 1] = '\0';
  channel_filter("conversd", long_text, filter);
  assert(strlen(filter) == 66 && filter[0] == ' ' && filter[65] == ' ');
  channel_filter("conversd", "*** ", filter);
  assert(strcmp(filter, "  ") == 0);
  channel_filter("dno484:nick", "hello", filter);
  assert(strcmp(filter, " dno484 ") == 0);

  for (int mode = 0; mode < 8; ++mode) {
    IPpar[0].ipMode = mode;
    accepted = 0;
    forwarding(&output);
    assert(accepted == !!(mode & IP_FORWARDING));
  }
  output.l2fflg = 0;
  accepted = 0;
  forwarding(&output);
  assert(!accepted);
  puts("GCC regression tests passed");
  return 0;
}
EOF
${CC:-cc} -std=gnu99 -Wall -Werror -funsigned-char \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
