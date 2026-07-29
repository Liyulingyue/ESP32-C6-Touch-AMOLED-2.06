#ifndef PORT_WIFI_H
#define PORT_WIFI_H

#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

typedef void (*wifi_event_callback_t)(wifi_event_t event, void *data);

esp_err_t wifi_init(void);
esp_err_t wifi_connect(const char *ssid, const char *password);
esp_err_t wifi_disconnect(void);
bool wifi_is_connected(void);
esp_err_t wifi_register_event_callback(wifi_event_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif
