#include <aether/mouse.h>

void get_mouse_xy(int32_t *x, int32_t *y)
{
    enter_syscall((uint64_t)x, (uint64_t)y, 0, 0, 0, SYS_GET_MOUSE_XY);
}

bool mouse_click()
{
    return (bool)enter_syscall(0, 0, 0, 0, 0, SYS_MOUSE_CLICK);
}
