#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

typedef void (*DNSCallback)(const char *hostname, struct addrinfo *result, int error, gpointer data);
void dns_start(void);
void dns_lookup(const char *hostname, DNSCallback callback, gpointer data);
void dns_stop(void);