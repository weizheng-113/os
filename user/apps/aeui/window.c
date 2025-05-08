#include <window.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <aether/window.h>

bool minimal_windows_num[MAX_WINDOW_NUM];
aeui_win_t windows[MAX_WINDOW_NUM];
bool initialized = false;
uint64_t current_top_wid = 0;

aeui_win_t *get_free_window()
{
    for (uint64_t i = 0; i < MAX_WINDOW_NUM; i++)
    {
        if (windows[i].free)
        {
            windows[i].wid = (i + 1);
            windows[i].free = false;
            windows[i].pid = 0;
            return &windows[i];
        }
    }
    return NULL;
}

void move_window(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSING)
    {

        aeui_win_t *win = lv_event_get_user_data(e);

        lv_indev_t *indev = lv_indev_active();
        if (!indev)
            return;

        lv_point_t vect;
        lv_indev_get_vect(indev, &vect);
        lv_coord_t x = lv_obj_get_x(win->win) + vect.x;
        lv_coord_t y = lv_obj_get_y(win->win) + vect.y;

        lv_coord_t width = lv_obj_get_width(lv_screen_active());
        lv_coord_t height = lv_obj_get_height(lv_screen_active());

        lv_coord_t win_width = lv_obj_get_width(win->win);
        lv_coord_t win_height = lv_obj_get_height(win->win);

        if (x < 0)
            x = 0;
        if (y < (0 + MINIMAL_BUTTON_SIZE))
            y = 0 + MINIMAL_BUTTON_SIZE;
        if (x >= (width - win_width))
            x = width - win_width;
        if (y >= (height - win_height))
            y = height - win_height;

        lv_obj_move_foreground(win->win);

        window_set_top(win->pid);
        current_top_wid = win->wid;

        lv_obj_set_pos(win->win, x, y);
    }
}

void close_window(lv_event_t *e)
{
    aeui_win_t *cwin = (aeui_win_t *)lv_event_get_user_data(e);

    kill(cwin->pid);

    lv_obj_delete(cwin->win);
    free_shared_memory(cwin->buffer);
    cwin->buffer = NULL;
    cwin->win = NULL;
    cwin->free = true;

    for (int i = 0; i < MAX_WINDOW_NUM; i++)
    {
        if (!windows[i].free && windows[i].pid == cwin->pid && windows[i].restore_btn)
        {
            lv_obj_delete(windows[i].restore_btn);
            windows[i].restore_btn = NULL;
            minimal_windows_num[cwin->minimal_window_id] = false;
            cwin->minimal_window_id = 0;
        }
    }

    cwin->pid = 0;

    window_set_top(0);
    current_top_wid = 0;
}

void restore_window(lv_event_t *e)
{
    aeui_win_t *cwin = (aeui_win_t *)lv_event_get_user_data(e);
    lv_obj_delete(cwin->restore_btn);
    cwin->restore_btn = NULL;

    minimal_windows_num[cwin->minimal_window_id] = false;
    cwin->minimal_window_id = 0;

    lv_obj_move_foreground(cwin->win);

    lv_obj_remove_flag(cwin->win, LV_OBJ_FLAG_HIDDEN);

    window_set_restored(cwin->pid);

    current_top_wid = cwin->wid;
}

void minimal_window(lv_event_t *e)
{
    aeui_win_t *cwin = (aeui_win_t *)lv_event_get_user_data(e);

    cwin->restore_btn = lv_button_create(lv_screen_active());

    uint64_t minimal_window_num;
    for (minimal_window_num = 0; minimal_window_num < MAX_WINDOW_NUM; minimal_window_num++)
        if (!minimal_windows_num[minimal_window_num])
            break;

    minimal_windows_num[minimal_window_num] = true;
    cwin->minimal_window_id = minimal_window_num;

    // 放在左上角
    uint64_t x = minimal_window_num * MINIMAL_BUTTON_SIZE;
    lv_obj_set_pos(cwin->restore_btn, x, 0);
    lv_obj_set_size(cwin->restore_btn, MINIMAL_BUTTON_SIZE, MINIMAL_BUTTON_SIZE);
    lv_obj_add_event_cb(cwin->restore_btn, restore_window, LV_EVENT_CLICKED, cwin);
    // 设置文字
    lv_obj_t *image = lv_image_create(cwin->restore_btn);
    lv_image_set_src(image, LV_SYMBOL_PLUS);
    lv_obj_center(image);

    lv_obj_add_flag(cwin->win, LV_OBJ_FLAG_HIDDEN);

    window_set_minimal(cwin->pid);

    current_top_wid = 0;
}

