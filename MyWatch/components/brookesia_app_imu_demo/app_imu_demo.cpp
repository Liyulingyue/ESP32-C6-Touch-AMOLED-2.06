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
static float s_accel_offset[3] = {0};
static float s_gyro_offset[3] = {0};
static float s_gyro_filtered[3] = {0};
static constexpr float FILTER_ALPHA = 0.001f;

static float s_angle[3] = {0};

static lv_obj_t *s_scr_accel = nullptr;
static lv_obj_t *s_scr_gyro = nullptr;
static lv_obj_t *s_scr_accel_calib = nullptr;
static lv_obj_t *s_scr_gyro_calib = nullptr;
static lv_obj_t *s_scr_zero = nullptr;
static lv_obj_t *s_scr_fixed_accel = nullptr;

static lv_obj_t *s_status_label = nullptr;

static lv_obj_t *s_accel_calib_x_val = nullptr;
static lv_obj_t *s_accel_calib_y_val = nullptr;
static lv_obj_t *s_accel_calib_z_val = nullptr;
static lv_obj_t *s_accel_calib_x_bar = nullptr;
static lv_obj_t *s_accel_calib_y_bar = nullptr;
static lv_obj_t *s_accel_calib_z_bar = nullptr;

static lv_obj_t *s_gyro_calib_x_val = nullptr;
static lv_obj_t *s_gyro_calib_y_val = nullptr;
static lv_obj_t *s_gyro_calib_z_val = nullptr;
static lv_obj_t *s_gyro_calib_x_bar = nullptr;
static lv_obj_t *s_gyro_calib_y_bar = nullptr;
static lv_obj_t *s_gyro_calib_z_bar = nullptr;

static lv_obj_t *s_fixed_accel_x_val = nullptr;
static lv_obj_t *s_fixed_accel_y_val = nullptr;
static lv_obj_t *s_fixed_accel_z_val = nullptr;
static lv_obj_t *s_fixed_accel_x_bar = nullptr;
static lv_obj_t *s_fixed_accel_y_bar = nullptr;
static lv_obj_t *s_fixed_accel_z_bar = nullptr;

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

static void switch_screen(lv_obj_t *target)
{
    lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

static void ui_event_scr_accel(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_zero);
    } else if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_gyro);
    }
}

static void ui_event_scr_gyro(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_accel);
    } else if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_fixed_accel);
    }
}

static void ui_event_scr_fixed_accel(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_gyro);
    } else if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_accel_calib);
    }
}

static void ui_event_scr_accel_calib(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_fixed_accel);
    } else if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_gyro_calib);
    }
}

static void ui_event_scr_gyro_calib(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_accel_calib);
    } else if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_zero);
    }
}

static void ui_event_scr_zero(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_gyro_calib);
    } else if (code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_active());
        switch_screen(s_scr_accel);
    }
}

