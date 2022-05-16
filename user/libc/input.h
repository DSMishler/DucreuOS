#pragma once
#include <stdint.h>

typedef struct input_event
{
   uint16_t type;
   uint16_t code;
   uint32_t value;
} input_event_t;

typedef struct input_cursor_position
{
   uint32_t x;
   uint32_t y;
} input_cursor_position_t;

char input_key_code_to_char(uint32_t code);
void input_print_event(input_event_t *iev);
