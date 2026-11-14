#include "axipdns.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

struct axip_dns_entry
{
  char hostname[AXIP_DNS_HOSTNAME_LENGTH];
  struct sockaddr_storage address;
  int resolved;
  int ready;
};

static pthread_mutex_t axip_dns_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct axip_dns_entry axip_dns_entries[AXIP_DNS_MAX_HOSTS];
static size_t axip_dns_count;
static size_t axip_dns_next_result;
static int axip_dns_active;
static int axip_dns_worker_done;

static void axip_dns_lock(void)
{
  int error = pthread_mutex_lock(&axip_dns_mutex);

  if (error != 0)
  {
    fprintf(stderr, "AXIP DNS: pthread_mutex_lock failed: %s\n",
            strerror(error));
    abort();
  }
}

static void axip_dns_unlock(void)
{
  int error = pthread_mutex_unlock(&axip_dns_mutex);

  if (error != 0)
  {
    fprintf(stderr, "AXIP DNS: pthread_mutex_unlock failed: %s\n",
            strerror(error));
    abort();
  }
}

int axip_dns_resolve(const char *hostname, struct sockaddr_storage *result)
{
  struct addrinfo hints;
  struct addrinfo *addresses = NULL;
  struct addrinfo *address;
  struct addrinfo *chosen = NULL;
  int error;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  error = getaddrinfo(hostname, NULL, &hints, &addresses);
  if (error != 0)
    return error;

  for (address = addresses; address != NULL; address = address->ai_next)
  {
    if (address->ai_family == AF_INET
        && address->ai_addrlen == sizeof(struct sockaddr_in))
    {
      chosen = address;
      break;
    }
    if (chosen == NULL && address->ai_family == AF_INET6
        && address->ai_addrlen == sizeof(struct sockaddr_in6))
      chosen = address;
  }
  memset(result, 0, sizeof(*result));
  if (chosen != NULL)
    memcpy(result, chosen->ai_addr, chosen->ai_addrlen);
  freeaddrinfo(addresses);
  return chosen != NULL ? 0 : EAI_NONAME;
}

static void *axip_dns_resolve_batch(void *unused)
{
  char hostname[AXIP_DNS_HOSTNAME_LENGTH];
  struct sockaddr_storage resolved_address;
  size_t i;
  int resolved;

  (void)unused;

  for (i = 0; ; ++i)
  {
    axip_dns_lock();
    if (i >= axip_dns_count)
    {
      axip_dns_worker_done = 1;
      axip_dns_unlock();
      return NULL;
    }
    memcpy(hostname, axip_dns_entries[i].hostname, sizeof(hostname));
    axip_dns_unlock();

    memset(&resolved_address, 0, sizeof(resolved_address));
    resolved = axip_dns_resolve(hostname, &resolved_address) == 0;

    axip_dns_lock();
    axip_dns_entries[i].address = resolved_address;
    axip_dns_entries[i].resolved = resolved;
    axip_dns_entries[i].ready = 1;
    axip_dns_unlock();
  }
}

int axip_dns_submit(const char *const *hostnames, size_t count)
{
  pthread_attr_t attributes;
  pthread_t worker;
  size_t i;
  size_t length;
  int error;
  int attributes_initialized = 0;

  if (hostnames == NULL || count == 0 || count > AXIP_DNS_MAX_HOSTS)
    return EINVAL;

  for (i = 0; i < count; ++i)
  {
    if (hostnames[i] == NULL)
      return EINVAL;

    for (length = 0;
         length < AXIP_DNS_HOSTNAME_LENGTH && hostnames[i][length] != '\0';
         ++length)
      ;

    if (length == 0 || length == AXIP_DNS_HOSTNAME_LENGTH)
      return EINVAL;
  }

  axip_dns_lock();
  if (axip_dns_active)
  {
    axip_dns_unlock();
    return EBUSY;
  }

  for (i = 0; i < count; ++i)
  {
    strcpy(axip_dns_entries[i].hostname, hostnames[i]);
    memset(&axip_dns_entries[i].address, 0,
           sizeof(axip_dns_entries[i].address));
    axip_dns_entries[i].resolved = 0;
    axip_dns_entries[i].ready = 0;
  }

  axip_dns_count = count;
  axip_dns_next_result = 0;
  axip_dns_worker_done = 0;
  axip_dns_active = 1;

  error = pthread_attr_init(&attributes);
  if (error == 0)
  {
    attributes_initialized = 1;
    error = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
  }
  if (error == 0)
    error = pthread_create(&worker, &attributes, axip_dns_resolve_batch, NULL);

  if (attributes_initialized)
    (void)pthread_attr_destroy(&attributes);

  if (error != 0)
  {
    axip_dns_active = 0;
    axip_dns_worker_done = 0;
    axip_dns_count = 0;
  }
  axip_dns_unlock();
  return error;
}

int axip_dns_collect(struct axip_dns_result *results, size_t capacity,
                     size_t *result_count, int *complete)
{
  size_t count = 0;
  int error;

  if (results == NULL || result_count == NULL || complete == NULL)
    return EINVAL;

  *result_count = 0;
  *complete = 0;

  error = pthread_mutex_trylock(&axip_dns_mutex);
  if (error == EBUSY)
    return 0;
  if (error != 0)
    return error;

  while (count < capacity
         && axip_dns_next_result < axip_dns_count
         && axip_dns_entries[axip_dns_next_result].ready)
  {
    results[count].index = axip_dns_next_result;
    results[count].resolved =
      axip_dns_entries[axip_dns_next_result].resolved;
    results[count].address =
      axip_dns_entries[axip_dns_next_result].address;
    axip_dns_entries[axip_dns_next_result].ready = 0;
    ++axip_dns_next_result;
    ++count;
  }

  if (axip_dns_active && axip_dns_worker_done
      && axip_dns_next_result == axip_dns_count)
  {
    axip_dns_active = 0;
    axip_dns_worker_done = 0;
    axip_dns_count = 0;
    *complete = 1;
  }

  axip_dns_unlock();
  *result_count = count;
  return 0;
}
