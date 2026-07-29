#include "app_imu_demo.hpp"
#include "bsp/esp-bsp.h"
#include "qmi8658.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "esp_lib_utils.h"
#include <cmath>

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "IMU_DEMO"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

#define APP_NAME "IMU"

namespace esp_brookesia::apps {

static qmi8658_dev_t *s_imu_dev = nullptr;
static lv_timer_t *s_imu_timer = nullptr;
static bool s_imu_calibrated = false;
static float s_accel_offset[3] = {0};
static float s_gyro_offset[3] = {0};
static int s_calib_samples = 0;
static float s_calib_accel_sum[3] = {0};
static float s_calib_gyro_sum[3] = {0};
static float s_gyro_filtered[3] = {0};
static constexpr float FILTER_ALPHA = 0.15f;

static lv_obj_t *s_scr_accel = nullptr;
static lv_obj_t *s_scr_gyro = nullptr;
static lv_obj_t *s_calib_label = nullptr;

IMUDemo *IMUDemo::_instance = nullptr;

IMUDemo *IMUDemo::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new IMUDemo();
    }
    return _instance;
}

IMUDemo::IMUDemo():
    App(APP_NAME, nullptr, true, false, false)
{
}

IMUDemo::~IMUDemo()
{
}

static void ui_event_scr_accel(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        lv_scr_load_anim(s_scr_gyro, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    }
}

