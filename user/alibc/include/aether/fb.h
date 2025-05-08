#pragma once

#include <libsyscall.h>

int get_fb_info(uint64_t *width, uint64_t *height, uint64_t *pitch, uint8_t *bpp);
int write_framebuffer(uint64_t x1, uint64_t y1, uint64_t x2, uint64_t y2, uint8_t *px_map);
