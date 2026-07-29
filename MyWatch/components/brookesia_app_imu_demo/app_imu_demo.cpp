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
#define PAGE_COUNT 7
#define AXIS_COUNT 3

namespace {

enum PageId { PAGE_ZERO, PAGE_ACCEL, PAGE_GYRO, PAGE_ATTITUDE, PAGE_FIXED, PAGE_ACCEL_CALIB, PAGE_GYRO_CALIB };

struct PageInfo {
    PageId id;
    const char *title;
    uint32_t color;
    const char *nav_hint;
};

static constexpr PageInfo s_page_info[PAGE_COUNT] = {
    {PAGE_ZERO, "ZERO POINT", 0xBF8700, "Swipe Left->GyroCalib  Right->Accel"},
    {PAGE_ACCEL, "ACCELEROMETER", 0x1C7F54, "Swipe Left->Zero  Right->Gyro"},
    {PAGE_GYRO, "GYROSCOPE", 0xA62639, "Swipe Left->Fixed  Right->Accel"},
    {PAGE_ATTITUDE, "ATTITUDE", 0x8B5CF6, "Swipe Left->Fixed  Right->Gyro"},
    {PAGE_FIXED, "FIXED ACCEL", 0x0FB3B1, "Swipe Left->AccelCalib  Right->Attitude"},
    {PAGE_ACCEL_CALIB, "ACCEL CALIB", 0x1C7F54, "Swipe Left->Fixed  Right->GyroCalib"},
    {PAGE_GYRO_CALIB, "GYRO CALIB", 0xA62639, "Swipe Left->AccelCalib  Right->Zero"},
};

struct AxisUI {
    lv_obj_t *vals[AXIS_COUNT];
    lv_obj_t *bars[AXIS_COUNT];
};

}

namespace esp_brookesia::apps {

static qmi8658_dev_t *s_imu_dev = nullptr;
static lv_timer_t *s_imu_timer = nullptr;
static lv_obj_t *s_pages[PAGE_COUNT] = {nullptr};
static lv_obj_t *s_status_label = nullptr;
static float s_accel_offset[3] = {0};
static float s_gyro_offset[3] = {0};
static float s_angle[3] = {0};
static AxisUI s_page_ui[PAGE_COUNT] = {};

IMUDemo *IMUDemo::_instance = nullptr;

IMUDemo *IMUDemo::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new IMUDemo();
    }
    return _instance;
}

IMUDemo::IMUDemo(): App(APP_NAME, nullptr, true, false, false) {}
IMUDemo::~IMUDemo() {}

static void switch_screen(lv_obj_t *target)
{
    lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

static int find_page_index(lv_obj_t *page)
{
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (s_pages[i] == page) return i;
    }
    return -1;
}

static void ui_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) return;

    lv_obj_t *page = (lv_obj_t *)lv_event_get_target(e);
    int idx = find_page_index(page);
    if (idx < 0) return;

    int dir = lv_indev_get_gesture_dir(lv_indev_active());
    int next_idx = (dir == LV_DIR_LEFT) ? (idx + 1) % PAGE_COUNT : (idx - 1 + PAGE_COUNT) % PAGE_COUNT;

    lv_indev_wait_release(lv_indev_active());
    switch_screen(s_pages[next_idx]);
}

static void update_axis(lv_obj_t *val_label, lv_obj_t *bar, float value, float max_val, const char *unit)
{
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
}

static void create_base_page(int page_idx)
{
    lv_obj_t *page = lv_obj_create(NULL);
    lv_obj_set_size(page, 410, 502);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x0D1117), 0);
    lv_obj_add_event_cb(page, ui_event_handler, LV_EVENT_GESTURE, nullptr);
    s_pages[page_idx] = page;

    lv_obj_t *title_bg = lv_obj_create(page);
    lv_obj_set_size(title_bg, 410, 70);
    lv_obj_align(title_bg, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bg, lv_color_hex(s_page_info[page_idx].color), 0);
    lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(title_bg);
    lv_label_set_text(title, s_page_info[page_idx].title);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *hint = lv_label_create(page);
    lv_label_set_text(hint, s_page_info[page_idx].nav_hint);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 5);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
}

