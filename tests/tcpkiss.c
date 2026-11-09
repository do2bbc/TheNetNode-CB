#ifdef __linux__
#include <netinet/tcp.h>
#endif
#include "tnn.h"
#include "tcpkiss.h"
#include <assert.h>
#include <stdarg.h>

#ifdef __WIN32__
typedef SOCKET TEST_SOCKET;
#define INVALID_TEST_SOCKET INVALID_SOCKET
#define test_close closesocket
#define pause_test() Sleep(2)
#else
typedef int TEST_SOCKET;
#define INVALID_TEST_SOCKET (-1)
#define test_close close
#define pause_test() usleep(2000)
#endif

DEVICE l1port[L1PNUM];
WORD l1ptab[L2PNUM], l2ptab[L1PNUM], max_device = -1, used_l1ports = -1;
WORD kick[L2PNUM], commandflag[L2PNUM], testflag[L2PNUM];
BOOLEAN kiss_active;
PORTINFO portpar[L2PNUM];
LHEAD rxfl, stfl, txl2fl[L2PNUM];
volatile ULONG tic10;
char *clipoi;
WORD clicnt;

typedef struct {
  MBHEAD head;
  unsigned char data[8192];
} TEST_FRAME;
static TEST_FRAME message;
static int send_limit, send_block, send_fail;

#ifdef __WIN32__
int WSAAPI tcpkiss_test_send(SOCKET socket, const char *data, int len, int flags)
#else
ssize_t tcpkiss_test_send(int socket, const void *data, size_t len, int flags)
#endif
{
  if (send_block || send_fail)
  {
#ifdef __WIN32__
    WSASetLastError(send_fail ? WSAECONNRESET : WSAEWOULDBLOCK);
#else
    errno = send_fail ? ECONNRESET : EAGAIN;
#endif
    return -1;
  }
  if (send_limit && (unsigned)len > (unsigned)send_limit)
    len = send_limit;
  return send(socket, data, len, flags);
}

LEHEAD *allocb(int owner)
{
  TEST_FRAME *frame = calloc(1, sizeof(*frame));
  (void)owner;
  assert(frame);
  return (LEHEAD *)frame;
}

LEHEAD *ulink(LEHEAD *entry)
{
  entry->nextle->prevle = entry->prevle;
  entry->prevle->nextle = entry->nextle;
  return entry;
}

LEHEAD *relink(LEHEAD *entry, LEHEAD *tail)
{
  entry->nextle = tail->nextle;
  entry->prevle = tail;
  tail->nextle->prevle = entry;
  tail->nextle = entry;
  return entry;
}

void putchr(char byte, MBHEAD *head)
{
  TEST_FRAME *frame = (TEST_FRAME *)head;
  assert(head->mbpc < sizeof(frame->data));
  frame->data[head->mbpc++] = byte;
}

#ifdef __WIN32__
unsigned
#endif
char getchr(MBHEAD *head)
{
  TEST_FRAME *frame = (TEST_FRAME *)head;
  assert(head->mbgc < head->mbpc);
  return frame->data[head->mbgc++];
}

MBHEAD *getmbp(void)
{
  memset(&message, 0, sizeof(message));
  return &message.head;
}

void putprintf(MBHEAD *head, const char *format, ...)
{
  TEST_FRAME *frame = (TEST_FRAME *)head;
  va_list args;
  int n;
  va_start(args, format);
  n = vsnprintf((char *)frame->data + head->mbpc,
                sizeof(frame->data) - head->mbpc, format, args);
  va_end(args);
  assert(n >= 0 && (unsigned)n < sizeof(frame->data) - head->mbpc);
  head->mbpc += n;
}

BOOLEAN issyso(void) { return TRUE; }
void prompt(MBHEAD *head) { (void)head; }
void seteom(MBHEAD *head) { (void)head; }

static void init_queue(LHEAD *queue)
{
  queue->head = queue->tail = queue;
}

static void drain(LHEAD *queue)
{
  while (queue->head != queue)
    free(ulink((LEHEAD *)queue->head));
}

static void nonblocking(TEST_SOCKET socket)
{
  int one = 1;
  assert(setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                   (const char *)&one, sizeof(one)) == 0);
#ifdef __WIN32__
  u_long value = 1;
  assert(ioctlsocket(socket, FIONBIO, &value) == 0);
#else
  assert(fcntl(socket, F_SETFL, O_NONBLOCK) == 0);
#endif
}

