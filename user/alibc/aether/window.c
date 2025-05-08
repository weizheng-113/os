#include <aether/window.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int window_input_index = -1;
int window_output_index = -1;
uint64_t wid;

void window_init()
{
    while (msgfind_by_flag(MSG_FLAGS_WINDOW_MANAGER_INPUT, &window_input_index) != 0)
    {
        __asm__ __volatile__("pause");
    }

    while (msgfind_by_flag(MSG_FLAGS_WINDOW_MANAGER_OUTPUT, &window_output_index) != 0)
    {
        __asm__ __volatile__("pause");
    }
}

uint64_t create_window_and_get_buffer(const char *title, int x, int y, int xsize, int ysize)
{
    char buf[64];
    sprintf(buf, "windowmanager create %s %d %d %d %d %d", title, x, y, xsize, ysize, getpid());
    uint64_t ret = send_window_msg(window_input_index, window_output_index, buf);
    if ((int64_t)ret < 0)
        return (int)ret;

    wid = ret;

    char cmd[32];
    memset(cmd, 0, 32);
    sprintf(cmd, "windowmanager getbuffer %d", wid);

    uint64_t buffer = send_window_msg(window_input_index, window_output_index, cmd);
    if ((int64_t)buffer < 0)
    {
        return (int)buffer;
    }

    return buffer;
}

void close_a_window(const char *title)
{
    char buf[32];
    sprintf(buf, "windowmanager close %s", title);
    uint64_t ret = send_window_msg(window_input_index, window_output_index, buf);
}

uint64_t set_window(uint64_t window_buffer, uint64_t width, uint64_t height)
{
    return enter_syscall(window_buffer, width, height, 0, 0, SYS_SET_WINDOW);
}

bool have_a_window()
{
    return enter_syscall(0, 0, 0, 0, 0, SYS_HAVE_A_WINDOW);
}

void window_set_minimal(uint64_t pid)
{
    enter_syscall(pid, 0, 0, 0, 0, SYS_WINDOW_SET_MINIMAL);
}

void window_set_restored(uint64_t pid)
{
    enter_syscall(pid, 0, 0, 0, 0, SYS_WINDOW_SET_RESTORED);
}

void window_set_top(uint64_t pid)
{
    enter_syscall(pid, 0, 0, 0, 0, SYS_WINDOW_SET_TOP);
}
