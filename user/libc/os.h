#pragma once
#include <input.h>
#include <gpu.h>

void os_putchar(char c);
void os_console(void);
void os_sleep(int quantums);
void os_get_input(input_event_t *buffer, int len);
int  os_get_region(uint32_t x_raw, uint32_t y_raw);
void os_add_region(uint32_t x_start, uint32_t x_end,
                   uint32_t y_start, uint32_t y_end, int value);
void os_draw_rectangle(gpu_rectangle_t *rect, pixel_t *color);
void os_draw_circle(gpu_circle_t *circ, pixel_t *color);
uint32_t os_get_screen_x(void);
uint32_t os_get_screen_y(void);
