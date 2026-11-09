#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
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
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
typedef short WORD;
typedef unsigned short UWORD;
typedef unsigned char UBYTE;
typedef unsigned long ULONG;
typedef enum { FALSE, TRUE } BOOLEAN;
#define L2PNUM 16
#define L1PNUM L2PNUM
#define L2MFLEN 330
#define MAXPATH 256
EOF
sed -n '/^#define CMD_TXDELAY /,/^#define TFESC /p' os/linux/linux.h >> "$tmp/test.c"
extract os/linux/linux.h '^typedef struct$'
cat >> "$tmp/test.c" <<'EOF'
typedef struct MBHEAD {
  struct MBHEAD *head, *tail;
  unsigned mbgc, mbpc;
  char data[8192];
} MBHEAD;
typedef MBHEAD LEHEAD;
typedef MBHEAD LHEAD;
static DEVICE l1port[L1PNUM];
static WORD l1ptab[L2PNUM], l2ptab[L1PNUM], kisschannel[L2PNUM];
static WORD max_device = -1, used_l1ports = -1;
static int tkcom, tkbaud, tokenring_ports, token_count, tic10;
unsigned short my_udp;
static ULONG cd_timer[L2PNUM];
static BOOLEAN kiss_active, tokenflag, testflag[L2PNUM], kick[L2PNUM];
static BOOLEAN commandflag[L2PNUM];
static struct { int speed; } portpar[L2PNUM];
static struct { int reset_count; } portstat[L2PNUM];
static MBHEAD txl2fl[L2PNUM], stfl, message;
static int clicnt;
static char *clipoi;
static BOOLEAN issyso(void) { return TRUE; }
static BOOLEAN skipsp(int *n, char **p) {
  while (*n && **p == ' ') { --*n; ++*p; }
  return *n != 0;
}
static int nxtnum(int *n, char **p) {
  char *end;
  long value = strtol(*p, &end, 10);
  *n -= end - *p;
  *p = end;
  return value;
}
static MBHEAD *getmbp(void) { memset(&message, 0, sizeof(message)); return &message; }
static void putprintf(MBHEAD *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(b->data + b->mbpc, sizeof(b->data) - b->mbpc, fmt, ap);
  va_end(ap);
  assert(n >= 0 && (unsigned)n < sizeof(b->data) - b->mbpc);
  b->mbpc += n;
}
static void putstr(const char *s, MBHEAD *b) { putprintf(b, "%s", s); }
static MBHEAD *putals(const char *s) { MBHEAD *b = getmbp(); putstr(s, b); return b; }
static void prompt(MBHEAD *b) { (void)b; }
static void seteom(MBHEAD *b) { (void)b; }
static void invmsg(void) { assert(0); }
static BOOLEAN portenabled(int port) { return l1ptab[port] >= 0; }
static void l1detach(int port) { (void)port; }
static void tf_set_kiss(DEVICE *d) { (void)d; }
static unsigned getchr(MBHEAD *b) { return (unsigned char)b->data[b->mbgc++]; }
static void ulink(LEHEAD *b) { b->head->tail = b->tail; b->tail->head = b->head; }
static void relink(LEHEAD *b, LEHEAD *tail) {
  b->head = tail->head; b->tail = tail; tail->head->tail = b; tail->head = b;
}
static void append_crc_16(char *b, int *n) { (void)b; (void)n; assert(0); }
static void append_crc_rmnc(char *b, int *n) { (void)b; (void)n; assert(0); }
static int check_crc_16(char *b, int *n) { (void)b; (void)n; return 1; }
static int check_crc_rmnc(char *b, int *n) { (void)b; (void)n; return 1; }
static int received_port, received_len, received_count;
static char received[1024];
static void frame_to_l1(WORD port, char *b, int n) {
  assert(n >= 0 && (unsigned)n <= sizeof(received));
  received_port = port; received_len = n; ++received_count;
  memcpy(received, b, n);
}
static void send_kisscmd(int, int, int);
#include "l1attach.h"
#include "tcpkiss.h"
BOOLEAN tcpkiss_attach(void) { return FALSE; }
BOOLEAN tcpkiss_detach(int port) { (void)port; return FALSE; }
void tcpkiss_dump(MBHEAD *b, int port) { (void)b; (void)port; assert(0); }
EOF
sed '/^#include "tnn.h"/d' os/linux/l1attach.c >> "$tmp/test.c"
extract os/linux/l1linux.c '^void send_kisscmd\('
extract os/linux/l1linux.c '^static void kissframe_to_tnc\(int l2prt,'
extract os/linux/l1linux.c '^static void frame_valid\(WORD port,'
extract os/linux/l1linux.c '^void framedata_to_queue\('
cat >> "$tmp/test.c" <<'EOF'
static void attach(int port, int channel, const char *device, int speed) {
  char command[512];
  snprintf(command, sizeof(command),
           "PORT=%d KTYP=KISS SPEED=%d DEVICE=%s CHANNEL=%d",
           port, speed, device, channel);
  clipoi = command; clicnt = strlen(command); ccpattach();
}
static void detach(int port) {
  char command[16];
  snprintf(command, sizeof(command), "%d", port);
  clipoi = command; clicnt = strlen(command); ccpdetach();
}
static void feed(int device, const unsigned char *b, unsigned n) {
  /* Fragment every byte, including escape pairs and command headers. */
  for (unsigned i = 0; i < n; ++i)
    framedata_to_queue(device, (char *)b + i, 1);
}
static void send_frame(int port, int master, const unsigned char *expected, unsigned n) {
  MBHEAD b = {0};
  char wire[1024];
  b.data[0] = FEND; b.data[1] = FESC; b.data[2] = 'x'; b.mbpc = 3;
  relink(&b, &txl2fl[port]);
  kissframe_to_tnc(port, FALSE);
  assert(read(master, wire, sizeof(wire)) == (ssize_t)n);
  assert(!memcmp(wire, expected, n));
  ulink(&b);
}
int main(void) {
  int master = posix_openpt(O_RDWR | O_NOCTTY);
  assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
  char device[256];
  snprintf(device, sizeof(device), "%s", ptsname(master));
  int probe = open(device, O_RDWR | O_NOCTTY);
  assert(probe >= 0);
  struct termios original, raw, restored;
  assert(tcgetattr(probe, &original) == 0);
  for (int p = 0; p < L2PNUM; ++p) {
    l1ptab[p] = l2ptab[p] = -1;
    txl2fl[p].head = txl2fl[p].tail = &txl2fl[p];
  }
  stfl.head = stfl.tail = &stfl;
  /* Logical ports deliberately differ from KISS channel numbers. */
  attach(7, 0, device, 19200);
  assert(strstr(message.data, "Attached"));
  int dev = l1ptab[7], fd = l1port[dev].kisslink;
  assert(fd >= 0 && used_l1ports == 0);
  assert(tcgetattr(probe, &raw) == 0 && cfgetospeed(&raw) == B19200);
  assert(raw.c_lflag == 0 && raw.c_oflag == 0);
  attach(3, 1, device, 19200);
  assert(l1ptab[3] == dev && l1port[dev].kisslink == fd && used_l1ports == 0);
  attach(4, 1, device, 19200);
  assert(l1ptab[4] == -1 && strstr(message.data, "duplicate"));
  attach(4, 2, device, 9600);
  assert(l1ptab[4] == -1 && strstr(message.data, "match"));
  attach(4, 16, device, 19200);
  assert(l1ptab[4] == -1 && strstr(message.data, "channel"));
  attach(4, -1, device, 19200);
  assert(l1ptab[4] == -1);
  for (int ch = 2; ch < 16; ++ch) {
    int port = ch == 3 ? 1 : ch == 7 ? 0 : ch;
    attach(port, ch, device, 19200);
    assert(l1ptab[port] == dev);
  }
  assert(used_l1ports == 0 && max_device == dev);
  for (int ch = 0; ch < 16; ++ch) {
    int port = ch == 0 ? 7 : ch == 1 ? 3 : ch == 3 ? 1 : ch == 7 ? 0 : ch;
    unsigned char wire[10] = { FEND };
    unsigned n = 1;
    if (ch == 12) { wire[n++] = FESC; wire[n++] = TFEND; }
    else wire[n++] = ch << 4;
    wire[n++] = FESC; wire[n++] = TFEND;
    wire[n++] = FESC; wire[n++] = TFESC;
    wire[n++] = 'x'; wire[n++] = FEND;
    feed(dev, wire, n);
    assert(received_port == port && received_len == 3);
    assert((unsigned char)received[0] == FEND && (unsigned char)received[1] == FESC);
    send_frame(port, master, wire, n);
    send_kisscmd(port, CMD_TXDELAY, FEND);
    char command[8];
    assert(read(master, command, sizeof(command)) == 5);
    unsigned char expected[] = { FEND, (ch << 4) | CMD_TXDELAY, FESC, TFEND, FEND };
    assert(!memcmp(command, expected, sizeof(expected)));
  }
  /* Reject an oversized frame safely, including an escaped final byte. */
  int before = received_count;
  unsigned char oversized[MAXKISSLEN + 8];
  memset(oversized, 'a', sizeof(oversized));
  oversized[0] = FEND; oversized[1] = 0x10;
  oversized[MAXKISSLEN + 1] = FESC;
  oversized[MAXKISSLEN + 2] = TFEND;
  oversized[sizeof(oversized) - 1] = FEND;
  feed(dev, oversized, sizeof(oversized));
  assert(received_count == before);
  unsigned char recovery[] = { FEND, 0x10, 'r', FEND };
  feed(dev, recovery, sizeof(recovery));
  assert(received_count == before + 1 && received_port == 3);
  MBHEAD maximum = {0};
  memset(maximum.data, FEND, MAXFRAMELEN - 1);
  maximum.mbpc = MAXFRAMELEN - 1;
  relink(&maximum, &txl2fl[12]);
  kissframe_to_tnc(12, FALSE);
  unsigned char maxwire[2 * MAXKISSLEN + 2];
  ssize_t maxlen = read(master, maxwire, sizeof(maxwire));
  assert(maxlen == 2 * (MAXFRAMELEN - 1) + 4);
  feed(dev, maxwire, maxlen);
  assert(received_port == 12 && received_len == MAXFRAMELEN - 1);
  for (int i = 0; i < received_len; ++i) assert((unsigned char)received[i] == FEND);
  ulink(&maximum);
  /* Dump and reload must retain arbitrary mappings on one descriptor. */
  MBHEAD saved = {0};
  dump_attach(&saved);
  assert(strstr(saved.data, "PORT=7 KTYP=KISS SPEED=19200"));
  assert(strstr(saved.data, "CHANNEL=15"));
  int count = received_count;
  unsigned char control[] = { FEND, 0x11, 42, FEND };
  feed(dev, control, sizeof(control));
  assert(received_count == count);
  unsigned char partial[] = { FEND, 0x00, 'a' };
  feed(dev, partial, sizeof(partial));
  detach(7);
  assert(l1port[dev].port_active && fcntl(fd, F_GETFD) >= 0);
  unsigned char removed[] = { FEND, 0x00, 'a', FEND };
  feed(dev, removed, sizeof(removed));
  assert(received_count == count);
  unsigned char alive[] = { FEND, 0x10, 'b', FEND };
  feed(dev, alive, sizeof(alive));
  assert(received_count == count + 1 && received_port == 3);
  for (int p = 0; p < L2PNUM; ++p)
    if (l1ptab[p] >= 0) detach(p);
  assert(used_l1ports == -1 && max_device == -1 && fcntl(fd, F_GETFD) == -1);
  assert(tcgetattr(probe, &restored) == 0);
  assert(restored.c_iflag == original.c_iflag && restored.c_lflag == original.c_lflag);
  assert(cfgetospeed(&restored) == cfgetospeed(&original));
  /* Replay the actual dumped ATTACH lines. */
  char *line = strtok(saved.data, "\r\n");
  while (line) {
    if (!strncmp(line, "ATTACH ", 7)) {
      clipoi = line + 7; clicnt = strlen(clipoi); ccpattach();
      if (!strstr(message.data, "Attached"))
        fprintf(stderr, "Replay failed: %s -> %s\n", line, message.data);
      assert(strstr(message.data, "Attached"));
    }
    line = strtok(NULL, "\r\n");
  }
  assert(used_l1ports == 0 && l1ptab[7] == l1ptab[3] && kisschannel[15] == 15);
  for (int p = 0; p < L2PNUM; ++p) detach(p);
  /* Old syntax without CHANNEL still selects channel zero. */
  char legacy[512];
  snprintf(legacy, sizeof(legacy), "PORT=5 KTYP=KISS SPEED=19200 DEVICE=%s", device);
  clipoi = legacy; clicnt = strlen(legacy); ccpattach();
  assert(l1ptab[5] >= 0 && kisschannel[5] == 0);
  detach(5);
  /* Rejected input must not open a device or consume a logical port. */
  snprintf(legacy, sizeof(legacy),
           "PORT=128 KTYP=KISS SPEED=19200 DEVICE=%s CHANNEL=0", device);
  clipoi = legacy; clicnt = strlen(legacy); ccpattach();
  assert(max_device == -1 && strstr(message.data, "Invalid Port"));
  snprintf(legacy, sizeof(legacy),
           "PORT=5 KTYP=KISS SPEED=19200 DEVICE=%s CHANNEL=1x", device);
  clipoi = legacy; clicnt = strlen(legacy); ccpattach();
  assert(max_device == -1 && strstr(message.data, "channel"));
  snprintf(legacy, sizeof(legacy),
           "PORT=5 KTYP=SMACK SPEED=19200 DEVICE=%s CHANNEL=0", device);
  clipoi = legacy; clicnt = strlen(legacy); ccpattach();
  assert(max_device == -1 && strstr(message.data, "channel"));
  attach(5, 0, "/definitely-not-a-tnn-device", 19200);
  assert(l1ptab[5] == -1 && max_device == -1 && used_l1ports == -1);
  /* Reuse a freed physical slot while another serial device remains open. */
  int second = posix_openpt(O_RDWR | O_NOCTTY);
  assert(second >= 0 && grantpt(second) == 0 && unlockpt(second) == 0);
  char device2[256];
  snprintf(device2, sizeof(device2), "%s", ptsname(second));
  attach(5, 0, device, 19200);
  attach(6, 0, device2, 19200);
  int other = l1ptab[6], otherfd = l1port[other].kisslink;
  detach(5);
  attach(5, 0, device, 19200);
  assert(l1ptab[5] == 0 && l1ptab[6] == other && fcntl(otherfd, F_GETFD) >= 0);
  detach(6);
  assert(l1port[l1ptab[5]].port_active);
  detach(5);
  close(second);
  close(probe); close(master);
  puts("Multiport KISS: 16 channels, RX/TX escaping, parameters, shared PTY, detach, dump/reload and legacy syntax passed");
  return 0;
}
EOF
${CC:-cc} -std=c99 -Iinclude -funsigned-char -Wall -Wextra -Werror \
  -Wno-unused-parameter -Wno-implicit-fallthrough \
  -fsanitize=address,undefined -g "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
