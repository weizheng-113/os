#include <task/msg.h>
#include <mm/heap.h>

static MsgPool g_pool;      // 全局消息池实例
static int initialized = 0; // 初始化标志
spinlock_t msg_op_lock;

// 创建消息（返回消息索引）
int msgpool_create(void)
{
    if (!initialized)
    {
        spin_init(&msg_op_lock);
        // 初始化所有消息单元
        for (int i = 0; i < MAX_MSG; i++)
        {
            g_pool.units[i].buffer = NULL;
            g_pool.units[i].flag = 0; // 初始标志位设为0
        }
        g_pool.free_count = MAX_MSG;
        initialized = 1;
    }

    if (g_pool.free_count == 0)
        return -1; // 池已满

    int index;
    for (index = 0; index < MAX_MSG; index++)
    {
        if (g_pool.units[index].buffer == NULL)
            break;
    }
    if (index == MAX_MSG)
        return -1;
    g_pool.free_count--;

    // 分配内存并清零
    g_pool.units[index].buffer = (char *)malloc(MSG_SIZE);
    if (!g_pool.units[index].buffer)
    {
        // 内存分配失败时回滚状态
        g_pool.free_count++;
        return -1;
    }
    memset(g_pool.units[index].buffer, 0, MSG_SIZE);
    g_pool.units[index].flag = 0; // 重置标志位

    return index;
}

// 设置消息标志位
int msgpool_set_flag(int index, int flag)
{
    if (index < 0 || index >= MAX_MSG || !g_pool.units[index].buffer)
        return -1;
    g_pool.units[index].flag = flag;
    return 0;
}

// 通过标志位查找消息（返回消息索引）
int msgpool_find_by_flag(int flag, int *out_index)
{
    for (int i = 0; i < MAX_MSG; i++)
    {
        if (g_pool.units[i].buffer && g_pool.units[i].flag == flag)
        {
            *out_index = i;
            return 0; // 找到
        }
    }
    return -1; // 未找到
}

// 销毁消息（释放资源）
void msgpool_destroy(int index)
{
    if (index < 0 || index >= MAX_MSG || !g_pool.units[index].buffer)
        return;

    // 释放内存并标记为空闲
    free(g_pool.units[index].buffer);
    g_pool.units[index].buffer = NULL;
    g_pool.units[index].flag = 0; // 重置标志位

    g_pool.free_count++;
}

// 读取消息（返回实际读取长度）
int msgpool_read(int index, void *buffer, size_t len)
{
    if (index < 0 || index >= MAX_MSG || !g_pool.units[index].buffer)
        return -1;

    spin_lock(&msg_op_lock);

    // 计算实际可读取长度
    size_t copy_len = (len < MSG_SIZE) ? len : MSG_SIZE - 1;
    memcpy(buffer, g_pool.units[index].buffer, copy_len);
    ((char *)buffer)[copy_len] = '\0'; // 添加字符串结束符

    spin_unlock(&msg_op_lock);

    return (int)copy_len;
}

// 写入消息（返回实际写入长度）
int msgpool_write(int index, const void *data, size_t len)
{
    if (index < 0 || index >= MAX_MSG || !g_pool.units[index].buffer)
        return -1;

    spin_lock(&msg_op_lock);

    // 计算实际可写入长度
    size_t copy_len = (len < MSG_SIZE) ? len : MSG_SIZE - 1;
    memcpy(g_pool.units[index].buffer, (void *)data, copy_len);
    g_pool.units[index].buffer[copy_len] = '\0'; // 添加字符串结束符

    spin_unlock(&msg_op_lock);

    return (int)copy_len;
}
