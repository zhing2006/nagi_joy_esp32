#include <string.h>

#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "lwip/sockets.h"

#include "global.h"
#include "commands.h"
#include "wifi.h"

static const char* TAG = "commands";

/// @brief List saved wifi networks.
static struct {
  struct arg_end* end;
} list_args;

/// @brief Join command information.
static struct {
  struct arg_str* ssid;
  struct arg_str* password;
  struct arg_int* timeout;
  struct arg_end* end;
} join_args;

/// @brief Disconnect command information.
static struct {
  struct arg_end* end;
} disconnect_args;

/// @brief Save command information.
static struct {
  struct arg_int* index;
  struct arg_end* end;
} save_args;

/// @brief Server command information.
static struct {
  struct arg_str* host;
  struct arg_int* port;
  struct arg_end* end;
} server_args;

/// @brief List command.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return The result of the command.
static int list_command(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&list_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, list_args.end, argv[0]);
    return 1;
  }

  // The wifi networks are saved in the "/data/wifi0.txt" and "/data/wifi1.txt" files.
  bool found = false;
  const char* wifi_files[] = {"/data/wifi0.txt", "/data/wifi1.txt"};
  for (int i = 0; i < 2; i++) {
    FILE *f = fopen(wifi_files[i], "r");
    if (f == NULL) {
      continue;
    }
    found = true;

    char line[128];

    fread(line, 1, 128, f);
    fclose(f);

    const char* ssid_begin = line;
    const char* ssid_end = strchr(line, '\t');
    const char* password_begin = ssid_end + 1;
    const char* password_end = strchr(password_begin, '\n');

    ESP_LOGI(TAG, "%d. SSID: %.*s, Password: %.*s", i + 1, ssid_end - ssid_begin, ssid_begin, password_end - password_begin, password_begin);
  }
  if (!found) {
    ESP_LOGI(TAG, "No saved networks.");
  }

  // The server address is saved in the "/data/server.txt" file.
  FILE *f = fopen("/data/server.txt", "r");
  if (f != NULL) {
    char host[64];
    int port;
    if (fscanf(f, "%63[^:]:%d", host, &port) == 2) {
      ESP_LOGI(TAG, "Server address: %s:%d", host, port);
    }
    fclose(f);
  } else {
    ESP_LOGI(TAG, "No server.");
  }

  return 0;
}

/// @brief Join command.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return The result of the command.
static int join_command(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&join_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, join_args.end, argv[0]);
    return 1;
  }

  // Check if connected.
  EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);
  if (bits & WIFI_CONNECTING_BIT) {
    ESP_LOGW(TAG, "Already connecting.");
    return 1;
  }
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGW(TAG, "Already connected.");
    return 1;
  }

  if (join_args.timeout->count == 0) {
    join_args.timeout->ival[0] = 10000;
  }

  esp_err_t err = connect_wifi(
    join_args.ssid->sval[0],
    join_args.password->sval[0],
    join_args.timeout->ival[0]
  );
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Connection Failed.");
    return 1;
  }

  return 0;
}

/// @brief Disconnect command.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return The result of the command.
static int disconnect_command(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&disconnect_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, disconnect_args.end, argv[0]);
    return 1;
  }

  // Check if connected.
  EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);
  if (!(bits & WIFI_CONNECTED_BIT)) {
    ESP_LOGW(TAG, "Not connected.");
    return 1;
  }

  disconnect_wifi();

  return 0;
}

/// @brief Save command.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return The result of the command.
static int save_command(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&save_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, save_args.end, argv[0]);
    return 1;
  }

  // Check if connected.
  EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);
  if (!(bits & WIFI_CONNECTED_BIT)) {
    ESP_LOGE(TAG, "Not connected.");
    return 1;
  }

  // The wifi networks are saved in the "/data/wifi0.txt" and "/data/wifi1.txt" files.
  const int index = save_args.index->ival[0];
  if (index < 0 || index > 1) {
    ESP_LOGE(TAG, "Invalid index %d.", index);
    return 1;
  }
  const char* wifi_files[] = {"/data/wifi0.txt", "/data/wifi1.txt"};
  FILE *f = fopen(wifi_files[index], "w");
  if (f == NULL) {
    ESP_LOGW(TAG, "Failed to save the network.");
    return 1;
  }

  // Get the current wifi network SSID and password.
  char wifi_ssid[33] = {0};
  char wifi_password[65] = {0};

  if (get_wifi_credentials(wifi_ssid, wifi_password) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get the wifi credentials.");
    return 1;
  }

  fprintf(f, "%s\t%s\n", wifi_ssid, wifi_password);
  fclose(f);

  return 0;
}