static void update_calib_bar(lv_obj_t *val_label, lv_obj_t *bar, float value, float max_val)
{
    char buf[32];
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
    lv_obj_add_event_cb(s_scr_accel, ui_event_scr_accel, LV_EVENT_GESTURE, nullptr);

    s_scr_gyro = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_gyro, 410, 502);
    lv_obj_set_style_bg_color(s_scr_gyro, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_gyro, ui_event_scr_gyro, LV_EVENT_GESTURE, nullptr);

    s_scr_zero = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_zero, 410, 502);
    lv_obj_set_style_bg_color(s_scr_zero, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_zero, ui_event_scr_zero, LV_EVENT_GESTURE, nullptr);

    s_scr_accel_calib = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_accel_calib, 410, 502);
    lv_obj_set_style_bg_color(s_scr_accel_calib, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_accel_calib, ui_event_scr_accel_calib, LV_EVENT_GESTURE, nullptr);

    s_scr_gyro_calib = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_gyro_calib, 410, 502);
    lv_obj_set_style_bg_color(s_scr_gyro_calib, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_gyro_calib, ui_event_scr_gyro_calib, LV_EVENT_GESTURE, nullptr);

    s_scr_fixed_accel = lv_obj_create(NULL);
    lv_obj_set_size(s_scr_fixed_accel, 410, 502);
    lv_obj_set_style_bg_color(s_scr_fixed_accel, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(s_scr_fixed_accel, ui_event_scr_fixed_accel, LV_EVENT_GESTURE, nullptr);

    auto create_title = [](lv_obj_t *parent, const char *text, lv_color_t bg_color) {
        lv_obj_t *title_bg = lv_obj_create(parent);
        lv_obj_set_size(title_bg, 410, 70);
        lv_obj_align(title_bg, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(title_bg, bg_color, 0);
        lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(title_bg);
        lv_label_set_text(title, text);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
        lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
        return title_bg;
    };

    auto create_nav_hint = [](lv_obj_t *parent, const char *text) {
        lv_obj_t *hint = lv_label_create(parent);
        lv_label_set_text(hint, text);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 5);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x8B949E), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
    };

    create_title(s_scr_accel, "ACCELEROMETER", lv_color_hex(0x1C7F54));
    create_nav_hint(s_scr_accel, "Swipe Left->Zero  Right->Gyro");

    create_title(s_scr_gyro, "GYROSCOPE", lv_color_hex(0xA62639));
    create_nav_hint(s_scr_gyro, "Swipe Left->Accel  Right->Fixed");

    create_title(s_scr_fixed_accel, "FIXED ACCEL", lv_color_hex(0x0FB3B1));
    create_nav_hint(s_scr_fixed_accel, "Swipe Left->Gyro  Right->AccelCalib");

    create_title(s_scr_accel_calib, "ACCEL CALIB", lv_color_hex(0x1C7F54));
    create_nav_hint(s_scr_accel_calib, "Swipe Left->Fixed  Right->GyroCalib");

    create_title(s_scr_gyro_calib, "GYRO CALIB", lv_color_hex(0xA62639));
    create_nav_hint(s_scr_gyro_calib, "Swipe Left->AccelCalib  Right->Zero");

    create_title(s_scr_zero, "ZERO POINT", lv_color_hex(0xBF8700));
    create_nav_hint(s_scr_zero, "Swipe Left->GyroCalib  Right->Accel");

    auto create_gauge = [](lv_obj_t *parent, int idx, const char *axis, lv_color_t color, const char *unit, lv_obj_t **out_val, lv_obj_t **out_bar) {
        int y_base = 90 + idx * 130;
        lv_obj_t *container = lv_obj_create(parent);
        lv_obj_set_size(container, 390, 115);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, y_base);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_radius(container, 16, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *axis_label = lv_label_create(container);
        lv_label_set_text(axis_label, axis);
        lv_obj_set_pos(axis_label, 25, 15);
        lv_obj_set_style_text_color(axis_label, color, 0);
        lv_obj_set_style_text_font(axis_label, &lv_font_montserrat_36, 0);
        lv_obj_clear_flag(axis_label, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *val_label = lv_label_create(container);
        char buf[32];
        snprintf(buf, sizeof(buf), "0.00 %s", unit);
        lv_label_set_text(val_label, buf);
        lv_obj_align(val_label, LV_ALIGN_TOP_RIGHT, -25, 15);
        lv_obj_set_style_text_color(val_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(val_label, &lv_font_montserrat_24, 0);
        lv_obj_clear_flag(val_label, LV_OBJ_FLAG_CLICKABLE);
        *out_val = val_label;

        lv_obj_t *bar_bg = lv_obj_create(container);
        lv_obj_set_size(bar_bg, 340, 20);
        lv_obj_align(bar_bg, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_obj_set_style_radius(bar_bg, 10, 0);
        lv_obj_set_style_bg_color(bar_bg, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(bar_bg, 0, 0);
        lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *center_tick = lv_obj_create(bar_bg);
        lv_obj_set_size(center_tick, 4, 20);
        lv_obj_align(center_tick, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(center_tick, lv_color_white(), 0);
        lv_obj_set_style_border_width(center_tick, 0, 0);
        lv_obj_clear_flag(center_tick, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *bar = lv_obj_create(bar_bg);
        lv_obj_set_size(bar, 1, 20);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, color, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        *out_bar = bar;
    };

    create_gauge(s_scr_accel, 0, "X", lv_color_hex(0x58A6FF), "m/s2", &accel_x_val, &accel_x_bar);
    create_gauge(s_scr_accel, 1, "Y", lv_color_hex(0x58A6FF), "m/s2", &accel_y_val, &accel_y_bar);
    create_gauge(s_scr_accel, 2, "Z", lv_color_hex(0x58A6FF), "m/s2", &accel_z_val, &accel_z_bar);

    create_gauge(s_scr_gyro, 0, "X", lv_color_hex(0xFF7B72), "rad/s", &gyro_x_val, &gyro_x_bar);
    create_gauge(s_scr_gyro, 1, "Y", lv_color_hex(0xFF7B72), "rad/s", &gyro_y_val, &gyro_y_bar);
    create_gauge(s_scr_gyro, 2, "Z", lv_color_hex(0xFF7B72), "rad/s", &gyro_z_val, &gyro_z_bar);

    create_gauge(s_scr_accel_calib, 0, "X", lv_color_hex(0x58A6FF), "m/s2", &s_accel_calib_x_val, &s_accel_calib_x_bar);
    create_gauge(s_scr_accel_calib, 1, "Y", lv_color_hex(0x58A6FF), "m/s2", &s_accel_calib_y_val, &s_accel_calib_y_bar);
    create_gauge(s_scr_accel_calib, 2, "Z", lv_color_hex(0x58A6FF), "m/s2", &s_accel_calib_z_val, &s_accel_calib_z_bar);

    create_gauge(s_scr_gyro_calib, 0, "X", lv_color_hex(0xFF7B72), "rad/s", &s_gyro_calib_x_val, &s_gyro_calib_x_bar);
    create_gauge(s_scr_gyro_calib, 1, "Y", lv_color_hex(0xFF7B72), "rad/s", &s_gyro_calib_y_val, &s_gyro_calib_y_bar);
    create_gauge(s_scr_gyro_calib, 2, "Z", lv_color_hex(0xFF7B72), "rad/s", &s_gyro_calib_z_val, &s_gyro_calib_z_bar);

    create_gauge(s_scr_fixed_accel, 0, "X", lv_color_hex(0x0FB3B1), "m/s2", &s_fixed_accel_x_val, &s_fixed_accel_x_bar);
    create_gauge(s_scr_fixed_accel, 1, "Y", lv_color_hex(0x0FB3B1), "m/s2", &s_fixed_accel_y_val, &s_fixed_accel_y_bar);
    create_gauge(s_scr_fixed_accel, 2, "Z", lv_color_hex(0x0FB3B1), "m/s2", &s_fixed_accel_z_val, &s_fixed_accel_z_bar);

    lv_obj_t *zero_box = lv_obj_create(s_scr_zero);
    lv_obj_set_size(zero_box, 370, 350);
    lv_obj_align(zero_box, LV_ALIGN_TOP_LEFT, 20, 85);
    lv_obj_set_style_bg_color(zero_box, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_radius(zero_box, 20, 0);
    lv_obj_set_style_border_width(zero_box, 0, 0);
    lv_obj_clear_flag(zero_box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *zero_title = lv_label_create(zero_box);
    lv_label_set_text(zero_title, "Set Zero Point");
    lv_obj_align(zero_title, LV_ALIGN_TOP_LEFT, 25, 20);
    lv_obj_set_style_text_color(zero_title, lv_color_hex(0xF0B90B), 0);
    lv_obj_set_style_text_font(zero_title, &lv_font_montserrat_22, 0);
    lv_obj_clear_flag(zero_title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *zero_hint = lv_label_create(zero_box);
    lv_label_set_text(zero_hint, "Place device at desired position");
    lv_obj_align(zero_hint, LV_ALIGN_TOP_LEFT, 25, 55);
    lv_obj_set_style_text_color(zero_hint, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(zero_hint, &lv_font_montserrat_16, 0);
    lv_obj_clear_flag(zero_hint, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *accel_zero_btn = lv_btn_create(zero_box);
    lv_obj_set_size(accel_zero_btn, 320, 65);
    lv_obj_align(accel_zero_btn, LV_ALIGN_TOP_LEFT, 25, 95);
    lv_obj_set_style_bg_color(accel_zero_btn, lv_color_hex(0x238636), 0);
    lv_obj_set_style_radius(accel_zero_btn, 16, 0);
    lv_obj_t *accel_zero_label = lv_label_create(accel_zero_btn);
    lv_label_set_text(accel_zero_label, "SET ACCEL ZERO");
    lv_obj_center(accel_zero_label);
    lv_obj_set_style_text_color(accel_zero_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(accel_zero_label, &lv_font_montserrat_20, 0);

    lv_obj_t *gyro_zero_btn = lv_btn_create(zero_box);
    lv_obj_set_size(gyro_zero_btn, 320, 65);
    lv_obj_align(gyro_zero_btn, LV_ALIGN_TOP_LEFT, 25, 175);
    lv_obj_set_style_bg_color(gyro_zero_btn, lv_color_hex(0x992220), 0);
    lv_obj_set_style_radius(gyro_zero_btn, 16, 0);
    lv_obj_t *gyro_zero_label = lv_label_create(gyro_zero_btn);
    lv_label_set_text(gyro_zero_label, "SET GYRO ZERO");
    lv_obj_center(gyro_zero_label);
    lv_obj_set_style_text_color(gyro_zero_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(gyro_zero_label, &lv_font_montserrat_20, 0);

    lv_obj_t *reset_btn = lv_btn_create(zero_box);
    lv_obj_set_size(reset_btn, 320, 55);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_LEFT, 25, 255);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_radius(reset_btn, 16, 0);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "RESET ALL");
    lv_obj_center(reset_label);
    lv_obj_set_style_text_color(reset_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_20, 0);

    s_status_label = lv_label_create(s_scr_zero);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, 35);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x2EA043), 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_18, 0);
    lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_CLICKABLE);

lv_obj_add_event_cb(accel_zero_btn, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED && s_imu_dev != nullptr) {
            qmi8658_data_t data;
            if (qmi8658_read_sensor_data(s_imu_dev, &data) == ESP_OK) {
                s_accel_offset[0] = data.accelX;
                s_accel_offset[1] = data.accelY;
                s_accel_offset[2] = data.accelZ;
                update_calib_bar(s_accel_calib_x_val, s_accel_calib_x_bar, s_accel_offset[0], 20.0f);
                update_calib_bar(s_accel_calib_y_val, s_accel_calib_y_bar, s_accel_offset[1], 20.0f);
                update_calib_bar(s_accel_calib_z_val, s_accel_calib_z_bar, s_accel_offset[2], 20.0f);
                lv_label_set_text(s_status_label, "Accel zeroed!");
                ESP_UTILS_LOGI("Accel zeroed: X=%.2f Y=%.2f Z=%.2f", s_accel_offset[0], s_accel_offset[1], s_accel_offset[2]);
            }
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(gyro_zero_btn, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            s_gyro_offset[0] = s_gyro_filtered[0];
            s_gyro_offset[1] = s_gyro_filtered[1];
            s_gyro_offset[2] = s_gyro_filtered[2];
            update_calib_bar(s_gyro_calib_x_val, s_gyro_calib_x_bar, s_gyro_offset[0], 2.0f);
            update_calib_bar(s_gyro_calib_y_val, s_gyro_calib_y_bar, s_gyro_offset[1], 2.0f);
            update_calib_bar(s_gyro_calib_z_val, s_gyro_calib_z_bar, s_gyro_offset[2], 2.0f);
            lv_label_set_text(s_status_label, "Gyro zeroed!");
            ESP_UTILS_LOGI("Gyro zeroed: X=%.4f Y=%.4f Z=%.4f", s_gyro_offset[0], s_gyro_offset[1], s_gyro_offset[2]);
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(reset_btn, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            s_accel_offset[0] = s_accel_offset[1] = s_accel_offset[2] = 0;
            s_gyro_offset[0] = s_gyro_offset[1] = s_gyro_offset[2] = 0;
            update_calib_bar(s_accel_calib_x_val, s_accel_calib_x_bar, 0, 20.0f);
            update_calib_bar(s_accel_calib_y_val, s_accel_calib_y_bar, 0, 20.0f);
            update_calib_bar(s_accel_calib_z_val, s_accel_calib_z_bar, 0, 20.0f);
            update_calib_bar(s_gyro_calib_x_val, s_gyro_calib_x_bar, 0, 2.0f);
            update_calib_bar(s_gyro_calib_y_val, s_gyro_calib_y_bar, 0, 2.0f);
            update_calib_bar(s_gyro_calib_z_val, s_gyro_calib_z_bar, 0, 2.0f);
            lv_label_set_text(s_status_label, "Reset!");
            ESP_UTILS_LOGI("Calibration reset");
        }
    }, LV_EVENT_CLICKED, nullptr);

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

                s_angle[0] += gyroX * 0.02f;
                s_angle[1] += gyroY * 0.02f;
                s_angle[2] += gyroZ * 0.02f;

                float pitch = s_angle[0];
                float roll = s_angle[1];
                float yaw = s_angle[2];

                float ax_fixed = accelX;
                float ay_fixed = accelY;
                float az_fixed = accelZ;
                float sinp = sinf(pitch);
                float cosp = cosf(pitch);
                float sinr = sinf(roll);
                float cosr = cosf(roll);
                float siny = sinf(yaw);
                float cosy = cosf(yaw);

                float ax_r1 = cosy * ax_fixed + siny * sinr * ay_fixed - siny * cosr * az_fixed;
                float ay_r1 = cosr * ay_fixed + sinr * az_fixed;
                float az_r1 = siny * ax_fixed - cosy * sinr * ay_fixed - cosy * cosr * az_fixed;
                float ax_fixed_final = cosr * ax_r1 - sinr * az_r1;
                float ay_fixed_final = sinp * sinr * ax_r1 + cosp * ay_r1 - sinp * cosr * az_r1;
                float az_fixed_final = cosp * sinr * ax_r1 + sinp * ay_r1 + cosp * cosr * az_r1;

                auto update_axis = [](lv_obj_t *val_label, lv_obj_t *bar, float value, float max_val, const char *unit) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f %s", value, unit);
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

                update_axis(accel_ui.x_val, accel_ui.x_bar, accelX, accel_ui.max_val, "m/s2");
                update_axis(accel_ui.y_val, accel_ui.y_bar, accelY, accel_ui.max_val, "m/s2");
                update_axis(accel_ui.z_val, accel_ui.z_bar, accelZ, accel_ui.max_val, "m/s2");

                update_axis(gyro_ui.x_val, gyro_ui.x_bar, gyroX, gyro_ui.max_val, "rad/s");
                update_axis(gyro_ui.y_val, gyro_ui.y_bar, gyroY, gyro_ui.max_val, "rad/s");
                update_axis(gyro_ui.z_val, gyro_ui.z_bar, gyroZ, gyro_ui.max_val, "rad/s");

                update_axis(s_fixed_accel_x_val, s_fixed_accel_x_bar, ax_fixed_final, 20.0f, "m/s2");
                update_axis(s_fixed_accel_y_val, s_fixed_accel_y_bar, ay_fixed_final, 20.0f, "m/s2");
                update_axis(s_fixed_accel_z_val, s_fixed_accel_z_bar, az_fixed_final, 20.0f, "m/s2");
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

    s_accel_offset[0] = s_accel_offset[1] = s_accel_offset[2] = 0;
    s_gyro_offset[0] = s_gyro_offset[1] = s_gyro_offset[2] = 0;
    s_gyro_filtered[0] = s_gyro_filtered[1] = s_gyro_filtered[2] = 0;
    s_angle[0] = s_angle[1] = s_angle[2] = 0;

    s_scr_accel = nullptr;
    s_scr_gyro = nullptr;
    s_scr_accel_calib = nullptr;
    s_scr_gyro_calib = nullptr;
    s_scr_zero = nullptr;
    s_scr_fixed_accel = nullptr;

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