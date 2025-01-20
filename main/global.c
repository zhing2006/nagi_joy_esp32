#include <esp_err.h>

#include "global.h"

#include "lwip/sockets.h"

esp_netif_t* g_wifi_netif = NULL;

struct sockaddr_in g_server_addr;