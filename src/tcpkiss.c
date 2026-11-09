#ifdef __linux__
#include <netinet/tcp.h>
#endif
#include "tnn.h"
#include "tcpkiss.h"

#ifdef __WIN32__
typedef SOCKET KISS_SOCKET;
#define KISS_INVALID INVALID_SOCKET
#define kiss_close closesocket
#define kiss_error WSAGetLastError()
#else
#include <netdb.h>
typedef int KISS_SOCKET;
#define KISS_INVALID (-1)
#define kiss_close close
#define kiss_error errno
#endif

#define KISS_RETRY 500
#define KISS_CONNECT_TIMEOUT 1000
#define KISS_WIRE_SIZE (2 * (MAXFRAMELEN + 1) + 2)

typedef struct {
  KISS_SOCKET socket;
  struct sockaddr_in address;
  int state;                    /* 0: retry, 1: connecting, 2: connected */
  ULONG since;
  unsigned char rx[MAXFRAMELEN + 1], tx[KISS_WIRE_SIZE];
  unsigned rxlen, txlen, txpos;
  int next_port;
  BOOLEAN in_frame, escape, invalid;
} TCPKISS;

static TCPKISS tcpkiss_connections[L1PNUM];
static int tcpkiss_channels[L2PNUM];

static int pending_error(int error)
{
#ifdef __WIN32__
  return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS
      || error == WSAEALREADY || error == WSAEINTR;
#else
  return error == EWOULDBLOCK || error == EAGAIN || error == EINPROGRESS
      || error == EALREADY || error == EINTR;
#endif
}

static void disconnected(int device, const char *reason, int error)
{
  TCPKISS *c = &tcpkiss_connections[device];
  printf("TCPKISS %s: %s (error %d), retry in 5 seconds%s\n",
         l1port[device].device, reason, error,
         c->txlen ? "; discarding pending frame, AX.25 must retry" : "");
  if (c->socket != KISS_INVALID)
    kiss_close(c->socket);
  c->socket = KISS_INVALID;
  c->state = 0;
  c->since = tic10;
  c->txlen = c->txpos = c->rxlen = 0;
  c->in_frame = c->escape = c->invalid = FALSE;
}

static void connected(int device)
{
  TCPKISS *c = &tcpkiss_connections[device];
  int port;
  c->state = 2;
  c->next_port = 0;
  printf("TCPKISS %s: connected\n", l1port[device].device);
  for (port = 0; port < L2PNUM; ++port)
    if (l1ptab[port] == device)
      commandflag[port] = TRUE;
}

static void connect_peer(int device)
{
  TCPKISS *c = &tcpkiss_connections[device];
  int result, one = 1;
#ifdef __WIN32__
  u_long nonblocking = 1;
#endif
  c->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (c->socket == KISS_INVALID)
  {
    disconnected(device, "socket failed", kiss_error);
    return;
  }
#ifndef __WIN32__
  if (c->socket >= FD_SETSIZE)
  {
    disconnected(device, "socket exceeds select descriptor limit", 0);
    return;
  }
#endif
#ifdef __WIN32__
  result = ioctlsocket(c->socket, FIONBIO, &nonblocking);
#else
  result = fcntl(c->socket, F_GETFL, 0);
  if (result >= 0)
    result = fcntl(c->socket, F_SETFL, result | O_NONBLOCK);
#endif
  if (result < 0
      || setsockopt(c->socket, IPPROTO_TCP, TCP_NODELAY,
                    (const char *)&one, sizeof(one)) != 0
      || setsockopt(c->socket, SOL_SOCKET, SO_KEEPALIVE,
                    (const char *)&one, sizeof(one)) != 0)
  {
    disconnected(device, "socket configuration failed", kiss_error);
    return;
  }
  c->since = tic10;
  result = connect(c->socket, (struct sockaddr *)&c->address, sizeof(c->address));
  if (result == 0)
    connected(device);
  else if (pending_error(kiss_error))
    c->state = 1;
  else
    disconnected(device, "connect failed", kiss_error);
}

static void flush_output(int device)
{
  TCPKISS *c = &tcpkiss_connections[device];
  int n;
  if (!c->txlen)
    return;
  n = send(c->socket, (const char *)c->tx + c->txpos, c->txlen - c->txpos,
#ifdef __WIN32__
           0
#else
           MSG_NOSIGNAL
#endif
          );
  if (n > 0)
  {
    c->txpos += n;
    if (c->txpos == c->txlen)
      c->txpos = c->txlen = 0;
  }
  else if (n == 0 || !pending_error(kiss_error))
    disconnected(device, "send failed", n == 0 ? 0 : kiss_error);
}

