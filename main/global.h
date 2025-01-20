#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "led_ws2812.h"
#include "esp_netif_types.h"

struct sockaddr_in;

/// @brief The LED strip handle.
extern led_strip_handle_t g_led_strip;

/// @brief The wifi network interface.
extern esp_netif_t* g_wifi_netif;

/// @brief The server address.
extern struct sockaddr_in g_server_addr;

#endif // __GLOBAL_H__