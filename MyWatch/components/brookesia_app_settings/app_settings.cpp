#include "app_settings.hpp"
#include "page_navigator.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "esp_lib_utils.h"
#include "port_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "SETTINGS"

using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;
using namespace esp_brookesia::apps;

#define APP_NAME "Settings"
#define PAGE_COUNT 5

namespace {

enum PageId { PAGE_HOME, PAGE_DISPLAY, PAGE_TIME, PAGE_WIFI, PAGE_ABOUT };

static lv_obj_t *s_wifi_ssid_input = nullptr;
static lv_obj_t *s_wifi_password_input = nullptr;
static lv_obj_t *s_wifi_status_label = nullptr;
static lv_obj_t *s_wifi_ssid_label = nullptr;

struct PageInfo {
    const char *title;
    uint32_t color;
};

static constexpr PageInfo s_page_info[PAGE_COUNT] = {
    {"SETTINGS", 0x4A90E2},
    {"DISPLAY", 0xF5A623},
    {"TIME", 0x50E3C4},
    {"WI-FI", 0x7B61FF},
    {"ABOUT", 0xA0A0A0},
};

}

namespace {

static PageNavigator *s_navigator = nullptr;
static lv_obj_t *s_pages[PAGE_COUNT] = {nullptr};
static lv_obj_t *s_keyboard = nullptr;

static void gesture_handler(lv_event_t *e)
{
    if (!s_navigator) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) return;

    int dir = lv_indev_get_gesture_dir(lv_indev_active());
    uint32_t allowed_dirs = s_navigator->get_swipe_directions();
    lv_indev_wait_release(lv_indev_active());
    
    if ((dir == LV_DIR_LEFT) && (allowed_dirs & LV_DIR_LEFT)) {
        s_navigator->navigate_next();
    } else if ((dir == LV_DIR_RIGHT) && (allowed_dirs & LV_DIR_RIGHT)) {
        s_navigator->navigate_prev();
    } else if ((dir == LV_DIR_TOP) && (allowed_dirs & LV_DIR_TOP)) {
        s_navigator->navigate_next();
    } else if ((dir == LV_DIR_BOTTOM) && (allowed_dirs & LV_DIR_BOTTOM)) {
        s_navigator->navigate_prev();
    }
}

}

namespace esp_brookesia::apps {

struct SettingsApp::Impl {
    lv_obj_t *pages[PAGE_COUNT] = {nullptr};
    int brightness = 50;
    int screen_timeout = 30;
    bool wifi_enabled = false;
    int volume = 80;
};

SettingsApp::SettingsApp(): App(APP_NAME, nullptr, true, false, false), impl(new Impl()) {}
SettingsApp::~SettingsApp() { delete impl; }

static void create_title(lv_obj_t *page, const char *text, uint32_t color)
{
    lv_obj_t *title_bg = lv_obj_create(page);
    lv_obj_set_size(title_bg, 410, 90);
    lv_obj_align(title_bg, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bg, lv_color_hex(color), 0);
    lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *status_bar = lv_obj_create(title_bg);
    lv_obj_set_size(status_bar, 410, 24);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(title_bg);
    lv_label_set_text(title, text);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *hint = lv_label_create(page);
    lv_label_set_text(hint, "Swipe left/right to switch pages");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 8);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
}

static void create_slider_row(lv_obj_t *parent, int y, const char *title, int value, int min_val, int max_val)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 390, 70);
    lv_obj_align(container, LV_ALIGN_TOP_LEFT, 10, y);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(container, 16, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_label_create(container);
    lv_label_set_text(icon, "~");
    lv_obj_set_pos(icon, 12, 12);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x60A5FA), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 45, 8);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *val_label = lv_label_create(container);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(val_label, buf);
    lv_obj_align(val_label, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_set_style_text_color(val_label, lv_color_hex(0x60A5FA), 0);
    lv_obj_set_style_text_font(val_label, &lv_font_montserrat_16, 0);
    lv_obj_clear_flag(val_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *bar_bg = lv_obj_create(container);
    lv_obj_set_size(bar_bg, 360, 8);
    lv_obj_align(bar_bg, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_radius(bar_bg, 4, 0);
    lv_obj_set_style_bg_color(bar_bg, lv_color_hex(0x374151), 0);
    lv_obj_set_style_border_width(bar_bg, 0, 0);
    lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar = lv_obj_create(bar_bg);
    int bar_width = (int)(360.0f * (value - min_val) / (max_val - min_val));
    if (bar_width < 1) bar_width = 1;
    lv_obj_set_size(bar, bar_width, 8);
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(bar, 4, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x60A5FA), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *create_text_input_row(lv_obj_t *parent, int y, const char *title, const char *placeholder, bool is_password)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 390, 60);
    lv_obj_align(container, LV_ALIGN_TOP_LEFT, 10, y);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(container, 16, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 15, 10);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ta = lv_textarea_create(container);
    lv_obj_set_size(ta, 250, 32);
    lv_obj_align(ta, LV_ALIGN_BOTTOM_RIGHT, -15, -8);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_password_mode(ta, is_password);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x374151), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x4B5563), 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_text_color(ta, lv_color_white(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);
    return ta;
}

