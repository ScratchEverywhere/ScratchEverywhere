#include <netinet/tcp.h>
#include <network.h>

#define SE_NET_USE_GETHOSTBYNAME
#define SE_NET_RANDOM(buf, len) false

#include <socket_bsd.hpp>
