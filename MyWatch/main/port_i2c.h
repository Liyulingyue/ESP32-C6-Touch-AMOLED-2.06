#ifndef PORT_I2C_H
#define PORT_I2C_H

#include "driver/i2c_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_MASTER_SDA_IO  7
#define I2C_MASTER_SCL_IO  8
#define I2C_MASTER_NUM     0
#define I2C_MASTER_FREQ_HZ 400000

extern i2c_master_bus_handle_t i2c_bus_handle;

esp_err_t i2c_bus_init(void);

#ifdef __cplusplus
}
#endif

#endif
