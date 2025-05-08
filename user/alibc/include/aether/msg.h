#pragma once

#include <libsyscall.h>

enum
{
    // 对于window manager的输入
    MSG_FLAGS_WINDOW_MANAGER_INPUT = 1024,
    // 对于window manager的输出
    MSG_FLAGS_WINDOW_MANAGER_OUTPUT,
    // 最大FLAGS数
    MSG_FLAGS_NUM = 2048,
};

int msg_create();
void msg_destroy(int index);
int msgfind_by_flag(int flags, int *index);
int msg_set_flag(int index, int flags);
int msg_read(int index, void *buffer, size_t len);
int msg_write(int index, const void *buffer, size_t len);

uint64_t send_window_msg(int window_input_index, int window_output_index, const char *cmd);
