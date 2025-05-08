#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <aether/mouse.h>
#include <aether/msg.h>
#include "aeui.h"

extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);

extern uint64_t current_top_wid;
extern bool minimal_windows_num[MAX_WINDOW_NUM];

#define MAX_ARGC 64
#define MAX_ARG_LEN 256

typedef enum
{
    STATE_DEFAULT,
    STATE_SINGLE_QUOTE,
    STATE_DOUBLE_QUOTE,
    STATE_ESCAPE
} parse_state;

char **parse_command(const char *input, int *argc)
{
    char **argv = malloc(MAX_ARGC * sizeof(char *));
    char current_arg[MAX_ARG_LEN];
    int arg_len = 0;
    parse_state state = STATE_DEFAULT;
    *argc = 0;

    for (const char *p = input; *p != '\0'; p++)
    {
        char c = *p;

        switch (state)
        {
        case STATE_DEFAULT:
            if (c == '\\')
            {
                state = STATE_ESCAPE;
            }
            else if (c == '\'')
            {
                state = STATE_SINGLE_QUOTE;
            }
            else if (c == '"')
            {
                state = STATE_DOUBLE_QUOTE;
            }
            else if (c == ' ' || c == '\t')
            {
                if (arg_len > 0)
                {
                    current_arg[arg_len] = '\0';
                    argv[(*argc)++] = strdup(current_arg);
                    arg_len = 0;
                }
            }
            else
            {
                current_arg[arg_len++] = c;
            }
            break;

        case STATE_SINGLE_QUOTE:
            if (c == '\'')
            {
                state = STATE_DEFAULT;
            }
            else
            {
                current_arg[arg_len++] = c;
            }
            break;

        case STATE_DOUBLE_QUOTE:
            if (c == '"')
            {
                state = STATE_DEFAULT;
            }
            else if (c == '\\')
            {
                state = STATE_ESCAPE;
            }
            else
            {
                current_arg[arg_len++] = c;
            }
            break;

        case STATE_ESCAPE:
            current_arg[arg_len++] = c;
            state = (state == STATE_DOUBLE_QUOTE) ? STATE_DOUBLE_QUOTE : STATE_DEFAULT;
            break;
        }

        if (arg_len >= MAX_ARG_LEN - 1 || *argc >= MAX_ARGC - 1)
        {
            break; // 防止缓冲区溢出
        }
    }

    // 处理最后一个参数
    if (arg_len > 0)
    {
        current_arg[arg_len] = '\0';
        argv[(*argc)++] = strdup(current_arg);
    }

    argv[*argc] = NULL; // 以NULL结尾
    return argv;
}

void free_argv(int argc, char **argv)
{
    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}

