#include "port_touch.h"
#include "port_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOUCH";

static i2c_master_dev_handle_t touch_dev_handle;

esp_err_t touch_init(void)
{
    gpio_config_t irq_conf = {
        .pin_bit_mask = (1ULL << FT3168_IRQ_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&irq_conf);

    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << FT3168_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_conf);

    gpio_set_level(FT3168_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(FT3168_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT3168_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    if (i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &touch_dev_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add FT3168 device");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FT3168 Touch initialized (IRQ=GPIO%d, RST=GPIO%d)", FT3168_IRQ_PIN, FT3168_RST_PIN);
    return ESP_OK;
}

bool touch_get_points(uint8_t *points)
{
    uint8_t reg = 0x02;
    uint8_t data[3];
    if (i2c_master_transmit_receive(touch_dev_handle, &reg, 1, data, 3, 1000) != ESP_OK) {
        return false;
    }
    *points = data[0] & 0x0F;
    return (*points > 0);
}
