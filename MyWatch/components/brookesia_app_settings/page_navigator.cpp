#include "page_navigator.hpp"

PageNavigator::PageNavigator(int page_count)
    : _page_count(page_count), _current_page(-1)
{
    _pages = new lv_obj_t *[_page_count] {nullptr};
}

PageNavigator::~PageNavigator()
{
    delete[] _pages;
}

void PageNavigator::add_page(int index, lv_obj_t *page)
{
    if (index < 0 || index >= _page_count) return;
    _pages[index] = page;
}

void PageNavigator::set_screen(lv_obj_t *screen, bool animate)
{
    if (screen == nullptr) return;
    if (animate) {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    } else {
        lv_scr_load(screen);
    }
}

void PageNavigator::set_current_page(int page_index, bool animate)
{
    if (page_index < 0 || page_index >= _page_count) return;
    _current_page = page_index;
    set_screen(_pages[_current_page], animate);
}

void PageNavigator::navigate_next()
{
    if (_current_page < 0 || _page_count <= 0) return;
    int next = (_current_page + 1) % _page_count;
    _current_page = next;
    set_screen(_pages[_current_page], true);
}

void PageNavigator::navigate_prev()
{
    if (_current_page < 0 || _page_count <= 0) return;
    int prev = (_current_page - 1 + _page_count) % _page_count;
    _current_page = prev;
    set_screen(_pages[_current_page], true);
}
