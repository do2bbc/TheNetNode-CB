#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int setup_udp(unsigned short port);
time_t sys_time;
static int ipv6_disabled;

int __real_socket(int domain, int type, int protocol);

int __wrap_socket(int domain, int type, int protocol)
{
  if (ipv6_disabled && domain == AF_INET6)
  {
    errno = EAFNOSUPPORT;
    return -1;
  }
  return __real_socket(domain, type, protocol);
}

void xprintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

int main(void)
{
  struct sockaddr_in6 addr6;
  struct sockaddr_in addr4;
  struct sockaddr_storage source;
  struct timeval timeout = {2, 0};
  socklen_t length;
  int probe, listener, ipv4, ipv6, i, seen4 = 0, seen6 = 0;
  char data;

  probe = socket(AF_INET6, SOCK_DGRAM, 0);
  assert(probe >= 0);
  memset(&addr6, 0, sizeof(addr6));
  addr6.sin6_family = AF_INET6;
  addr6.sin6_addr = in6addr_loopback;
  assert(bind(probe, (struct sockaddr *)&addr6, sizeof(addr6)) == 0);
  length = sizeof(addr6);
  assert(getsockname(probe, (struct sockaddr *)&addr6, &length) == 0);
  close(probe);

  listener = setup_udp(addr6.sin6_port);
  assert(listener >= 0);
  assert(setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
  ipv4 = socket(AF_INET, SOCK_DGRAM, 0);
  ipv6 = socket(AF_INET6, SOCK_DGRAM, 0);
  assert(ipv4 >= 0 && ipv6 >= 0);
  memset(&addr4, 0, sizeof(addr4));
  addr4.sin_family = AF_INET;
  addr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr4.sin_port = addr6.sin6_port;
  assert(sendto(ipv4, "4", 1, 0, (struct sockaddr *)&addr4, sizeof(addr4)) == 1);
  assert(sendto(ipv6, "6", 1, 0, (struct sockaddr *)&addr6, sizeof(addr6)) == 1);

  for (i = 0; i < 2; ++i)
  {
    fd_set readers;
    struct timeval wait = {2, 0};
    FD_ZERO(&readers);
    FD_SET(listener, &readers);
    assert(select(listener + 1, &readers, NULL, NULL, &wait) == 1);
    length = sizeof(source);
    assert(recvfrom(listener, &data, 1, 0, (struct sockaddr *)&source, &length) == 1);
    assert(source.ss_family == AF_INET6);
    if (data == '4')
    {
      assert(IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&source)->sin6_addr));
      seen4 = 1;
    }
    else
    {
      assert(data == '6');
      assert(IN6_IS_ADDR_LOOPBACK(&((struct sockaddr_in6 *)&source)->sin6_addr));
      seen6 = 1;
    }
  }
  assert(seen4 && seen6);
  close(ipv4);
  close(ipv6);
  close(listener);
  ipv6_disabled = 1;
  listener = setup_udp(addr6.sin6_port);
  assert(listener >= 0);
  ipv4 = socket(AF_INET, SOCK_DGRAM, 0);
  assert(ipv4 >= 0);
  assert(sendto(ipv4, "4", 1, 0, (struct sockaddr *)&addr4, sizeof(addr4)) == 1);
  {
    fd_set readers;
    struct timeval wait = {2, 0};
    FD_ZERO(&readers);
    FD_SET(listener, &readers);
    assert(select(listener + 1, &readers, NULL, NULL, &wait) == 1);
  }
  length = sizeof(source);
  assert(recvfrom(listener, &data, 1, 0, (struct sockaddr *)&source, &length) == 1);
  assert(data == '4' && source.ss_family == AF_INET);
  close(ipv4);
  close(listener);
  puts("AXIP UDP dual-stack socket test passed");
  return 0;
}
