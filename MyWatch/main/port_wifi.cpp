#include "port_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "esp_wifi.h"
#include "esp_http_server.h"

static const char *TAG = "WIFI";
static bool connected = false;
static bool initialized = false;
static bool ap_running = false;
static wifi_event_callback_t event_callback = NULL;
static esp_netif_t *netif = NULL;
static esp_netif_t *ap_netif = NULL;
static httpd_handle_t httpd_handle = NULL;

static esp_err_t wifi_init_internal(void);

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
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "AP: Station connected");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "AP: Station disconnected");
    }
}

esp_err_t wifi_init(void)
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
        ESP_LOGI(TAG, "WiFi already initialized");
        initialized = true;
        return ESP_OK;
    }
    if (initialized) {
        return ESP_OK;
    }
    return wifi_init_internal();
}

static esp_err_t wifi_init_internal(void)
{
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %d", ret);
        return ret;
    }
    
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %d", ret);
        return ret;
    }
    
    netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", ret);
        return ret;
    }

    esp_event_handler_instance_t handler;
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_handler_instance_register failed: %d", ret);
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "WiFi initialized");
    return ESP_OK;
}

esp_err_t wifi_connect(const char *ssid, const char *password)
{
    wifi_init();
    
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
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

static esp_err_t httpd_handler_config_get(httpd_req_t *req)
{
    const char *html_response = 
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<title>WiFi Config</title>"
        "<style>"
        "body{font-family:Arial;max-width:400px;margin:40px auto;padding:20px}"
        "input{width:100%;padding:12px;margin:8px 0;box-sizing:border-box}"
        "button{width:100%;padding:14px;background:#4CAF50;color:white;border:none;border-radius:4px;cursor:pointer}"
        "button:hover{background:#45a049}"
        ".status{margin-top:20px;padding:10px;background:#f0f0f0;border-radius:4px}"
        "</style></head><body>"
        "<h2>WiFi Configuration</h2>"
        "<form id=\"configForm\">"
        "<input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"WiFi SSID\" required>"
        "<input type=\"password\" id=\"password\" name=\"password\" placeholder=\"WiFi Password\" required>"
        "<button type=\"submit\">Connect</button>"
        "</form>"
        "<div class=\"status\" id=\"status\">Ready to configure</div>"
        "<script>"
        "document.getElementById('configForm').onsubmit=function(e){"
        "e.preventDefault();"
        "var s=document.getElementById('ssid').value;"
        "var p=document.getElementById('password').value;"
        "fetch('/config?ssid='+encodeURIComponent(s)+'&password='+encodeURIComponent(p))"
        ".then(r=>r.text()).then(d=>document.getElementById('status').innerHTML='<b>Connecting to: '+s+'</b><br>Please wait...');"
        "};"
        "</script></body></html>";
    
    httpd_resp_send(req, html_response, strlen(html_response));
    return ESP_OK;
}

static esp_err_t httpd_handler_config_set(httpd_req_t *req)
{
    char buf[256];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char ssid[64] = {0};
        char password[64] = {0};
        
        if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK &&
            httpd_query_key_value(buf, "password", password, sizeof(password)) == ESP_OK) {
            
            ESP_LOGI(TAG, "Config received - SSID: %s", ssid);
            
            wifi_disconnect();
            esp_wifi_set_mode(WIFI_MODE_STA);
            wifi_connect(ssid, password);
            
            const char *response = "OK - Connecting to WiFi. Please wait...";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }
    }
    
    const char *response = "ERROR - Invalid parameters";
    httpd_resp_send(req, response, strlen(response));
    return ESP_FAIL;
}

esp_err_t wifi_start_ap_config(void)
{
    ESP_LOGI(TAG, "Starting AP config...");
    
    if (ap_running) {
        ESP_LOGW(TAG, "AP already running");
        return ESP_OK;
    }
    
    wifi_init();
    
    esp_err_t ret;
    
    ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
    }
    
    wifi_config_t ap_config = {};
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    strcpy((char *)ap_config.ap.ssid, "ESP32-C6-Config");
    ap_config.ap.ssid_len = 14;
    
    ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %d", ret);
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %d", ret);
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %d", ret);
        return ret;
    }
    
    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_cfg.server_port = 80;
    
    httpd_handle_t server = NULL;
    ret = httpd_start(&server, &httpd_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %d", ret);
        return ret;
    }
    
    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = httpd_handler_config_get,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_get);
    
    httpd_uri_t uri_config = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = httpd_handler_config_set,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_config);
    
    httpd_handle = server;
    
    ap_running = true;
    ESP_LOGI(TAG, "AP started - Connect to ESP32-C6-Config and open 192.168.4.1");
    
    return ESP_OK;
}

esp_err_t wifi_stop_ap_config(void)
{
    if (httpd_handle) {
        httpd_stop(httpd_handle);
        httpd_handle = NULL;
    }
    
    if (ap_netif) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    
    ap_running = false;
    esp_wifi_set_mode(WIFI_MODE_STA);
    
    ESP_LOGI(TAG, "AP stopped");
    return ESP_OK;
}

bool wifi_is_ap_running(void)
{
    return ap_running;
}
