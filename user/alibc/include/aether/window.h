#pragma once

#include <aether/msg.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400

#define BUTTON_SIZE 30
#define MINIMAL_BUTTON_SIZE 40

void window_init();

uint64_t create_window_and_get_buffer(const char *title, int x, int y, int xsize, int ysize);

void close_a_window(const char *title);

uint64_t set_window(uint64_t window_buffer, uint64_t width, uint64_t height);
bool have_a_window();

void window_set_minimal(uint64_t pid);
void window_set_restored(uint64_t pid);
void window_set_top(uint64_t pid);
