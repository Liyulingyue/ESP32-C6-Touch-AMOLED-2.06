#ifndef PORT_IMU_H
#define PORT_IMU_H

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QMI8658_I2C_ADDR 0x6B

typedef struct {
    float acc_x, acc_y, acc_z;
    float gyro_x, gyro_y, gyro_z;
} imu_data_t;

esp_err_t imu_init(void);
esp_err_t imu_read_data(imu_data_t *data);

#ifdef __cplusplus
}
#endif

#endif
