#ifndef __WIFI_H__
#define __WIFI_H__

struct EventGroupDef_t;
typedef struct EventGroupDef_t* EventGroupHandle_t;
typedef int esp_err_t;

/// @brief Wifi event group.
extern EventGroupHandle_t g_wifi_event_group;
/// @brief Wifi started bit.
extern const int WIFI_STARTED_BIT;
/// @brief Wifi connecting bit.
extern const int WIFI_CONNECTING_BIT;
/// @brief Wifi connected bit.
extern const int WIFI_CONNECTED_BIT;

/// @brief Initialize wifi.
void initialize_wifi(void);

/// @brief Connect to the wifi.
/// @param ssid The SSID.
/// @param password The password.
/// @param timeout_ms The timeout in milliseconds.
/// @return The result.
esp_err_t connect_wifi(const char *ssid, const char *password, int timeout_ms);

/// @brief Disconnect from the wifi.
void disconnect_wifi(void);

/// @brief Get the wifi credentials.
/// @param ssid The SSID.
/// @param password The password.
/// @return The result.
esp_err_t get_wifi_credentials(char *ssid, char *password);

/// @brief Automatically connect to the wifi.
/// @return The result.
esp_err_t auto_connect_wifi(void);

#endif // __WIFI_H__