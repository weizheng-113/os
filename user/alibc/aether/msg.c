#include <aether/msg.h>
#include <string.h>

int msg_create()
{
    return enter_syscall(0, 0, 0, 0, 0, SYS_MSG_CREATE);
}

void msg_destroy(int index)
{
    enter_syscall(index, 0, 0, 0, 0, SYS_MSG_DESTROY);
}

int msgfind_by_flag(int flags, int *index)
{
    return enter_syscall(flags, (uint64_t)index, 0, 0, 0, SYS_MSG_FIND);
}

int msg_set_flag(int index, int flags)
{
    return enter_syscall(index, flags, 0, 0, 0, SYS_MSG_SET_FLAGS);
}

int msg_read(int index, void *buffer, size_t len)
{
    return enter_syscall(index, (uint64_t)buffer, len, 0, 0, SYS_MSG_READ);
}

int msg_write(int index, const void *buffer, size_t len)
{
    return enter_syscall(index, (uint64_t)buffer, len, 0, 0, SYS_MSG_WRITE);
}

uint64_t send_window_msg(int window_input_index, int window_output_index, const char *cmd)
{
    msg_write(window_input_index, cmd, strlen(cmd) + 1);
    int64_t ret = 0;
    msg_read(window_output_index, &ret, sizeof(int64_t));
    while (ret == 0)
    {
        __asm__ __volatile__("pause");
        msg_read(window_output_index, &ret, sizeof(int64_t));
    }
    int64_t tmp = 0;
    msg_write(window_output_index, (const int64_t *)&tmp, sizeof(int64_t));
    if (ret < 0)
    {
        return (uint64_t)ret;
    }

    return (uint64_t)ret;
}
