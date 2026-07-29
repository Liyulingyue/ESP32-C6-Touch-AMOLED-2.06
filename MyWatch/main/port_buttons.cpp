#include "port_buttons.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUTTONS";

static button_callback_t callback = NULL;
static TaskHandle_t button_task_handle = NULL;

static void button_task(void *arg)
{
    bool boot_last = gpio_get_level(BUTTON_BOOT_PIN);
    bool pwr_last = gpio_get_level(BUTTON_PWR_PIN);
    uint32_t boot_press_time = 0;
    uint32_t pwr_press_time = 0;

    while (1) {
        bool boot_now = gpio_get_level(BUTTON_BOOT_PIN);
        bool pwr_now = gpio_get_level(BUTTON_PWR_PIN);
        uint32_t now = xTaskGetTickCount();

        if (boot_now != boot_last) {
            if (boot_now == 0) {
                boot_press_time = now;
            } else {
                uint32_t duration = now - boot_press_time;
                if (duration < 100 / portTICK_PERIOD_MS) {
                    // Ignore bounce
                } else if (duration > 2000 / portTICK_PERIOD_MS) {
                    if (callback) callback(BUTTON_EVENT_BOOT_LONG_PRESS);
                } else {
                    if (callback) callback(BUTTON_EVENT_BOOT_PRESS);
                }
            }
            boot_last = boot_now;
        }

        if (pwr_now != pwr_last) {
            if (pwr_now == 0) {
                pwr_press_time = now;
            } else {
                uint32_t duration = now - pwr_press_time;
                if (duration < 100 / portTICK_PERIOD_MS) {
                    // Ignore bounce
                } else if (duration > 2000 / portTICK_PERIOD_MS) {
                    if (callback) callback(BUTTON_EVENT_PWR_LONG_PRESS);
                } else {
                    if (callback) callback(BUTTON_EVENT_PWR_PRESS);
                }
            }
            pwr_last = pwr_now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t buttons_init(void)
{
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_BOOT_PIN) | (1ULL << BUTTON_PWR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&btn_conf);

    xTaskCreate(button_task, "buttons", 4096, NULL, 5, &button_task_handle);

    ESP_LOGI(TAG, "Buttons initialized (BOOT=GPIO%d, PWR=GPIO%d)", BUTTON_BOOT_PIN, BUTTON_PWR_PIN);
    return ESP_OK;
}

esp_err_t buttons_register_callback(button_callback_t cb)
{
    callback = cb;
    return ESP_OK;
}
