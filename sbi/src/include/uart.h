// safeguard: only load this header file once
#pragma once

void uart_init(void);
char uart_get(void);
void uart_put(char);

void uart_buffer_push(char);
char uart_buffer_pop(void);

void uart_handle_irq(void);