static void escaped_byte(TCPKISS *c, unsigned char byte)
{
  if (byte == FEND || byte == FESC)
  {
    c->tx[c->txlen++] = FESC;
    c->tx[c->txlen++] = byte == FEND ? TFEND : TFESC;
  }
  else
    c->tx[c->txlen++] = byte;
}

static void command(TCPKISS *c, int port, int cmd, int value)
{
  c->tx[c->txlen++] = FEND;
  escaped_byte(c, (tcpkiss_channels[port] << 4) | cmd);
  escaped_byte(c, value);
  c->tx[c->txlen++] = FEND;
}

static void receive_bytes(int device, const unsigned char *data, unsigned len)
{
  TCPKISS *c = &tcpkiss_connections[device];
  unsigned i;
  for (i = 0; i < len; ++i)
  {
    unsigned char byte = data[i];
    if (byte == FEND)
    {
      if (c->in_frame && !c->invalid && !c->escape && c->rxlen > 1
          && (c->rx[0] & 15) == 0)
      {
        int port;
        for (port = 0; port < L2PNUM; ++port)
          if (l1ptab[port] == device && tcpkiss_channels[port] == (c->rx[0] >> 4)
              && portenabled(port))
          {
            MBHEAD *frame = (MBHEAD *)allocb(ALLOC_MBHEAD);
            unsigned j;
            frame->l2port = port;
            for (j = 1; j < c->rxlen; ++j)
              putchr(c->rx[j], frame);
            relink((LEHEAD *)frame, (LEHEAD *)rxfl.tail);
            break;
          }
      }
      else if (c->invalid || c->escape)
        ++l1port[device].bad_frames;
      c->in_frame = TRUE;
      c->rxlen = 0;
      c->invalid = c->escape = FALSE;
      continue;
    }
    if (!c->in_frame || c->invalid)
      continue;
    if (c->escape)
    {
      c->escape = FALSE;
      if (byte == TFEND)
        byte = FEND;
      else if (byte == TFESC)
        byte = FESC;
      else
      {
        c->invalid = TRUE;
        continue;
      }
    }
    else if (byte == FESC)
    {
      c->escape = TRUE;
      continue;
    }
    if (c->rxlen == sizeof(c->rx))
      c->invalid = TRUE;
    else
      c->rx[c->rxlen++] = byte;
  }
}

void tcpkiss_poll(void)
{
  int device, port;
  for (device = 0; device <= max_device; ++device)
  {
    TCPKISS *c = &tcpkiss_connections[device];
    unsigned char buffer[2048];
    int n, scan, first;
    if (!l1port[device].port_active || l1port[device].kisstype != KISS_TCP)
      continue;
    if (c->state == 0)
    {
      if ((ULONG)(tic10 - c->since) >= KISS_RETRY)
        connect_peer(device);
      continue;
    }
    if (c->state == 1)
    {
      fd_set writefds, errors;
      struct timeval timeout = {0, 0};
      FD_ZERO(&writefds);
      FD_ZERO(&errors);
      FD_SET(c->socket, &writefds);
      FD_SET(c->socket, &errors);
      n = select((int)c->socket + 1, NULL, &writefds, &errors, &timeout);
      if (n > 0)
      {
        int error = 0;
#ifdef __WIN32__
        int size = sizeof(error);
#else
        socklen_t size = sizeof(error);
#endif
        if (getsockopt(c->socket, SOL_SOCKET, SO_ERROR, (char *)&error, &size) != 0)
          error = kiss_error;
        if (error)
          disconnected(device, "connect failed", error);
        else
          connected(device);
      }
      else if (n < 0 && !pending_error(kiss_error))
        disconnected(device, "select failed", kiss_error);
      else if ((ULONG)(tic10 - c->since) >= KISS_CONNECT_TIMEOUT)
        disconnected(device, "connect timeout", 0);
      continue;
    }
    n = recv(c->socket, (char *)buffer, sizeof(buffer), 0);
    if (n > 0)
      receive_bytes(device, buffer, n);
    else if (n == 0 || !pending_error(kiss_error))
    {
      disconnected(device, "connection lost", n == 0 ? 0 : kiss_error);
      continue;
    }
    flush_output(device);
    if (c->state != 2 || c->txlen)
      continue;
    first = c->next_port;
    for (scan = 0; scan < L2PNUM; ++scan)
    {
      LHEAD *queue;
      MBHEAD *frame;
      port = (first + scan) % L2PNUM;
      if (l1ptab[port] != device || !portenabled(port))
        continue;
      c->next_port = (port + 1) % L2PNUM;
      if (testflag[port])
      {
        printf("TCPKISS port %d: TEST signal not supported; use Dire Wolf calibration\n", port);
        testflag[port] = FALSE;
      }
      if (portpar[port].reset_port)
      {
        portpar[port].reset_port = FALSE;
        commandflag[port] = TRUE;
      }
      if (commandflag[port])
      {
        command(c, port, CMD_TXDELAY, portpar[port].txdelay);
        command(c, port, CMD_PERSIST, fullduplex(port) ? 255 : portpar[port].persistance);
        command(c, port, CMD_SLOTTIME, portpar[port].slottime);
        command(c, port, CMD_TXTAIL, TAILTIME);
        command(c, port, CMD_FULLDUP, fullduplex(port) ? 1 : 0);
        commandflag[port] = FALSE;
        flush_output(device);
        if (c->state != 2 || c->txlen)
          break;
      }
      queue = (LHEAD *)&txl2fl[port];
      if (queue->head == queue)
      {
        kick[port] = FALSE;
        continue;
      }
      frame = (MBHEAD *)queue->head;
      if (frame->mbpc - frame->mbgc > MAXFRAMELEN)
      {
        printf("TCPKISS port %d: oversized TX frame discarded\n", port);
        ulink((LEHEAD *)frame);
        relink((LEHEAD *)frame, (LEHEAD *)stfl.tail);
        continue;
      }
      c->tx[c->txlen++] = FEND;
      escaped_byte(c, tcpkiss_channels[port] << 4);
      while (frame->mbgc < frame->mbpc)
        escaped_byte(c, getchr(frame));
      c->tx[c->txlen++] = FEND;
      ulink((LEHEAD *)frame);
      relink((LEHEAD *)frame, (LEHEAD *)stfl.tail);
      flush_output(device);
      if (c->state != 2 || c->txlen)
        break;
    }
  }
}

