/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <aether/fb.h>
#include <aether/window.h>
#include <stdlib.h>
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#define LVGL_WINDOW_WIDTH 800
#define LVGL_WINDOW_HEIGHT 600
// #ifndef MY_DISP_HOR_RES
// #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
// #define MY_DISP_HOR_RES 320
// #endif

// #ifndef MY_DISP_VER_RES
// #warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
// #define MY_DISP_VER_RES 240
// #endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**********************
 *  STATIC VARIABLES
 **********************/
static uint64_t fb_width = 0;
static uint64_t fb_height = 0;
static uint64_t window_buffer_address = 0;
bool use_window = false;
extern int window_input_index;
extern int window_output_index;
extern uint64_t wid;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t *disp = lv_display_create(fb_width, fb_height);
    lv_display_set_flush_cb(disp, disp_flush);

    uint8_t *buffer = lv_malloc(fb_width * fb_height * sizeof(uint32_t));
    lv_display_set_buffers(disp, buffer, NULL, fb_width * fb_height * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);

    lv_display_set_default(disp);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    if (have_a_window())
    {
        // is normal program
        fb_width = LVGL_WINDOW_WIDTH;
        fb_height = LVGL_WINDOW_HEIGHT;

        uint64_t buffer = create_window_and_get_buffer("LVGL", 100, 100, LVGL_WINDOW_WIDTH, LVGL_WINDOW_HEIGHT);
        if ((int64_t)buffer < 0)
        {
            abort();
        }

        window_buffer_address = buffer;

        use_window = true;
    }
    else
    {
        // is system program
        get_fb_info(&fb_width, &fb_height, NULL, NULL);
    }
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    if (disp_flush_enabled)
    {
        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

        if (use_window)
        {
            uint64_t xsize = area->x2 - area->x1 + 1;

            for (lv_coord_t y = area->y1; y <= area->y2; y++)
            {
                lv_memcpy((uint32_t *)window_buffer_address + y * fb_width + area->x1, (const uint32_t *)px_map + (y - area->y1) * xsize, xsize * sizeof(uint32_t));
            }
        }
        else
        {
            write_framebuffer(area->x1, area->y1, area->x2, area->y2, px_map);
        }
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_display_flush_ready(disp_drv);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
