#ifndef PORT_RTC_H
#define PORT_RTC_H

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCF85063_I2C_ADDR 0x51

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} rtc_time_t;

esp_err_t rtc_init(void);
esp_err_t rtc_get_time(rtc_time_t *time);
esp_err_t rtc_set_time(rtc_time_t *time);

#ifdef __cplusplus
}
#endif

#endif
