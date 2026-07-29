#include "port_rtc.h"
#include "port_i2c.h"
#include "esp_log.h"

static const char *TAG = "RTC";

static i2c_master_dev_handle_t rtc_dev_handle;

static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

esp_err_t rtc_init(void)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    if (i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &rtc_dev_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add PCF85063 device");
        return ESP_FAIL;
    }

    uint8_t ctrl = 0x00;
    uint8_t reg = 0x00;
    if (i2c_master_transmit_receive(rtc_dev_handle, &reg, 1, &ctrl, 1, 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read control register");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "PCF85063 RTC initialized");
    return ESP_OK;
}

esp_err_t rtc_get_time(rtc_time_t *time)
{
    uint8_t reg = 0x04;
    uint8_t data[7];
    if (i2c_master_transmit_receive(rtc_dev_handle, &reg, 1, data, 7, 1000) != ESP_OK) {
        return ESP_FAIL;
    }

    time->second = bcd_to_dec(data[0] & 0x7F);
    time->minute = bcd_to_dec(data[1] & 0x7F);
    time->hour = bcd_to_dec(data[2] & 0x3F);
    time->day = bcd_to_dec(data[3] & 0x3F);
    time->month = bcd_to_dec(data[5] & 0x1F);
    time->year = 2000 + bcd_to_dec(data[6]);

    return ESP_OK;
}

esp_err_t rtc_set_time(rtc_time_t *time)
{
    uint8_t data[8] = {0x04, dec_to_bcd(time->second), dec_to_bcd(time->minute),
                        dec_to_bcd(time->hour), dec_to_bcd(time->day), 0, dec_to_bcd(time->month),
                        dec_to_bcd(time->year - 2000)};

    if (i2c_master_transmit(rtc_dev_handle, data, 8, 1000) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