static TEST_SOCKET listener(unsigned short *port)
{
  TEST_SOCKET socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  struct sockaddr_in address;
#ifdef __WIN32__
  int len = sizeof(address);
#else
  socklen_t len = sizeof(address);
#endif
  assert(socket_fd != INVALID_TEST_SOCKET);
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(*port);
  {
    int one = 1;
    assert(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                     (const char *)&one, sizeof(one)) == 0);
  }
  assert(bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) == 0);
  assert(getsockname(socket_fd, (struct sockaddr *)&address, &len) == 0);
  *port = ntohs(address.sin_port);
  assert(listen(socket_fd, 4) == 0);
  nonblocking(socket_fd);
  return socket_fd;
}

static void attach_text(const char *text, const char *expected)
{
  clipoi = (char *)text;
  clicnt = strlen(text);
  assert(tcpkiss_attach());
  assert(strstr((char *)message.data, expected));
}

static void attach_port(int port, int channel, unsigned short tcpport)
{
  char text[160];
  snprintf(text, sizeof(text),
           "PORT=%d KTYP=TCPKISS DEVICE=127.0.0.1:%u CHANNEL=%d",
           port, tcpport, channel);
  attach_text(text, "Attached");
  portpar[port].major = 1;
  portpar[port].txdelay = 30;
  portpar[port].persistance = 63;
  portpar[port].slottime = 10;
}

static TEST_SOCKET accept_peer(TEST_SOCKET server)
{
  unsigned i;
  TEST_SOCKET peer = INVALID_TEST_SOCKET;
  for (i = 0; i < 2000 && peer == INVALID_TEST_SOCKET; ++i)
  {
    tcpkiss_poll();
    ++tic10;
    peer = accept(server, NULL, NULL);
    pause_test();
  }
  assert(peer != INVALID_TEST_SOCKET);
  nonblocking(peer);
  return peer;
}

static unsigned read_wire(TEST_SOCKET peer, unsigned char *data, unsigned capacity,
                          unsigned required)
{
  unsigned i, used = 0;
  for (i = 0; i < 2000 && used < required; ++i)
  {
    int n;
    tcpkiss_poll();
    ++tic10;
    n = recv(peer, (char *)data + used, capacity - used, 0);
    if (n > 0)
      used += n;
    pause_test();
  }
  assert(used >= required);
  return used;
}

static void feed(TEST_SOCKET peer, const unsigned char *data, unsigned len)
{
  unsigned i, j;
  for (i = 0; i < len; ++i)
  {
    assert(send(peer, (const char *)data + i, 1, 0) == 1);
    for (j = 0; j < 3; ++j)
    {
      tcpkiss_poll();
      ++tic10;
      pause_test();
    }
  }
}

static void tx_frame(int port, const unsigned char *data, unsigned len)
{
  MBHEAD *head = (MBHEAD *)allocb(ALLOC_MBHEAD);
  unsigned i;
  for (i = 0; i < len; ++i)
    putchr(data[i], head);
  relink((LEHEAD *)head, (LEHEAD *)txl2fl[port].tail);
  kick[port] = TRUE;
}

