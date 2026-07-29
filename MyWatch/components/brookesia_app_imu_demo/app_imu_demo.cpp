#include "app_imu_demo.hpp"
#include "bsp/esp-bsp.h"
#include "qmi8658.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "esp_lib_utils.h"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "IMU_DEMO"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

#define APP_NAME "IMU"

namespace esp_brookesia::apps {

static qmi8658_dev_t *s_imu_dev = nullptr;

IMUDemo *IMUDemo::_instance = nullptr;

IMUDemo *IMUDemo::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new IMUDemo();
    }
    return _instance;
}

IMUDemo::IMUDemo():
    App(APP_NAME, nullptr, false)
{
}

IMUDemo::~IMUDemo()
{
}

bool IMUDemo::run(void)
{
    ESP_UTILS_LOGD("Run IMU Demo");

    if (s_imu_dev == nullptr) {
        i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
        if (bus_handle != nullptr) {
            s_imu_dev = new qmi8658_dev_t;
            if (qmi8658_init(s_imu_dev, bus_handle, QMI8658_ADDRESS_HIGH) == ESP_OK) {
                qmi8658_set_accel_range(s_imu_dev, QMI8658_ACCEL_RANGE_8G);
                qmi8658_set_accel_odr(s_imu_dev, QMI8658_ACCEL_ODR_500HZ);
                qmi8658_set_accel_unit_mps2(s_imu_dev, true);
                qmi8658_write_register(s_imu_dev, QMI8658_CTRL5, 0x03);
                ESP_UTILS_LOGI("IMU initialized");
            } else {
                delete s_imu_dev;
                s_imu_dev = nullptr;
                ESP_LOGE("IMU_DEMO", "QMI8658 init failed");
            }
        }
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "IMU Sensor");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    lv_obj_t *data_label = lv_label_create(scr);
    lv_label_set_text(data_label, s_imu_dev ? "Reading data..." : "IMU not found");
    lv_obj_align(data_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(data_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(data_label, &lv_font_montserrat_16, 0);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Back");

    if (s_imu_dev) {
        lv_timer_create([](lv_timer_t *t) {
            lv_obj_t *label = (lv_obj_t *)t->user_data;
            qmi8658_data_t data;
            if (qmi8658_read_sensor_data(s_imu_dev, &data) == ESP_OK) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "Accel X: %.2f m/s2\n"
                    "Accel Y: %.2f m/s2\n"
                    "Accel Z: %.2f m/s2\n\n"
                    "Gyro X: %.2f rad/s\n"
                    "Gyro Y: %.2f rad/s\n"
                    "Gyro Z: %.2f rad/s",
                    data.accelX, data.accelY, data.accelZ,
                    data.gyroX, data.gyroY, data.gyroZ);
                lv_label_set_text(label, buf);
            }
        }, 100, data_label);
    }

    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, true);

    return true;
}

bool IMUDemo::back(void)
{
    ESP_UTILS_LOGD("Back IMU Demo");
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

extern "C" {

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, IMUDemo, APP_NAME, []()
{
    return std::shared_ptr<IMUDemo>(IMUDemo::requestInstance(), [](IMUDemo *p) {});
})

}

}