uint64_t handle_command(char *buffer)
{
    int argc;
    char **argv = parse_command(buffer, &argc);

    if (argc < 2)
    {
        free_argv(argc, argv);
        return (uint64_t)-EINVAL;
    }
    if (strcmp(argv[0], "windowmanager"))
    {
        free_argv(argc, argv);
        return (uint64_t)-EINVAL;
    }
    if (!strcmp(argv[1], "create"))
    {
        if (argc < 8)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        char *title_str = argv[2];
        char *x_str = argv[3];
        char *y_str = argv[4];
        char *width_str = argv[5];
        char *height_str = argv[6];
        char *pid_str = argv[7];

        int x = atoi((const char *)x_str);
        int y = atoi((const char *)y_str);
        int width = atoi((const char *)width_str);
        int height = atoi((const char *)height_str);
        int pid = atoi(pid_str);
        if ((x < 0 || y < 0 || width < 0 || height < 0) ||
            ((x + width) > lv_obj_get_width(lv_screen_active()) || ((y + height) > lv_obj_get_height(lv_screen_active()))))
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        aeui_win_t *win = create_window(NULL, (const char *)title_str, x, y, width, height, (uint64_t)pid);
        if (!win)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }
        window_set_top(win->pid);

        free_argv(argc, argv);
        return win->wid;
    }
    else if (!strcmp(argv[1], "close"))
    {
        if (argc < 3)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        char *title_str = argv[2];

        for (int i = 0; i < MAX_WINDOW_NUM; i++)
        {
            if (!strcmp(windows[i].title_str, title_str))
            {
                lv_obj_delete(windows[i].win);
                free_shared_memory(windows[i].buffer);
                windows[i].free = true;
                break;
            }
        }

        free_argv(argc, argv);
        return 1;
    }
    else if (!strcmp(argv[1], "getbuffer"))
    {
        if (argc < 3)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        char *wid_str = argv[2];

        int wid = atoi(wid_str) - 1;
        if (wid >= MAX_WINDOW_NUM || wid < 0)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        free_argv(argc, argv);
        return (uint64_t)windows[wid].buffer;
    }
    else if (!strcmp(argv[1], "get_xy_pressed"))
    {
        if (argc < 3)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        char *wid_str = argv[2];
        int wid = atoi(wid_str); // Do not minus one
        if (wid >= MAX_WINDOW_NUM || wid < 1)
        {
            free_argv(argc, argv);
            return (uint64_t)-EINVAL;
        }

        if (current_top_wid != 0 && wid == current_top_wid)
        {
            lv_point_t point;
            get_mouse_xy(&point.x, &point.y);
            lv_coord_t x = point.x - lv_obj_get_x(windows[wid - 1].win);
            lv_coord_t y = point.y - lv_obj_get_y(windows[wid - 1].win) - (BUTTON_SIZE + 10);
            if (x < 0)
                x = 0;
            if (y < 0)
                y = 0;

            lv_coord_t win_width = lv_obj_get_width(windows[wid - 1].win);
            lv_coord_t win_height = lv_obj_get_height(windows[wid - 1].win);

            if (x > win_width)
                x = win_width;
            if (y > win_height)
                y = win_height;

            uint64_t pressed = (uint64_t)mouse_click();

            free_argv(argc, argv);
            return (((pressed & 0xFFFFFFFF) << 32) | ((y & 0xFFFF) << 16) | (x & 0xFFFF));
        }

        free_argv(argc, argv);
        return 0x7FFFFFFFFFFFFFFF;
    }

    free_argv(argc, argv);
    return (uint64_t)-ENOSYS;
}

void flush_all_windows()
{
    for (int wid = 0; wid < MAX_WINDOW_NUM; wid++)
    {
        if (windows[wid].free)
            continue;

        if (!windows[wid].canvas)
            continue;

        if (windows[wid].pid != 0 && !have_proc(windows[wid].pid))
        {
            lv_obj_delete(windows[wid].win);
            free_shared_memory(windows[wid].buffer);
            windows[wid].buffer = NULL;
            windows[wid].win = NULL;
            windows[wid].free = true;

            for (int i = 0; i < MAX_WINDOW_NUM; i++)
            {
                if (!windows[i].free && windows[i].pid == windows[wid].pid && windows[i].restore_btn)
                {
                    lv_obj_delete(windows[i].restore_btn);
                    windows[i].restore_btn = NULL;
                    minimal_windows_num[windows[wid].minimal_window_id] = false;
                    windows[wid].minimal_window_id = 0;
                }
            }

            windows[wid].pid = 0;

            window_set_top(0);
            current_top_wid = 0;

            continue;
        }

        lv_obj_invalidate(windows[wid].canvas);
    }
}

int input_index;
int output_index;

extern void init_heap();

void _start()
{
    init_heap();

    signal(SIGQUIT, 0);

    lv_init();

    lv_port_disp_init();
    lv_port_indev_init();

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x01847f), LV_PART_MAIN);

    input_index = msg_create();
    msg_set_flag(input_index, MSG_FLAGS_WINDOW_MANAGER_INPUT);
    output_index = msg_create();
    msg_set_flag(output_index, MSG_FLAGS_WINDOW_MANAGER_OUTPUT);
    uint64_t initial_value = 0;
    msg_write(output_index, &initial_value, sizeof(uint64_t));

    char buffer[256];
    memset(buffer, 0, 256);

    while (1)
    {
        lv_task_handler();
        usleep(50);

        msg_read(input_index, buffer, 255);
        if (strlen(buffer) != 0)
        {
            uint64_t ret = handle_command(buffer);
            memset(buffer, 0, 256);
            int64_t tmp = 0;
            msg_write(input_index, &tmp, sizeof(int64_t));
            msg_write(output_index, &ret, sizeof(uint64_t));
        }

        flush_all_windows();

        lv_tick_inc(1);

        __asm__ __volatile__("pause");
    }

    msg_destroy(input_index);
    msg_destroy(output_index);

    exit(0);
}