static void protocol_test(void)
{
  unsigned short tcpport = 0;
  TEST_SOCKET server = listener(&tcpport), peer;
  unsigned char wire[8192], bad[400], payload[] = {FEND, FESC, 'x'};
  const unsigned char expected[] = {FEND, 0x10, FESC, TFEND, FESC, TFESC, 'x', FEND};
  const unsigned char rx[] = {FEND, 0x10, 'a', FESC, TFEND, FESC, TFESC, FEND};
  unsigned count, i;
  int device;
  attach_port(7, 0, tcpport);
  attach_port(3, 1, tcpport);
  device = l1ptab[7];
  assert(l1ptab[3] == device && used_l1ports == 0 && max_device == device);
  {
    char command[160];
    snprintf(command, sizeof(command),
             "PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1:%u CHANNEL=1", tcpport);
    attach_text(command, "Duplicate KISS channel");
    assert(l1ptab[4] == -1);
  }
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1:8001 CHANNEL=16", "Invalid");
  attach_text("PORT=-1 KTYP=TCPKISS DEVICE=127.0.0.1:8001", "Invalid");
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1:65536", "Invalid");
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1:99999999999999999999", "Invalid");
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1:0", "Invalid");
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1", "hostname");
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=:8001", "hostname");
  attach_text("PORT=4 KTYP=TCPKISS DEVICE=127.0.0.1:8001 SPEED=0", "Unknown");
  attach_text("PORT=4 PORT=5 KTYP=TCPKISS DEVICE=127.0.0.1:8001", "Duplicate");
  attach_text("KTYP=TCPKISS DEVICE=127.0.0.1:8001", "required");
  assert(l1ptab[4] == -1);
  send_limit = 1;
  peer = accept_peer(server);
  count = read_wire(peer, wire, sizeof(wire), 40);
  assert(count == 40);
  assert(wire[1] == (0x10 | CMD_TXDELAY) && wire[2] == 30);
  assert(wire[21] == CMD_TXDELAY && wire[22] == 30);
  for (i = 0; i < count; i += 4)
    assert((wire[i + 1] & 15) >= 1 && (wire[i + 1] & 15) <= 5);
  feed(peer, rx, sizeof(rx));
  assert(rxfl.head != &rxfl);
  {
    TEST_FRAME *frame = (TEST_FRAME *)rxfl.head;
    assert(frame->head.l2port == 3 && frame->head.mbpc == 3);
    assert(frame->data[0] == 'a' && frame->data[1] == FEND && frame->data[2] == FESC);
  }
  drain(&rxfl);
  portpar[3].major = NO_MAJOR;
  feed(peer, rx, sizeof(rx));
  assert(rxfl.head == &rxfl);
  portpar[3].major = 1;
  memset(bad, 'x', sizeof(bad));
  bad[0] = FEND;
  bad[1] = 0x10;
  bad[sizeof(bad) - 1] = FEND;
  feed(peer, bad, sizeof(bad));
  assert(rxfl.head == &rxfl && l1port[device].bad_frames);
  {
    const unsigned char invalid[] = {
      FEND, 0x11, 'x', FEND, /* Parameter response must not become AX.25. */
      FEND, 0x20, 'x', FEND, /* Unmapped channel. */
      FEND, 0x10, FESC, 'x', FEND
    };
    feed(peer, invalid, sizeof(invalid));
    assert(rxfl.head == &rxfl);
  }
  send_block = 1;
  tx_frame(3, payload, sizeof(payload));
  for (i = 0; i < 10; ++i)
    tcpkiss_poll();
  send_block = 0;
  count = read_wire(peer, wire, sizeof(wire), sizeof(expected));
  assert(count == sizeof(expected) && !memcmp(wire, expected, sizeof(expected)));
  drain(&stfl);
  for (i = 2; i < 16; ++i)
  {
    int port = i == 3 ? 1 : i == 7 ? 0 : (int)i;
    attach_port(port, i, tcpport);
  }
  count = read_wire(peer, wire, sizeof(wire), 14 * 20);
  assert(count == 14 * 20);
  for (i = 0; i < 16; ++i)
  {
    unsigned char frame[] = {FEND, (unsigned char)(i << 4), 'z', FEND};
    unsigned char expected_tx[9];
    unsigned offset = 1;
    int port = i == 0 ? 7 : i == 1 ? 3 : i == 3 ? 1 : i == 7 ? 0 : (int)i;
    if (i == 12)
    {
      const unsigned char escaped[] = {FEND, FESC, TFEND, 'z', FEND};
      feed(peer, escaped, sizeof(escaped));
    }
    else
      feed(peer, frame, sizeof(frame));
    assert(rxfl.head != &rxfl);
    assert(((MBHEAD *)rxfl.head)->l2port == port);
    drain(&rxfl);
    expected_tx[0] = FEND;
    if (i == 12)
    {
      expected_tx[offset++] = FESC;
      expected_tx[offset++] = TFEND;
    }
    else
      expected_tx[offset++] = i << 4;
    memcpy(expected_tx + offset, expected + 2, sizeof(expected) - 2);
    offset += sizeof(expected) - 2;
    tx_frame(port, payload, sizeof(payload));
    count = read_wire(peer, wire, sizeof(wire), offset);
    assert(count == offset && !memcmp(wire, expected_tx, offset));
    drain(&stfl);
  }
  {
    unsigned char maximum[MAXFRAMELEN];
    memset(maximum, FEND, sizeof(maximum));
    tx_frame(3, maximum, sizeof(maximum));
    count = read_wire(peer, wire, sizeof(wire), 2 * MAXFRAMELEN + 3);
    assert(count == 2 * MAXFRAMELEN + 3);
    assert(wire[0] == FEND && wire[1] == 0x10 && wire[count - 1] == FEND);
    for (i = 2; i < count - 1; i += 2)
      assert(wire[i] == FESC && wire[i + 1] == TFEND);
    drain(&stfl);
  }
  {
    const unsigned char parameters[] = {
      FEND, 0x11, FESC, TFEND, FEND,
      FEND, 0x12, FESC, TFESC, FEND
    };
    portpar[3].txdelay = FEND;
    portpar[3].persistance = FESC;
    commandflag[3] = TRUE;
    count = read_wire(peer, wire, sizeof(wire), 22);
    assert(count == 22 && !memcmp(wire, parameters, sizeof(parameters)));
    portpar[3].txdelay = 30;
    portpar[3].persistance = 63;
  }
  getmbp();
  tcpkiss_dump(&message.head, 3);
  assert(strstr((char *)message.data, "CHANNEL=1"));
  /* The same TCP stream survives detaching one logical port. */
  assert(tcpkiss_detach(7));
  assert(l1port[device].port_active && l1ptab[3] == device);
  feed(peer, rx, sizeof(rx));
  assert(rxfl.head != &rxfl);
  drain(&rxfl);
  /* Force a send failure with an incomplete frame; never replay its suffix. */
  tx_frame(3, payload, sizeof(payload));
  tcpkiss_poll();
  send_fail = 1;
  tcpkiss_poll();
  send_fail = 0;
  test_close(peer);
  test_close(server);
  tic10 += 500;
  tcpkiss_poll();
  tcpkiss_poll();
  /* Queue a new frame while the server is absent, then restart it. */
  tx_frame(3, payload, sizeof(payload));
  server = listener(&tcpport);
  tic10 += 500;
  peer = accept_peer(server);
  count = read_wire(peer, wire, sizeof(wire), 15 * 20 + sizeof(expected));
  assert(count == 15 * 20 + sizeof(expected));
  {
    unsigned matches = 0;
    for (i = 0; i + sizeof(expected) <= count; ++i)
      if (!memcmp(wire + i, expected, sizeof(expected)))
        ++matches;
    assert(matches == 1);
  }
  drain(&stfl);
  /* Discard an RX fragment at disconnect. */
  {
    const unsigned char fragment[] = {FEND, 0x10, 'a', FESC};
    feed(peer, fragment, sizeof(fragment));
  }
  test_close(peer);
  tcpkiss_poll();
  tic10 += 500;
  peer = accept_peer(server);
  read_wire(peer, wire, sizeof(wire), 15 * 20);
  feed(peer, rx, sizeof(rx));
  assert(rxfl.head != &rxfl);
  assert(((MBHEAD *)rxfl.head)->mbpc == 3);
  drain(&rxfl);
  for (i = 0; i < L2PNUM; ++i)
    if (l1ptab[i] == device)
      assert(tcpkiss_detach(i));
  assert(!l1port[device].port_active && max_device == -1 && used_l1ports == -1);
  test_close(peer);
  test_close(server);
  puts("TCPKISS: 16 channels, framing, parameters, short writes, backpressure, reconnect and detach passed");
}

