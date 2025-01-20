#include "udp.h"

#include "lwip/sockets.h"

int g_sock = -1;

/// @brief Initialize the UDP client.
void initialize_udp_client(void) {
  g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  ESP_ERROR_CHECK(g_sock >= 0 ? ESP_OK : ESP_FAIL);

  struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
  setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}