static lv_obj_t *create_button(lv_obj_t *parent, int y, const char *text, uint32_t color)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 370, 50);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 20, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return btn;
}

static void create_switch_row(lv_obj_t *parent, int y, const char *title, bool enabled)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 390, 60);
    lv_obj_align(container, LV_ALIGN_TOP_LEFT, 10, y);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(container, 16, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_label_create(container);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_set_pos(icon, 12, 18);
    lv_obj_set_style_text_color(icon, enabled ? lv_color_hex(0x34D399) : lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 45, 20);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *sw = lv_switch_create(container);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -12, 0);
    if (enabled) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *create_info_row(lv_obj_t *parent, int y, const char *title, const char *value)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 390, 50);
    lv_obj_align(container, LV_ALIGN_TOP_LEFT, 10, y);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 15, 15);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *value_label = lv_label_create(container);
    lv_label_set_text(value_label, value);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0xF3F4F6), 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(value_label, LV_OBJ_FLAG_CLICKABLE);
    return value_label;
}

static void create_menu_item(lv_obj_t *parent, int y, uint32_t color, const char *title, const char *hint)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_size(item, 370, 70);
    lv_obj_align(item, LV_ALIGN_TOP_LEFT, 10, y);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(item, 16, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_bg = lv_obj_create(item);
    lv_obj_set_size(icon_bg, 44, 44);
    lv_obj_align(icon_bg, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(icon_bg, lv_color_hex(color), 0);
    lv_obj_set_style_radius(icon_bg, 12, 0);
    lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = lv_label_create(icon_bg);
    lv_label_set_text(icon, LV_SYMBOL_RIGHT);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title_label = lv_label_create(item);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 65, 15);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *hint_label = lv_label_create(item);
    lv_label_set_text(hint_label, hint);
    lv_obj_set_pos(hint_label, 65, 40);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_12, 0);
    lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *arrow = lv_label_create(item);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x4B5563), 0);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_16, 0);
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
}

