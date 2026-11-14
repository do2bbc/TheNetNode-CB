#include "axipdns.h"

#include <assert.h>
#include <errno.h>
#include <netdb.h>
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
  const char *hostnames[] = {"slow.test", "missing.test"};
  const char *second_batch[] = {"missing.test"};
  struct axip_dns_result results[AXIP_DNS_MAX_HOSTS];
  size_t count;
  int complete;

  assert(axip_dns_submit(hostnames, 2) == 0);
  assert(axip_dns_submit(second_batch, 1) == EBUSY);
  wait_for_resolver();

  assert(axip_dns_collect(results, AXIP_DNS_MAX_HOSTS,
                          &count, &complete) == 0);
  assert(count == 0 && !complete);

  assert(pthread_mutex_lock(&resolver_mutex) == 0);
  release_resolver = TRUE;
  assert(pthread_cond_broadcast(&resolver_condition) == 0);
  assert(pthread_mutex_unlock(&resolver_mutex) == 0);

  assert(collect_until_complete(results, 2) == 2);
  assert(results[0].index == 0 && results[0].resolved);
  assert(ntohl(results[0].address.s_addr) == 0x7f000001);
  assert(results[1].index == 1 && !results[1].resolved);

  assert(axip_dns_submit(second_batch, 1) == 0);
  assert(collect_until_complete(results, 1) == 1);
  assert(results[0].index == 0 && !results[0].resolved);

  puts("AXIP DNS worker regressions passed");
  return 0;
}