/* Parse separately so serial ATTACH keeps its historical syntax and behavior. */
BOOLEAN tcpkiss_attach(void)
{
  char text[MAXPATH + 128], endpoint[MAXPATH], host[MAXPATH];
  char *token, *value, *end, *colon;
  int port = -1, channel = 0, device, p, tcpport, seen = 0;
  BOOLEAN ours = FALSE;
  const char *error = NULL;
  struct addrinfo hints, *address = NULL;
  MBHEAD *mbp;
  if (!issyso() || clicnt <= 0 || (unsigned)clicnt >= sizeof(text))
    return FALSE;
  memcpy(text, clipoi, clicnt);
  text[clicnt] = 0;
  for (token = strtok(text, " \t\r\n"); token; token = strtok(NULL, " \t\r\n"))
    if (stricmp(token, "KTYP=TCPKISS") == 0)
      ours = TRUE;
  if (!ours)
    return FALSE;
  memcpy(text, clipoi, clicnt);
  text[clicnt] = 0;
  endpoint[0] = 0;
  for (token = strtok(text, " \t\r\n"); token; token = strtok(NULL, " \t\r\n"))
  {
    long number;
    int bit = 0;
    value = strchr(token, '=');
    if (!value)
    {
      error = "Expected OPTION=value.";
      break;
    }
    *value++ = 0;
    if (stricmp(token, "PORT") == 0 || stricmp(token, "CHANNEL") == 0)
    {
      number = strtol(value, &end, 10);
      if (!*value || *end || number < 0
          || number >= (stricmp(token, "PORT") == 0 ? L2PNUM : 16))
      {
        error = "Invalid port or channel.";
        break;
      }
      if (stricmp(token, "PORT") == 0)
      {
        port = (int)number;
        bit = 1;
      }
      else
      {
        channel = (int)number;
        bit = 2;
      }
    }
    else if (stricmp(token, "DEVICE") == 0 && strlen(value) < sizeof(endpoint))
    {
      strcpy(endpoint, value);
      bit = 4;
    }
    else if (stricmp(token, "KTYP") == 0 && stricmp(value, "TCPKISS") == 0)
      bit = 8;
    else
      error = "Unknown TCPKISS option.";
    if (seen & bit)
      error = "Duplicate TCPKISS option.";
    if (error)
      break;
    seen |= bit;
  }
  if (!error && (port < 0 || !endpoint[0]))
    error = "PORT and DEVICE are required.";
  if (!error && l1ptab[port] != -1)
    error = "Port is busy.";
  strcpy(host, endpoint);
  colon = strrchr(host, ':');
  if (!error && (!colon || colon == host))
    error = "DEVICE must be hostname:tcpport (IPv4).";
  tcpport = 0;
  if (!error)
  {
    *colon++ = 0;
    {
      long number = strtol(colon, &end, 10);
      if (!*colon || *end || number < 1 || number > 65535)
        error = "Invalid TCP port.";
      else
        tcpport = (int)number;
    }
  }
  device = -1;
  if (!error)
    for (p = 0; p <= max_device; ++p)
      if (l1port[p].port_active && l1port[p].kisstype == KISS_TCP
          && stricmp(l1port[p].device, endpoint) == 0)
        device = p;
  if (!error && device >= 0)
    for (p = 0; p < L2PNUM; ++p)
      if (l1ptab[p] == device && tcpkiss_channels[p] == channel)
        error = "Duplicate KISS channel.";
  if (!error && device < 0)
  {
    for (device = 0; device < L1PNUM; ++device)
      if (!l1port[device].port_active)
        break;
    if (device == L1PNUM)
      error = "No free interface.";
    else
    {
      int result;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      result = getaddrinfo(host, NULL, &hints, &address);
      if (result != 0)
      {
        printf("TCPKISS %s: address lookup failed (%d)\n", endpoint, result);
        error = "Address lookup failed.";
      }
    }
    if (!error)
    {
      TCPKISS *c = &tcpkiss_connections[device];
      memset(c, 0, sizeof(*c));
      c->socket = KISS_INVALID;
      c->address = *(struct sockaddr_in *)address->ai_addr;
      c->address.sin_port = htons(tcpport);
      c->since = tic10 - KISS_RETRY;
      l1port[device].port_active = TRUE;
      l1port[device].kisstype = KISS_TCP;
      l1port[device].kisslink = -1; /* Socket is owned here, not by serial I/O. */
      strcpy(l1port[device].device, endpoint);
      l1port[device].tnn_lockfile[0] = 0;
      l2ptab[device] = port;
      if (device > max_device)
        max_device = device;
      ++used_l1ports;
      kiss_active = TRUE;
    }
  }
  if (address)
    freeaddrinfo(address);
  mbp = getmbp();
  if (error)
    putprintf(mbp, "%s\rSyntax: ATTACH PORT=n KTYP=TCPKISS DEVICE=host:port [CHANNEL=0..15]\r", error);
  else
  {
    l1ptab[port] = device;
    tcpkiss_channels[port] = channel;
    commandflag[port] = TRUE;
    putprintf(mbp, "Port %d Attached (TCPKISS channel %d, reconnect enabled).\r", port, channel);
  }
  prompt(mbp);
  seteom(mbp);
  return TRUE;
}

