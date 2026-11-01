#pragma once
namespace UiLayout {
constexpr int screen_width=480, screen_height=480, status_height=40, nav_height=60;
constexpr int content_height=screen_height-status_height-nav_height;
constexpr int nav_button_width=68, nav_button_height=48, nav_gap=4, nav_padding=3, nav_arrow_width=24;
constexpr int nav_span(int count) { return count>0 ? count*nav_button_width+(count-1)*nav_gap : 0; }
constexpr bool nav_overflows(int count) { return nav_span(count)>screen_width-2*nav_padding; }
constexpr int nav_view_width(int count) { return screen_width-(nav_overflows(count)?2*nav_arrow_width:0); }
constexpr int nav_target(int current,int remaining,int direction) {
    const int maximum=current+(remaining>0?remaining:0);
    const int desired=current+direction*3*(nav_button_width+nav_gap);
    return desired<0?0:desired>maximum?maximum:desired;
}
}
