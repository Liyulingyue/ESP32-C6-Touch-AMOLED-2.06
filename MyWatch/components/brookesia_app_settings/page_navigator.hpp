#pragma once

#include "lvgl.h"
#include <cstdint>

class PageNavigator {
public:
    PageNavigator(int page_count);
    ~PageNavigator();

    void add_page(int index, lv_obj_t *page);
    void set_screen(lv_obj_t *screen, bool animate = false);
    void set_current_page(int page_index, bool animate = true);
    void navigate_next();
    void navigate_prev();
    void set_swipe_directions(uint32_t directions) { _swipe_directions = directions; }
    uint32_t get_swipe_directions() const { return _swipe_directions; }
    int get_current_page() const { return _current_page; }

private:
    lv_obj_t **_pages = nullptr;
    int _page_count = 0;
    int _current_page = -1;
    uint32_t _swipe_directions = LV_DIR_LEFT | LV_DIR_RIGHT;
};