/// @brief Server command.
/// @param argc The number of arguments.
/// @param argv The arguments.
/// @return The result of the command.
static int server_command(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&server_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, server_args.end, argv[0]);
    return 1;
  }

  if (server_args.port->count == 0) {
    server_args.port->ival[0] = 8888;
  }

  if (server_args.host->count == 0) {
    ESP_LOGE(TAG, "Host is required.");
    return 1;
  }

  const char* host = server_args.host->sval[0];
  const int port = server_args.port->ival[0];

  // Check if the host is a valid IP address.
  esp_ip4_addr_t ip;
  if (inet_pton(AF_INET, host, &ip) != 1) {
    ESP_LOGE(TAG, "Invalid host.");
    return 1;
  }

  // Store global variable.
  g_server_addr.sin_family = AF_INET;
  g_server_addr.sin_port = htons(port);
  g_server_addr.sin_addr.s_addr = ip.addr;

  // Write host:port to the "/data/server.txt" file.
  FILE *f = fopen("/data/server.txt", "w");
  if (f == NULL) {
    ESP_LOGW(TAG, "Failed to save the server address.");
    return 1;
  }
  fprintf(f, "%s:%d\n", host, port);
  fclose(f);

  return 0;
}

/// @brief Register the user commands.
/// @return The result of the registration.
esp_err_t register_user_commands(void) {
  // Register the list command.
  list_args.end = arg_end(0);

  const esp_console_cmd_t list_console_cmd = {
    .command = "list",
    .help = "List saved WiFi networks.",
    .func = &list_command,
    .argtable = &list_args
  };
  esp_err_t err = esp_console_cmd_register(&list_console_cmd);
  if (err != ESP_OK)
    return err;

  // Register the join command.
  join_args.password = arg_str0(NULL, NULL, "<string>", "The password of the WiFi network.");
  join_args.ssid = arg_str1(NULL, NULL, "<string>", "The SSID of the WiFi network.");
  join_args.timeout = arg_int0(NULL, "timeout", "<int>", "The connection timeout in milliseconds.");
  join_args.end = arg_end(2);

  const esp_console_cmd_t join_console_cmd = {
    .command = "join",
    .help = "Join a WiFi network.",
    .func = &join_command,
    .argtable = &join_args
  };
  err = esp_console_cmd_register(&join_console_cmd);
  if (err != ESP_OK)
    return err;

  // Register the disconnect command.
  disconnect_args.end = arg_end(0);

  const esp_console_cmd_t disconnect_console_cmd = {
    .command = "disconnect",
    .help = "Disconnect from the WiFi network.",
    .func = &disconnect_command,
    .argtable = &disconnect_args
  };
  err = esp_console_cmd_register(&disconnect_console_cmd);
  if (err != ESP_OK)
    return err;

  // Register the save command.
  save_args.index = arg_int1(NULL, NULL, "<int>", "The index of the storage slot.");
  save_args.end = arg_end(1);

  const esp_console_cmd_t save_console_cmd = {
    .command = "save",
    .help = "Save the current WiFi network.",
    .func = &save_command,
    .argtable = &save_args
  };
  err = esp_console_cmd_register(&save_console_cmd);
  if (err != ESP_OK)
    return err;

  // Register the server command.
  server_args.host = arg_str1(NULL, NULL, "<string>", "The host of the server.");
  server_args.port = arg_int0(NULL, "port", "<int>", "The port of the server.");
  server_args.end = arg_end(2);

  const esp_console_cmd_t server_console_cmd = {
    .command = "server",
    .help = "Set the server address.",
    .func = &server_command,
    .argtable = &server_args
  };
  err = esp_console_cmd_register(&server_console_cmd);
  if (err != ESP_OK)
    return err;

  return ESP_OK;
}

#include "commands.h"