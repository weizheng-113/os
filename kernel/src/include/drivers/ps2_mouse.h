#pragma once

#include <klibc.h>

typedef struct
{
    uint8_t buf[3], phase;
    int x, y, btn;
    char roll;
    bool left;
    bool center;
    bool right;
} mouse_dec;

void sys_get_mouse_xy(int32_t *x, int32_t *y);
bool sys_mouse_click();
