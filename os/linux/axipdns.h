#ifndef AXIPDNS_H
#define AXIPDNS_H

#include <netinet/in.h>
#include <stddef.h>

#define AXIP_DNS_MAX_HOSTS 129 /* 128 routes and one default route */
#define AXIP_DNS_HOSTNAME_LENGTH 64

struct axip_dns_result
{
  size_t index;
  int resolved;
  struct in_addr address;
};

int axip_dns_submit(const char *const *hostnames, size_t count);
int axip_dns_collect(struct axip_dns_result *results, size_t capacity,
                     size_t *result_count, int *complete);

#endif