bool SettingsApp::run()
{
    ESP_UTILS_LOGD("Run Settings App");

    s_navigator = new PageNavigator(PAGE_COUNT);
    s_navigator->set_swipe_directions(LV_DIR_LEFT | LV_DIR_RIGHT);

    for (int i = 0; i < PAGE_COUNT; i++) {
        s_pages[i] = lv_obj_create(NULL);
        lv_obj_set_size(s_pages[i], 410, 502);
        lv_obj_set_style_bg_color(s_pages[i], lv_color_hex(0x0D1117), 0);
        lv_obj_add_event_cb(s_pages[i], gesture_handler, LV_EVENT_GESTURE, nullptr);
        s_navigator->add_page(i, s_pages[i]);
    }

    s_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(s_keyboard, 410, 200);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);

    // HOME page
    lv_obj_t *home_page = s_pages[PAGE_HOME];
    create_title(home_page, s_page_info[PAGE_HOME].title, s_page_info[PAGE_HOME].color);

    lv_obj_t *menu_container = lv_obj_create(home_page);
    lv_obj_set_size(menu_container, 390, 350);
    lv_obj_align(menu_container, LV_ALIGN_TOP_LEFT, 10, 85);
    lv_obj_set_style_bg_color(menu_container, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_radius(menu_container, 16, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_clear_flag(menu_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    create_menu_item(menu_container, 10, 0xF5A623, "Display", "Brightness, timeout");
    create_menu_item(menu_container, 85, 0x50E3C4, "Time", "Date, time, timezone");
    create_menu_item(menu_container, 160, 0x7B61FF, "Wi-Fi", "Network settings");
    create_menu_item(menu_container, 235, 0xA0A0A0, "About", "Device info");

    // DISPLAY page
    lv_obj_t *display_page = s_pages[PAGE_DISPLAY];
    create_title(display_page, s_page_info[PAGE_DISPLAY].title, s_page_info[PAGE_DISPLAY].color);
    create_slider_row(display_page, 90, "Brightness", impl->brightness, 0, 100);
    create_slider_row(display_page, 185, "Screen Timeout", impl->screen_timeout, 5, 120);
    create_switch_row(display_page, 280, "Auto Brightness", false);

    // TIME page
    lv_obj_t *time_page = s_pages[PAGE_TIME];
    create_title(time_page, s_page_info[PAGE_TIME].title, s_page_info[PAGE_TIME].color);
    create_info_row(time_page, 90, "Date", "2026-01-01");
    create_info_row(time_page, 145, "Time", "00:00:00");
    create_info_row(time_page, 200, "Timezone", "UTC+8");
    create_slider_row(time_page, 260, "Hour Format (24h)", 1, 0, 1);

    // WIFI page
    lv_obj_t *wifi_page = s_pages[PAGE_WIFI];
    create_title(wifi_page, s_page_info[PAGE_WIFI].title, s_page_info[PAGE_WIFI].color);
    
    s_wifi_ssid_input = create_text_input_row(wifi_page, 100, "SSID", "Enter WiFi name", false);
    lv_obj_add_event_cb(s_wifi_ssid_input, [](lv_event_t *e) {
        lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
        if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
            lv_keyboard_set_textarea(s_keyboard, ta);
            lv_obj_remove_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        } else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
            lv_keyboard_set_textarea(s_keyboard, NULL);
            lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_ALL, NULL);
    
    s_wifi_password_input = create_text_input_row(wifi_page, 170, "Password", "Enter password", true);
    lv_obj_add_event_cb(s_wifi_password_input, [](lv_event_t *e) {
        lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
        if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
            lv_keyboard_set_textarea(s_keyboard, ta);
            lv_obj_remove_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        } else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
            lv_keyboard_set_textarea(s_keyboard, NULL);
            lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_ALL, NULL);
    
    lv_obj_t *connect_btn = create_button(wifi_page, 240, "Start AP Config", 0x34D399);
    lv_obj_add_event_cb(connect_btn, [](lv_event_t *e) {
        if (wifi_is_ap_running()) {
            lv_label_set_text(s_wifi_status_label, "AP already running");
            return;
        }
        
        lv_label_set_text(s_wifi_status_label, "Starting AP...");
        esp_err_t ret = wifi_start_ap_config();
        if (ret == ESP_OK) {
            lv_label_set_text(s_wifi_status_label, "AP: ESP32-C6-Config");
        } else {
            lv_label_set_text(s_wifi_status_label, "AP failed");
        }
    }, LV_EVENT_CLICKED, nullptr);
    
    s_wifi_status_label = create_info_row(wifi_page, 290, "Status", "Not connected");
    s_wifi_ssid_label = create_info_row(wifi_page, 345, "SSID", "-");

    // ABOUT page
    lv_obj_t *about_page = s_pages[PAGE_ABOUT];
    create_title(about_page, s_page_info[PAGE_ABOUT].title, s_page_info[PAGE_ABOUT].color);
    create_info_row(about_page, 90, "Model", "ESP32-C6-Touch-AMOLED");
    create_info_row(about_page, 145, "Resolution", "410x502");
    create_info_row(about_page, 200, "Panel", "SH8601A");
    create_info_row(about_page, 255, "IMU", "QMI8658C");
    create_info_row(about_page, 310, "Firmware", "v1.0.0");

    s_navigator->set_screen(s_pages[0], false);
    s_navigator->set_current_page(0);

    return true;
}

bool SettingsApp::back()
{
    ESP_UTILS_LOGD("Back Settings App");

    if (s_keyboard) {
        lv_obj_del(s_keyboard);
        s_keyboard = nullptr;
    }

    delete s_navigator;
    s_navigator = nullptr;

    for (int i = 0; i < PAGE_COUNT; i++) {
        s_pages[i] = nullptr;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

extern "C" {

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, SettingsApp, APP_NAME, []()
{
    return std::shared_ptr<SettingsApp>(new SettingsApp(), [](SettingsApp *p) {});
})

}

}