static void ui_event_scr_gyro(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        lv_scr_load_anim(s_scr_accel, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    }
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

    static lv_obj_t *accel_x_val, *accel_y_val, *accel_z_val;
    static lv_obj_t *accel_x_bar, *accel_y_bar, *accel_z_bar;
    static lv_obj_t *gyro_x_val, *gyro_y_val, *gyro_z_val;
    static lv_obj_t *gyro_x_bar, *gyro_y_bar, *gyro_z_bar;

    s_scr_accel = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_accel, 410, 502);
    lv_obj_set_style_bg_color(s_scr_accel, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_accel, ui_event_scr_accel, LV_EVENT_ALL, nullptr);

    s_scr_gyro = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_gyro, 410, 502);
    lv_obj_set_style_bg_color(s_scr_gyro, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_gyro, ui_event_scr_gyro, LV_EVENT_ALL, nullptr);

    lv_obj_t *title_bg_accel = lv_obj_create(s_scr_accel);
    lv_obj_set_size(title_bg_accel, 410, 60);
    lv_obj_align(title_bg_accel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bg_accel, lv_color_hex(0x1C7F54), 0);
    lv_obj_clear_flag(title_bg_accel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_accel = lv_label_create(title_bg_accel);
    lv_label_set_text(title_accel, "ACCELEROMETER");
    lv_obj_align(title_accel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(title_accel, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_accel, &lv_font_montserrat_24, 0);
    lv_obj_clear_flag(title_accel, LV_OBJ_FLAG_CLICKABLE);

    s_calib_label = lv_label_create(s_scr_accel);
    lv_label_set_text(s_calib_label, "Keep steady for calibration...");
    lv_obj_align(s_calib_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(s_calib_label, lv_color_hex(0xF0B90B), 0);
    lv_obj_set_style_text_font(s_calib_label, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(s_calib_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title_bg_gyro = lv_obj_create(s_scr_gyro);
    lv_obj_set_size(title_bg_gyro, 410, 60);
    lv_obj_align(title_bg_gyro, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bg_gyro, lv_color_hex(0xA62639), 0);
    lv_obj_clear_flag(title_bg_gyro, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_gyro = lv_label_create(title_bg_gyro);
    lv_label_set_text(title_gyro, "GYROSCOPE");
    lv_obj_align(title_gyro, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(title_gyro, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_gyro, &lv_font_montserrat_24, 0);
    lv_obj_clear_flag(title_gyro, LV_OBJ_FLAG_CLICKABLE);

    auto create_full_gauge = [](lv_obj_t *parent, int idx, const char *axis, lv_color_t color) -> std::tuple<lv_obj_t *, lv_obj_t *> {
        int y_base = 80 + idx * 130;
        lv_obj_t *container = lv_obj_create(parent);
        lv_obj_set_size(container, 390, 115);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, y_base);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_radius(container, 16, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *axis_label = lv_label_create(container);
        lv_label_set_text(axis_label, axis);
        lv_obj_set_pos(axis_label, 20, 15);
        lv_obj_set_style_text_color(axis_label, color, 0);
        lv_obj_set_style_text_font(axis_label, &lv_font_montserrat_36, 0);
        lv_obj_clear_flag(axis_label, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *val_label = lv_label_create(container);
        lv_label_set_text(val_label, "0.00");
        lv_obj_align(val_label, LV_ALIGN_TOP_RIGHT, -20, 15);
        lv_obj_set_style_text_color(val_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(val_label, &lv_font_montserrat_24, 0);
        lv_obj_clear_flag(val_label, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *unit_label = lv_label_create(container);
        lv_label_set_text(unit_label, "m/s2");
        lv_obj_align(unit_label, LV_ALIGN_TOP_RIGHT, -20, 45);
        lv_obj_set_style_text_color(unit_label, lv_color_hex(0x8B949E), 0);
        lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_12, 0);
        lv_obj_clear_flag(unit_label, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *bar_bg = lv_obj_create(container);
        lv_obj_set_size(bar_bg, 340, 18);
        lv_obj_align(bar_bg, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_obj_set_style_radius(bar_bg, 9, 0);
        lv_obj_set_style_bg_color(bar_bg, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(bar_bg, 0, 0);
        lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *center_tick = lv_obj_create(bar_bg);
        lv_obj_set_size(center_tick, 4, 18);
        lv_obj_align(center_tick, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(center_tick, lv_color_white(), 0);
        lv_obj_set_style_border_width(center_tick, 0, 0);
        lv_obj_clear_flag(center_tick, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *bar = lv_obj_create(bar_bg);
        lv_obj_set_size(bar, 1, 18);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, color, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

        return {val_label, bar};
    };

    std::tie(accel_x_val, accel_x_bar) = create_full_gauge(s_scr_accel, 0, "X", lv_color_hex(0x58A6FF));
    std::tie(accel_y_val, accel_y_bar) = create_full_gauge(s_scr_accel, 1, "Y", lv_color_hex(0x58A6FF));
    std::tie(accel_z_val, accel_z_bar) = create_full_gauge(s_scr_accel, 2, "Z", lv_color_hex(0x58A6FF));

    std::tie(gyro_x_val, gyro_x_bar) = create_full_gauge(s_scr_gyro, 0, "X", lv_color_hex(0xFF7B72));
    std::tie(gyro_y_val, gyro_y_bar) = create_full_gauge(s_scr_gyro, 1, "Y", lv_color_hex(0xFF7B72));
    std::tie(gyro_z_val, gyro_z_bar) = create_full_gauge(s_scr_gyro, 2, "Z", lv_color_hex(0xFF7B72));

    struct IMU_UI_Data {
        lv_obj_t *x_val, *y_val, *z_val;
        lv_obj_t *x_bar, *y_bar, *z_bar;
        float max_val;
    };

    static IMU_UI_Data accel_ui = {accel_x_val, accel_y_val, accel_z_val, accel_x_bar, accel_y_bar, accel_z_bar, 20.0f};
    static IMU_UI_Data gyro_ui = {gyro_x_val, gyro_y_val, gyro_z_val, gyro_x_bar, gyro_y_bar, gyro_z_bar, 2.0f};

    if (s_imu_dev) {
        s_imu_timer = lv_timer_create([](lv_timer_t *t) {
            if (s_imu_dev == nullptr) return;
            qmi8658_data_t data;
            if (qmi8658_read_sensor_data(s_imu_dev, &data) == ESP_OK) {
                if (!s_imu_calibrated) {
                    s_calib_accel_sum[0] += data.accelX;
                    s_calib_accel_sum[1] += data.accelY;
                    s_calib_accel_sum[2] += data.accelZ;
                    s_calib_gyro_sum[0] += data.gyroX;
                    s_calib_gyro_sum[1] += data.gyroY;
                    s_calib_gyro_sum[2] += data.gyroZ;
                    s_calib_samples++;

                    if (s_calib_samples >= 50) {
                        s_accel_offset[0] = s_calib_accel_sum[0] / 50.0f;
                        s_accel_offset[1] = s_calib_accel_sum[1] / 50.0f;
                        s_accel_offset[2] = s_calib_accel_sum[2] / 50.0f - 9.81f;
                        s_gyro_offset[0] = s_calib_gyro_sum[0] / 50.0f;
                        s_gyro_offset[1] = s_calib_gyro_sum[1] / 50.0f;
                        s_gyro_offset[2] = s_calib_gyro_sum[2] / 50.0f;
                        s_imu_calibrated = true;
                        lv_obj_add_flag(s_calib_label, LV_OBJ_FLAG_HIDDEN);
                        ESP_UTILS_LOGI("IMU calibrated");
                    }
                    return;
                }

                float accelX = data.accelX - s_accel_offset[0];
                float accelY = data.accelY - s_accel_offset[1];
                float accelZ = data.accelZ - s_accel_offset[2];
                float gyroX = data.gyroX - s_gyro_offset[0];
                float gyroY = data.gyroY - s_gyro_offset[1];
                float gyroZ = data.gyroZ - s_gyro_offset[2];

                s_gyro_filtered[0] = s_gyro_filtered[0] + FILTER_ALPHA * (gyroX - s_gyro_filtered[0]);
                s_gyro_filtered[1] = s_gyro_filtered[1] + FILTER_ALPHA * (gyroY - s_gyro_filtered[1]);
                s_gyro_filtered[2] = s_gyro_filtered[2] + FILTER_ALPHA * (gyroZ - s_gyro_filtered[2]);
                gyroX = s_gyro_filtered[0];
                gyroY = s_gyro_filtered[1];
                gyroZ = s_gyro_filtered[2];

                auto update_axis = [](lv_obj_t *val_label, lv_obj_t *bar, float value, float max_val) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.2f", value);
                    lv_label_set_text(val_label, buf);

                    float abs_val = fabsf(value);
                    int bar_width = (int)(170.0f * (abs_val / max_val));
                    if (bar_width > 170) bar_width = 170;
                    if (bar_width < 1) bar_width = 1;
                    lv_obj_set_width(bar, bar_width);

                    if (value < 0) {
                        lv_obj_align(bar, LV_ALIGN_CENTER, -(bar_width / 2), 0);
                    } else if (value > 0) {
                        lv_obj_align(bar, LV_ALIGN_CENTER, (bar_width / 2), 0);
                    } else {
                        lv_obj_set_width(bar, 4);
                        lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
                    }
                };

                update_axis(accel_ui.x_val, accel_ui.x_bar, accelX, accel_ui.max_val);
                update_axis(accel_ui.y_val, accel_ui.y_bar, accelY, accel_ui.max_val);
                update_axis(accel_ui.z_val, accel_ui.z_bar, accelZ, accel_ui.max_val);

                update_axis(gyro_ui.x_val, gyro_ui.x_bar, gyroX, gyro_ui.max_val);
                update_axis(gyro_ui.y_val, gyro_ui.y_bar, gyroY, gyro_ui.max_val);
                update_axis(gyro_ui.z_val, gyro_ui.z_bar, gyroZ, gyro_ui.max_val);
            }
        }, 20, nullptr);
    }

    lv_scr_load(s_scr_accel);

    return true;
}

bool IMUDemo::back(void)
{
    ESP_UTILS_LOGD("Back IMU Demo");

    if (s_imu_timer) {
        lv_timer_delete(s_imu_timer);
        s_imu_timer = nullptr;
    }

    s_imu_calibrated = false;
    s_calib_samples = 0;
    s_calib_accel_sum[0] = s_calib_accel_sum[1] = s_calib_accel_sum[2] = 0;
    s_calib_gyro_sum[0] = s_calib_gyro_sum[1] = s_calib_gyro_sum[2] = 0;
    s_gyro_filtered[0] = s_gyro_filtered[1] = s_gyro_filtered[2] = 0;

    s_scr_accel = nullptr;
    s_scr_gyro = nullptr;
    s_calib_label = nullptr;

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