static void create_gauge(int page_idx, int axis_idx, const char *axis_name, uint32_t color, const char *unit)
{
    int y_base = 90 + axis_idx * 130;
    lv_obj_t *container = lv_obj_create(s_pages[page_idx]);
    lv_obj_set_size(container, 390, 115);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, y_base);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_radius(container, 16, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *axis_label = lv_label_create(container);
    lv_label_set_text(axis_label, axis_name);
    lv_obj_set_pos(axis_label, 25, 15);
    lv_obj_set_style_text_color(axis_label, lv_color_hex(color), 0);
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
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    s_page_ui[page_idx].vals[axis_idx] = val_label;
    s_page_ui[page_idx].bars[axis_idx] = bar;
}

static void create_zero_page(void)
{
    lv_obj_t *zero_box = lv_obj_create(s_pages[PAGE_ZERO]);
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

    auto create_btn = [](lv_obj_t *parent, int y, uint32_t color, const char *text) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 320, 65);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 25, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        return btn;
    };

    lv_obj_t *accel_zero_btn = create_btn(zero_box, 95, 0x238636, "SET ACCEL ZERO");
    lv_obj_t *gyro_zero_btn = create_btn(zero_box, 175, 0x992220, "SET GYRO ZERO");
    lv_obj_t *reset_btn = create_btn(zero_box, 255, 0x30363D, "RESET ALL");

    s_status_label = lv_label_create(s_pages[PAGE_ZERO]);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, 35);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x2EA043), 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_18, 0);
    lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(accel_zero_btn, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED && s_imu_dev != nullptr) {
            qmi8658_data_t data;
            if (qmi8658_read_sensor_data(s_imu_dev, &data) == ESP_OK) {
                s_accel_offset[0] = data.accelX;
                s_accel_offset[1] = data.accelY;
                s_accel_offset[2] = data.accelZ;
                lv_label_set_text(s_status_label, "Accel zeroed!");
                ESP_UTILS_LOGI("Accel zeroed: X=%.4f Y=%.4f Z=%.4f m/s2",
                    s_accel_offset[0], s_accel_offset[1], s_accel_offset[2]);
            }
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(gyro_zero_btn, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED && s_imu_dev != nullptr) {
            qmi8658_data_t data;
            if (qmi8658_read_sensor_data(s_imu_dev, &data) == ESP_OK) {
                s_gyro_offset[0] = data.gyroX;
                s_gyro_offset[1] = data.gyroY;
                s_gyro_offset[2] = data.gyroZ;
                lv_label_set_text(s_status_label, "Gyro zeroed!");
                ESP_UTILS_LOGI("Gyro zeroed: X=%.4f Y=%.4f Z=%.4f dps",
                    s_gyro_offset[0], s_gyro_offset[1], s_gyro_offset[2]);
            }
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(reset_btn, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            s_accel_offset[0] = s_accel_offset[1] = s_accel_offset[2] = 0;
            s_gyro_offset[0] = s_gyro_offset[1] = s_gyro_offset[2] = 0;
            s_angle[0] = s_angle[1] = s_angle[2] = 0;
            lv_label_set_text(s_status_label, "Reset!");
            ESP_UTILS_LOGI("Calibration reset");
        }
    }, LV_EVENT_CLICKED, nullptr);
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
                qmi8658_set_gyro_range(s_imu_dev, QMI8658_GYRO_RANGE_256DPS);
                qmi8658_set_gyro_odr(s_imu_dev, QMI8658_GYRO_ODR_500HZ);
                qmi8658_write_register(s_imu_dev, QMI8658_CTRL5, 0x03);
                ESP_UTILS_LOGI("IMU initialized");
            } else {
                delete s_imu_dev;
                s_imu_dev = nullptr;
                ESP_LOGE("IMU_DEMO", "QMI8658 init failed");
            }
        }
    }

    for (int i = 0; i < PAGE_COUNT; i++) {
        create_base_page(i);
    }

    create_gauge(PAGE_ACCEL, 0, "X", 0x58A6FF, "m/s2");
    create_gauge(PAGE_ACCEL, 1, "Y", 0x58A6FF, "m/s2");
    create_gauge(PAGE_ACCEL, 2, "Z", 0x58A6FF, "m/s2");

    create_gauge(PAGE_GYRO, 0, "X", 0xFF7B72, "dps");
    create_gauge(PAGE_GYRO, 1, "Y", 0xFF7B72, "dps");
    create_gauge(PAGE_GYRO, 2, "Z", 0xFF7B72, "dps");

    create_gauge(PAGE_ATTITUDE, 0, "Pitch", 0xA78BFA, "deg");
    create_gauge(PAGE_ATTITUDE, 1, "Roll", 0xC084FC, "deg");
    create_gauge(PAGE_ATTITUDE, 2, "Yaw", 0xE879F9, "deg");

    create_gauge(PAGE_FIXED, 0, "X", 0x0FB3B1, "m/s2");
    create_gauge(PAGE_FIXED, 1, "Y", 0x0FB3B1, "m/s2");
    create_gauge(PAGE_FIXED, 2, "Z", 0x0FB3B1, "m/s2");

    create_gauge(PAGE_ACCEL_CALIB, 0, "X", 0x58A6FF, "m/s2");
    create_gauge(PAGE_ACCEL_CALIB, 1, "Y", 0x58A6FF, "m/s2");
    create_gauge(PAGE_ACCEL_CALIB, 2, "Z", 0x58A6FF, "m/s2");

    create_gauge(PAGE_GYRO_CALIB, 0, "X", 0xFF7B72, "dps");
    create_gauge(PAGE_GYRO_CALIB, 1, "Y", 0xFF7B72, "dps");
    create_gauge(PAGE_GYRO_CALIB, 2, "Z", 0xFF7B72, "dps");

    create_zero_page();

    if (s_imu_dev) {
        s_imu_timer = lv_timer_create([](lv_timer_t *t) {
            if (s_imu_dev == nullptr) return;
            qmi8658_data_t data;
            if (qmi8658_read_sensor_data(s_imu_dev, &data) != ESP_OK) return;

            float accelX = data.accelX - s_accel_offset[0];
            float accelY = data.accelY - s_accel_offset[1];
            float accelZ = data.accelZ - s_accel_offset[2];

            float gyroX = data.gyroX;
            float gyroY = data.gyroY;
            float gyroZ = data.gyroZ;

            float gyroX_filt = data.gyroX - s_gyro_offset[0];
            float gyroY_filt = data.gyroY - s_gyro_offset[1];
            float gyroZ_filt = data.gyroZ - s_gyro_offset[2];

            constexpr float GYRO_THRESHOLD = 7.0f;
            if (fabsf(gyroX_filt) < GYRO_THRESHOLD) gyroX_filt = 0;
            if (fabsf(gyroY_filt) < GYRO_THRESHOLD) gyroY_filt = 0;
            if (fabsf(gyroZ_filt) < GYRO_THRESHOLD) gyroZ_filt = 0;

            float dps_to_rad = M_PI / 180.0f;
            s_angle[0] += gyroX_filt * 0.02f * dps_to_rad;
            s_angle[1] += gyroY_filt * 0.02f * dps_to_rad;
            s_angle[2] += gyroZ_filt * 0.02f * dps_to_rad;

            float pitch = s_angle[0];
            float roll = s_angle[1];
            float yaw = s_angle[2];

            float ax_fixed = accelX;
            float ay_fixed = accelY;
            float az_fixed = accelZ;
            float sinp = sinf(pitch), cosp = cosf(pitch);
            float sinr = sinf(roll), cosr = cosf(roll);
            float siny = sinf(yaw), cosy = cosf(yaw);

            float ax_r1 = cosy * ax_fixed + siny * sinr * ay_fixed - siny * cosr * az_fixed;
            float ay_r1 = cosr * ay_fixed + sinr * az_fixed;
            float az_r1 = siny * ax_fixed - cosy * sinr * ay_fixed - cosy * cosr * az_fixed;
            float ax_fixed_final = cosr * ax_r1 - sinr * az_r1;
            float ay_fixed_final = sinp * sinr * ax_r1 + cosp * ay_r1 - sinp * cosr * az_r1;
            float az_fixed_final = cosp * sinr * ax_r1 + sinp * ay_r1 + cosp * cosr * az_r1;

            float rad_to_deg = 180.0f / M_PI;

            AxisUI &accel_ui = s_page_ui[PAGE_ACCEL];
            update_axis(accel_ui.vals[0], accel_ui.bars[0], accelX, 20.0f, "m/s2");
            update_axis(accel_ui.vals[1], accel_ui.bars[1], accelY, 20.0f, "m/s2");
            update_axis(accel_ui.vals[2], accel_ui.bars[2], accelZ, 20.0f, "m/s2");

            AxisUI &gyro_ui = s_page_ui[PAGE_GYRO];
            update_axis(gyro_ui.vals[0], gyro_ui.bars[0], gyroX, 250.0f, "dps");
            update_axis(gyro_ui.vals[1], gyro_ui.bars[1], gyroY, 250.0f, "dps");
            update_axis(gyro_ui.vals[2], gyro_ui.bars[2], gyroZ, 250.0f, "dps");

            AxisUI &att_ui = s_page_ui[PAGE_ATTITUDE];
            update_axis(att_ui.vals[0], att_ui.bars[0], pitch * rad_to_deg, 180.0f, "deg");
            update_axis(att_ui.vals[1], att_ui.bars[1], roll * rad_to_deg, 180.0f, "deg");
            update_axis(att_ui.vals[2], att_ui.bars[2], yaw * rad_to_deg, 180.0f, "deg");

            AxisUI &fixed_ui = s_page_ui[PAGE_FIXED];
            update_axis(fixed_ui.vals[0], fixed_ui.bars[0], ax_fixed_final, 20.0f, "m/s2");
            update_axis(fixed_ui.vals[1], fixed_ui.bars[1], ay_fixed_final, 20.0f, "m/s2");
            update_axis(fixed_ui.vals[2], fixed_ui.bars[2], az_fixed_final, 20.0f, "m/s2");

            AxisUI &accel_calib_ui = s_page_ui[PAGE_ACCEL_CALIB];
            update_axis(accel_calib_ui.vals[0], accel_calib_ui.bars[0], s_accel_offset[0], 20.0f, "m/s2");
            update_axis(accel_calib_ui.vals[1], accel_calib_ui.bars[1], s_accel_offset[1], 20.0f, "m/s2");
            update_axis(accel_calib_ui.vals[2], accel_calib_ui.bars[2], s_accel_offset[2], 20.0f, "m/s2");

            AxisUI &gyro_calib_ui = s_page_ui[PAGE_GYRO_CALIB];
            update_axis(gyro_calib_ui.vals[0], gyro_calib_ui.bars[0], s_gyro_offset[0], 250.0f, "dps");
            update_axis(gyro_calib_ui.vals[1], gyro_calib_ui.bars[1], s_gyro_offset[1], 250.0f, "dps");
            update_axis(gyro_calib_ui.vals[2], gyro_calib_ui.bars[2], s_gyro_offset[2], 250.0f, "dps");
        }, 20, nullptr);
    }

    lv_scr_load(s_pages[PAGE_ACCEL]);
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
    s_angle[0] = s_angle[1] = s_angle[2] = 0;

    for (int i = 0; i < PAGE_COUNT; i++) {
        s_pages[i] = nullptr;
    }

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