static void direwolf_test(unsigned short tcpport)
{
  static const unsigned char frame[] = {
    0x9c, 0x60, 0x86, 0x82, 0x98, 0x98, 0x60, /* N0CALL */
    0x9c, 0x60, 0x86, 0x82, 0x98, 0x98, 0x63, /* N0CALL-1 */
    0x03, 0xf0, 'T', 'N', 'N', '-', 'T', 'C', 'P', 'K', 'I', 'S', 'S'
  };
  unsigned i;
  attach_port(7, 0, tcpport);
  attach_port(3, 1, tcpport);
  for (i = 0; i < 2000 && commandflag[3]; ++i)
  {
    tcpkiss_poll();
    ++tic10;
    pause_test();
  }
  assert(!commandflag[3]);
  tx_frame(7, frame, sizeof(frame));
  tx_frame(3, frame, sizeof(frame));
  for (i = 0; i < 1000; ++i)
  {
    tcpkiss_poll();
    ++tic10;
    pause_test();
  }
  assert(stfl.head != &stfl);
  drain(&stfl);
  tcpkiss_exit();
  puts("Dire Wolf TCPKISS: two-channel connect, parameters and AX.25 TX completed; verify dummy log");
}

int main(int argc, char **argv)
{
  int port;
#ifdef __WIN32__
  WSADATA data;
  assert(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
  init_queue(&rxfl);
  init_queue(&stfl);
  for (port = 0; port < L2PNUM; ++port)
  {
    l1ptab[port] = l2ptab[port] = -1;
    l1port[port].kisslink = -1;
    portpar[port].major = NO_MAJOR;
    init_queue(&txl2fl[port]);
  }
  if (argc == 2)
    direwolf_test((unsigned short)atoi(argv[1]));
  else
    protocol_test();
#ifdef __WIN32__
  WSACleanup();
#endif
  return 0;
}
