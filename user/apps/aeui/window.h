#pragma once

#include <lvgl.h>
#include <aether/window.h>

// 窗口结构体
typedef struct aeui_window
{
    uint64_t wid;               // 窗口id
    uint64_t pid;               // 所属进程id
    bool free;                  // 是否空闲
    const char *title_str;      // 标题字符串
    uint32_t *buffer;           // 窗口画布的帧缓冲区
    lv_obj_t *canvas;           // 画布
    lv_obj_t *parent;           // 父对象
    lv_obj_t *win;              // 窗口对象
    lv_obj_t *header;           // 头
    lv_obj_t *title;            // 标题栏
    lv_obj_t *close_btn;        // 关闭按钮
    lv_obj_t *restore_btn;      // 恢复按钮
    lv_obj_t *min_btn;          // 最小化按钮
    lv_coord_t width;           // 宽度
    lv_coord_t height;          // 高度
    uint64_t minimal_window_id; // 最小化窗口的id
    lv_coord_t min_width;       // 最小化宽度
    lv_coord_t min_height;      // 最小化高度
} aeui_win_t;

#define MAX_WINDOW_NUM 16

extern aeui_win_t windows[MAX_WINDOW_NUM];

aeui_win_t *create_window(lv_obj_t *parent, const char *title, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint64_t pid);
