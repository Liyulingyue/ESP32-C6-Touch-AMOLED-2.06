#include "port_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"

static const char *TAG = "WIFI";

static bool connected = false;
static wifi_event_callback_t event_callback = NULL;
static esp_netif_t *netif = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        connected = true;
        ESP_LOGI(TAG, "WiFi connected");
        if (event_callback) event_callback(WIFI_EVENT_STA_CONNECTED, NULL);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;
        ESP_LOGI(TAG, "WiFi disconnected");
        if (event_callback) event_callback(WIFI_EVENT_STA_DISCONNECTED, NULL);
    }
}

esp_err_t wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t handler;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &handler));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_LOGI(TAG, "WiFi initialized");
    return ESP_OK;
}

esp_err_t wifi_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    return esp_wifi_start();
}

esp_err_t wifi_disconnect(void)
{
    return esp_wifi_disconnect();
}

bool wifi_is_connected(void)
{
    return connected;
}

esp_err_t wifi_register_event_callback(wifi_event_callback_t callback)
{
    event_callback = callback;
    return ESP_OK;
}
