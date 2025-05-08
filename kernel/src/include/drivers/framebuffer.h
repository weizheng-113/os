#pragma once

#include <klibc.h>

extern struct limine_framebuffer *current_framebuffer;

uint64_t sys_get_fbinfo(uint64_t width_ptr, uint64_t height_ptr, uint64_t pitch_ptr, uint64_t bpp_ptr);
uint64_t sys_write_fb(uint64_t x1, uint64_t y1, uint64_t x2, uint64_t y2, uint64_t buffer);
void fb_init();
