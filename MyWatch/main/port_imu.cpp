#include "port_imu.h"
#include "bsp/esp-bsp.h"
#include "qmi8658.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "IMU";

static qmi8658_dev_t qmi8658_dev;
static bool imu_initialized = false;

esp_err_t imu_init(void)
{
    if (imu_initialized) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle");
        return ESP_FAIL;
    }

    esp_err_t ret = qmi8658_init(&qmi8658_dev, bus_handle, QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize QMI8658: %s", esp_err_to_name(ret));
        return ret;
    }

    qmi8658_set_accel_range(&qmi8658_dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&qmi8658_dev, QMI8658_ACCEL_ODR_500HZ);
    qmi8658_set_accel_unit_mps2(&qmi8658_dev, true);
    qmi8658_write_register(&qmi8658_dev, QMI8658_CTRL5, 0x03);

    imu_initialized = true;
    ESP_LOGI(TAG, "QMI8658 IMU initialized");
    return ESP_OK;
}

esp_err_t imu_read_data(imu_data_t *data)
{
    if (!imu_initialized || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    qmi8658_data_t qmi_data;
    esp_err_t ret = qmi8658_read_sensor_data(&qmi8658_dev, &qmi_data);
    if (ret != ESP_OK) {
        return ret;
    }

    data->acc_x = qmi_data.accelX;
    data->acc_y = qmi_data.accelY;
    data->acc_z = qmi_data.accelZ;
    data->gyro_x = qmi_data.gyroX;
    data->gyro_y = qmi_data.gyroY;
    data->gyro_z = qmi_data.gyroZ;

    return ESP_OK;
}
