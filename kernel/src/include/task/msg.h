#pragma once

#include <klibc.h>

#define MAX_MSG 128  // 最大消息数量
#define MSG_SIZE 256 // 单条消息最大长度

// 消息单元结构体
typedef struct
{
    char *buffer; // 消息缓冲区
    int flag;     // 消息标志位
} MsgUnit;

// 消息池结构体
typedef struct
{
    MsgUnit units[MAX_MSG]; // 消息单元数组
    int free_count;         // 空闲索引数量
} MsgPool;

int msgpool_create(void);
int msgpool_set_flag(int index, int flag);
int msgpool_find_by_flag(int flag, int *out_index);
void msgpool_destroy(int index);
int msgpool_read(int index, void *buffer, size_t len);
int msgpool_write(int index, const void *data, size_t len);
