#include <drivers/framebuffer.h>
#include <fast_memcpy.h>

struct limine_framebuffer *current_framebuffer = NULL;

extern volatile struct limine_framebuffer_request framebuffer_request;

uint64_t sys_get_fbinfo(uint64_t width_ptr, uint64_t height_ptr, uint64_t pitch_ptr, uint64_t bpp_ptr)
{
    if (width_ptr)
        *(uint64_t *)width_ptr = current_framebuffer->width;
    if (height_ptr)
        *(uint64_t *)height_ptr = current_framebuffer->height;
    if (pitch_ptr)
        *(uint64_t *)pitch_ptr = current_framebuffer->pitch;
    if (bpp_ptr)
        *(uint8_t *)bpp_ptr = current_framebuffer->bpp;

    return 0;
}

uint64_t sys_write_fb(uint64_t x1, uint64_t y1, uint64_t x2, uint64_t y2, uint64_t buffer)
{
    uint64_t x_size = x2 - x1 + 1;

    for (uint64_t y = y1; y <= y2; y++)
    {
        fast_memcpy((uint32_t *)current_framebuffer->address + y * current_framebuffer->width + x1, (const uint32_t *)buffer + (y - y1) * x_size, x_size * sizeof(uint32_t));
    }

    return 0;
}

void fb_init()
{
    current_framebuffer = framebuffer_request.response->framebuffers[0];
}