aeui_win_t *create_window(lv_obj_t *parent, const char *title, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint64_t pid)
{
    if (!initialized)
    {
        memset(windows, 0, sizeof(windows));
        for (uint64_t i = 0; i < MAX_WINDOW_NUM; i++)
        {
            windows[i].free = true;
        }
        memset(minimal_windows_num, false, sizeof(minimal_windows_num));
        initialized = true;
    }

    if (y < MINIMAL_BUTTON_SIZE)
        y = MINIMAL_BUTTON_SIZE;
    if (x < 0)
        x = 0;

    if (w < (BUTTON_SIZE + 100) || h < BUTTON_SIZE)
        return NULL;

    if (parent == NULL)
        parent = lv_screen_active();

    aeui_win_t *cwin = get_free_window();
    if (cwin == NULL)
        return cwin;

    cwin->buffer = alloc_shared_memory(w * h * sizeof(uint32_t));
    if (!cwin->buffer)
    {
        cwin->free = true;
        return NULL;
    }
    memset(cwin->buffer, 0xff, w * h * sizeof(uint32_t));

    cwin->pid = pid;
    cwin->parent = parent;
    cwin->min_width = BUTTON_SIZE;
    cwin->min_height = BUTTON_SIZE;
    cwin->title_str = title;

    cwin->width = w;
    cwin->height = h;

    cwin->win = lv_win_create(parent);
    lv_obj_move_foreground(cwin->win);
    lv_obj_set_size(cwin->win, w, h);
    lv_obj_set_pos(cwin->win, x, y);
    lv_obj_set_user_data(cwin->win, cwin);

    cwin->header = lv_win_get_header(cwin->win);
    lv_obj_set_height(cwin->header, BUTTON_SIZE + 10);
    lv_obj_remove_flag(cwin->header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(cwin->header, move_window, LV_EVENT_ALL, cwin);

    cwin->title = lv_win_add_title(cwin->win, cwin->title_str);
    lv_obj_set_height(cwin->title, BUTTON_SIZE);
    lv_obj_center(cwin->title);

    cwin->min_btn = lv_win_add_button(cwin->win, LV_SYMBOL_MINUS, BUTTON_SIZE);
    lv_obj_set_size(cwin->min_btn, BUTTON_SIZE, BUTTON_SIZE);
    lv_obj_set_pos(cwin->min_btn, w - BUTTON_SIZE * 2, 0);
    lv_obj_add_event_cb(cwin->min_btn, minimal_window, LV_EVENT_CLICKED, cwin);

    cwin->close_btn = lv_win_add_button(cwin->win, LV_SYMBOL_CLOSE, BUTTON_SIZE);
    lv_obj_set_size(cwin->close_btn, BUTTON_SIZE, BUTTON_SIZE);
    lv_obj_set_pos(cwin->min_btn, w - BUTTON_SIZE, 0);
    lv_obj_add_event_cb(cwin->close_btn, close_window, LV_EVENT_CLICKED, cwin);

    lv_obj_delete(lv_win_get_content(cwin->win));
    cwin->canvas = lv_canvas_create(cwin->win);
    lv_obj_set_pos(cwin->canvas, 0, BUTTON_SIZE + 10);
    lv_obj_set_size(cwin->canvas, cwin->width, cwin->height - (BUTTON_SIZE + 10));
    lv_canvas_set_buffer(cwin->canvas, cwin->buffer, cwin->width, cwin->height - (BUTTON_SIZE + 10), LV_COLOR_FORMAT_XRGB8888);
    lv_obj_move_foreground(cwin->canvas);

    return cwin;
}
