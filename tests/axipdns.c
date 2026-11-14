#include "axipdns.h"

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <net/if.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FALSE 0
#define TRUE 1

static pthread_mutex_t resolver_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t resolver_condition = PTHREAD_COND_INITIALIZER;
static int resolver_entered;
static int release_resolver;

int __real_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **result);

int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **result)
{
  if (strcmp(node, "ipv6.test") == 0)
    return __real_getaddrinfo("::1", service, hints, result);
  if (strcmp(node, "scoped.test") == 0)
    return __real_getaddrinfo("fe80::1%lo", service, hints, result);
  if (strcmp(node, "dual.test") == 0)
  {
    struct addrinfo *ipv4;
    struct addrinfo *last;
    int error = __real_getaddrinfo("::1", service, hints, result);
    if (error != 0)
      return error;
    error = __real_getaddrinfo("127.0.0.1", service, hints, &ipv4);
    if (error != 0)
    {
      freeaddrinfo(*result);
      return error;
    }
    for (last = *result; last->ai_next != NULL; last = last->ai_next)
      ;
    last->ai_next = ipv4;
    return 0;
  }
  if (strcmp(node, "slow.test") == 0)
  {
    assert(pthread_mutex_lock(&resolver_mutex) == 0);
    resolver_entered = TRUE;
    assert(pthread_cond_broadcast(&resolver_condition) == 0);
    while (!release_resolver)
      assert(pthread_cond_wait(&resolver_condition, &resolver_mutex) == 0);
    assert(pthread_mutex_unlock(&resolver_mutex) == 0);
  }
  else
    return EAI_NONAME;

  return __real_getaddrinfo("127.0.0.1", service, hints, result);
}

static void wait_for_resolver(void)
{
  struct timespec deadline;

  assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
  deadline.tv_sec += 3;
  assert(pthread_mutex_lock(&resolver_mutex) == 0);
  while (!resolver_entered)
  {
    int error = pthread_cond_timedwait(&resolver_condition, &resolver_mutex,
                                       &deadline);
    assert(error == 0);
  }
  assert(pthread_mutex_unlock(&resolver_mutex) == 0);
}

static int collect_until_complete(struct axip_dns_result *results,
                                  size_t expected)
{
  struct axip_dns_result collected[AXIP_DNS_MAX_HOSTS];
  size_t count;
  size_t total = 0;
  int complete = FALSE;
  int attempt;

  for (attempt = 0; attempt < 300 && !complete; ++attempt)
  {
    assert(axip_dns_collect(collected, AXIP_DNS_MAX_HOSTS,
                            &count, &complete) == 0);
    assert(total + count <= expected);
    memcpy(results + total, collected, count * sizeof(*collected));
    total += count;
    if (!complete)
    {
      struct timespec pause = {0, 10000000};
      (void)nanosleep(&pause, NULL);
    }
  }

  assert(complete);
  assert(total == expected);
  return (int)total;
}

int main(void)
{
  const char *hostnames[] = {"slow.test", "missing.test", "ipv6.test", "dual.test",
                             "scoped.test"};
  const char *second_batch[] = {"missing.test"};
  struct axip_dns_result results[AXIP_DNS_MAX_HOSTS];
  size_t count;
  int complete;

  assert(axip_dns_submit(hostnames, 5) == 0);
  assert(axip_dns_submit(second_batch, 1) == EBUSY);
  wait_for_resolver();

  assert(axip_dns_collect(results, AXIP_DNS_MAX_HOSTS,
                          &count, &complete) == 0);
  assert(count == 0 && !complete);

  assert(pthread_mutex_lock(&resolver_mutex) == 0);
  release_resolver = TRUE;
  assert(pthread_cond_broadcast(&resolver_condition) == 0);
  assert(pthread_mutex_unlock(&resolver_mutex) == 0);

  assert(collect_until_complete(results, 5) == 5);
  assert(results[0].index == 0 && results[0].resolved);
  assert(results[0].address.ss_family == AF_INET);
  assert(ntohl(((struct sockaddr_in *)&results[0].address)->sin_addr.s_addr)
         == 0x7f000001);
  assert(results[1].index == 1 && !results[1].resolved);
  assert(results[2].index == 2 && results[2].resolved);
  assert(results[2].address.ss_family == AF_INET6);
  assert(IN6_IS_ADDR_LOOPBACK(
      &((struct sockaddr_in6 *)&results[2].address)->sin6_addr));
  assert(results[3].index == 3 && results[3].resolved);
  assert(results[3].address.ss_family == AF_INET);
  assert(results[4].index == 4 && results[4].resolved);
  assert(results[4].address.ss_family == AF_INET6);
  assert(if_nametoindex("lo") != 0);
  assert(((struct sockaddr_in6 *)&results[4].address)->sin6_scope_id
         == if_nametoindex("lo"));

  assert(axip_dns_submit(second_batch, 1) == 0);
  assert(collect_until_complete(results, 1) == 1);
  assert(results[0].index == 0 && !results[0].resolved);

  puts("AXIP DNS worker regressions passed");
  return 0;
}
