#ifndef PORT_TOUCH_H
#define PORT_TOUCH_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FT3168_I2C_ADDR 0x38
#define FT3168_IRQ_PIN  GPIO_NUM_15
#define FT3168_RST_PIN  GPIO_NUM_10

esp_err_t touch_init(void);
bool touch_get_points(uint8_t *points);

#ifdef __cplusplus
}
#endif

#endif
