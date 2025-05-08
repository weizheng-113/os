#pragma once

#include <libsyscall.h>
#include <stdint.h>

void get_mouse_xy(int32_t *x, int32_t *y);
bool mouse_click();
