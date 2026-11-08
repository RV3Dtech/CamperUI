#pragma once
#include <stddef.h>
#include <stdint.h>
void ui_display_battery_open();
void ui_display_battery_update();
void ui_display_battery_reset();
void ui_format_battery_runtime(char *text, size_t size, uint64_t elapsed_ms, bool lower_bound = false);
