#include <os.h>
#include <gpu.h>
#include "../../src/include/syscodes.h"

// just like OS->SBI's putchar
void os_putchar(char c)
{
   asm volatile("mv a7, %0\nmv a0, %1\necall" : :
                "r"(SYSCALL_PUTCHAR), "r"(c) : "a7", "a0");
}

void os_console(void)
{
   asm volatile("mv a7, %0\necall" : : "r"(SYSCALL_CONSOLE) : "a7");
}


void os_sleep(int quantums)
{
   asm volatile("mv a7, %0\nmv a0, %1\necall" : :
                "r"(SYSCALL_SLEEP), "r"(quantums) : "a7", "a0");
}

void os_get_input(input_event_t *buffer, int len)
{
   asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\necall" : :
                "r"(SYSCALL_GET_INPUT), "r"(buffer), "r"(len) :
                "a7", "a0", "a1");
}

int os_get_region(uint32_t x_raw, uint32_t y_raw)
{
   int region;
   asm volatile("mv a7, %1\nmv a0, %2\nmv a1, %3\necall\nmv %0, a0" :
                "=r"(region) :
                "r"(SYSCALL_GET_REGION), "r"(x_raw), "r"(y_raw) :
                "a7", "a0", "a1");
   return region;
}

void os_add_region(uint32_t x_start, uint32_t x_end,
                   uint32_t y_start, uint32_t y_end, int value)
{
   asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\n"
                "mv a2, %3\nmv a3, %4\nmv a4, %5\necall" : :
                "r"(SYSCALL_ADD_REGION), "r"(x_start), "r"(x_end),
                "r"(y_start), "r"(y_end), "r"(value) :
                "a7", "a0", "a1", "a2", "a3", "a4", "a5");
}

void os_draw_rectangle(gpu_rectangle_t *rect, pixel_t *color)
{
   asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\necall" : :
                "r"(SYSCALL_DRAW_RECTANGLE), "r"(rect), "r"(color) :
                "a7", "a0", "a1");
}

void os_draw_circle(gpu_circle_t *circ, pixel_t *color)
{
   asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\necall" : :
                "r"(SYSCALL_DRAW_CIRCLE), "r"(circ), "r"(color) :
                "a7", "a0", "a1");
}

uint32_t os_get_screen_x(void)
{
   int x_max;
   asm volatile("mv a7, %1\necall\nmv %0, a0" :
                "=r"(x_max) : "r"(SYSCALL_GET_SCREEN_X) :
                "a7", "a0", "a1");
   return x_max;
}

uint32_t os_get_screen_y(void)
{
   int y_max;
   asm volatile("mv a7, %1\necall\nmv %0, a0" :
                "=r"(y_max) : "r"(SYSCALL_GET_SCREEN_Y) :
                "a7", "a0", "a1");
   return y_max;
}
