#include "esp_log.h"
#include "esp_wifi.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "global.h"
#include "led_ws2812.h"
#include "wifi.h"

static const char *TAG = "wifi";

#define WIFI_MAXIMUM_RETRY 5
static uint32_t g_wifi_retry_count = 0;

const int WIFI_STARTED_BIT = BIT0;
const int WIFI_CONNECTING_BIT = BIT1;
const int WIFI_CONNECTED_BIT = BIT2;
EventGroupHandle_t g_wifi_event_group;

/// @brief Wifi event handler.
/// @param arg The argument.
/// @param event_base The event base.
/// @param event_id The event ID.
/// @param event_data The event data.
static void wifi_event_handler(
  void* arg,
  esp_event_base_t event_base,
  int32_t event_id,
  void* event_data
) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    xEventGroupSetBits(g_wifi_event_group, WIFI_STARTED_BIT);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (g_wifi_retry_count < WIFI_MAXIMUM_RETRY) {
      esp_wifi_connect();
      g_wifi_retry_count++;
      ESP_LOGI(TAG, "Retry to connect to the AP");
    } else {
      // Set the LED to red.
      write_ws2812(25, 0, 0);

      g_wifi_retry_count = 0;

      ESP_LOGE(TAG, "Connect to the AP failed");
    }

    xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTING_BIT);
    xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    // Set the LED to green.
    write_ws2812(0, 25, 0);

    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    g_wifi_retry_count = 0;

    xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTING_BIT);
    xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

/// @brief Initialize wifi.
void initialize_wifi(void) {
  g_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  g_wifi_netif = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

  ESP_ERROR_CHECK(esp_wifi_start());
}

/// @brief Connect to the wifi.
/// @param ssid The SSID.
/// @param password The password.
/// @param timeout_ms The timeout in milliseconds.
/// @return The result.
esp_err_t connect_wifi(const char *ssid, const char *password, int timeout_ms) {
  // Check if we are already connecting or connected.
  EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);
  if ((bits & WIFI_CONNECTING_BIT) || (bits & WIFI_CONNECTED_BIT)) {
    ESP_LOGE(TAG, "Already connecting or connected to the wifi.");
    return ESP_FAIL;
  }

  wifi_config_t wifi_config = {
    .sta = {
      .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
      .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
      .sae_h2e_identifier = "",
    },
  };
  memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
  memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
  memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
  memcpy(wifi_config.sta.password, password, strlen(password));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  esp_err_t err = esp_wifi_connect();
  if (err == ESP_OK) {
    // Set the LED to blue.
    write_ws2812(0, 0, 25);

    xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTING_BIT);
  }
  return err;
}

/// @brief Disconnect from the wifi.
void disconnect_wifi(void) {
  g_wifi_retry_count = WIFI_MAXIMUM_RETRY;
  esp_wifi_disconnect();
}

/// @brief Get the wifi credentials.
/// @param ssid The SSID.
/// @param password The password.
/// @return The result.
esp_err_t get_wifi_credentials(char *ssid, char *password) {
  wifi_config_t wifi_config = { 0 };
  esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  if (err != ESP_OK)
    return err;

  memcpy(ssid, wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid));
  memcpy(password, wifi_config.sta.password, sizeof(wifi_config.sta.password));

  return ESP_OK;
}

/// @brief Automatically connect to the wifi.
/// @return The result.
esp_err_t auto_connect_wifi(void) {
  // // Iterate through all NVS entries.
  // nvs_iterator_t it;
  // ESP_ERROR_CHECK(nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it));
  // if (it == NULL) {
  //   ESP_LOGW(TAG, "Failed to find any NVS entry. We will NOT auto-connect.");
  //   return ESP_OK;
  // }
  // do {
  //   nvs_entry_info_t info;
  //   nvs_entry_info(it, &info);
  //   ESP_LOGI(TAG, "Namespace: %s, Key: %s, Type: %d", info.namespace_name, info.key, info.type);
  //   nvs_entry_next(&it);
  // } while (it != NULL);

  // Try to get the SSID and password from the NVS.
  wifi_config_t wifi_config = { 0 };
  esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to get the latest wifi config. We will NOT auto-connect.");
    return ESP_OK;
  }
  // If ssid is empty, we will NOT auto-connect.
  if (strlen((const char*) wifi_config.sta.ssid) == 0) {
    ESP_LOGW(TAG, "SSID is empty. We will NOT auto-connect.");
    return ESP_OK;
  }

  err = esp_wifi_connect();
  if (err == ESP_OK) {
    // Set the LED to blue.
    write_ws2812(0, 0, 25);

    xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTING_BIT);
  }
  return err;
}