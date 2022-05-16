#pragma once
#include <../../sbi/src/include/svrcodes.h>
#include <stdint.h>

void sbi_putchar(char c);
char sbi_getchar(void);
void sbi_poweroff(void);

int sbi_get_hart_status(unsigned int hart);
int sbi_hart_start(unsigned int hart, uint64_t address, uint64_t frame);
int sbi_hart_stop(unsigned int hart);
int sbi_who_am_i(void);

uint64_t sbi_get_clint_time(void);
void sbi_set_clint_timer(int hart, uint64_t set);
void sbi_add_clint_timer(int hart, uint64_t add);
void sbi_acknowledge_clint_timer(void);
void sbi_show_clint_timer(int hart);