BOOLEAN tcpkiss_detach(int port)
{
  int device = l1ptab[port], other;
  MBHEAD *mbp;
  if (l1port[device].kisstype != KISS_TCP)
    return FALSE;
  l1ptab[port] = -1;
  tcpkiss_channels[port] = 0;
  tcpkiss_connections[device].in_frame = FALSE;
  tcpkiss_connections[device].rxlen = 0;
  tcpkiss_connections[device].escape = FALSE;
  tcpkiss_connections[device].invalid = FALSE;
  for (other = 0; other < L2PNUM; ++other)
    if (l1ptab[other] == device)
      break;
  if (other < L2PNUM)
    l2ptab[device] = other;
  else
  {
    TCPKISS *c = &tcpkiss_connections[device];
    if (c->socket != KISS_INVALID)
      kiss_close(c->socket);
    c->socket = KISS_INVALID;
    c->state = 0;
    l1port[device].port_active = FALSE;
    l1port[device].kisstype = KISS_NIX;
    l1port[device].device[0] = 0;
    l2ptab[device] = -1;
    --used_l1ports;
    while (max_device >= 0 && !l1port[max_device].port_active)
      --max_device;
  }
  mbp = getmbp();
  putprintf(mbp, "Port %d Detach.\r", port);
  prompt(mbp);
  seteom(mbp);
  return TRUE;
}

void tcpkiss_dump(MBHEAD *mbp, int port)
{
  putprintf(mbp, "KTYP=TCPKISS DEVICE=%s CHANNEL=%d",
            l1port[l1ptab[port]].device, tcpkiss_channels[port]);
}

void tcpkiss_exit(void)
{
  int device;
  for (device = 0; device <= max_device; ++device)
    if (l1port[device].port_active && l1port[device].kisstype == KISS_TCP)
    {
      TCPKISS *c = &tcpkiss_connections[device];
      if (c->socket != KISS_INVALID)
        kiss_close(c->socket);
      c->socket = KISS_INVALID;
      c->state = 0;
    }
}
