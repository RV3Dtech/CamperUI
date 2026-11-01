#include "ui_layout_metrics.h"
using namespace UiLayout;
static_assert(status_height+content_height+nav_height==screen_height);
static_assert(nav_button_width>=44 && nav_button_height>=44);
static_assert(!nav_overflows(0) && !nav_overflows(6));
static_assert(nav_overflows(7) && nav_overflows(12));
static_assert(nav_view_width(6)==480 && nav_view_width(12)==432);
static_assert(nav_target(0,256,1)==216);
static_assert(nav_target(216,40,1)==256);
static_assert(nav_target(256,0,-1)==40);
static_assert(nav_target(40,216,-1)==0);

// Original comfortable button geometry, requested after the compact trial.
static_assert(nav_button_width==68 && nav_gap==4);
static_assert(nav_overflows(7) && nav_view_width(7)==432);
