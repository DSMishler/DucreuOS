#pragma once
#include <stdint.h>

// these headers copied from Dr. Marz.
#define CLINT_BASE_ADDRESS 0x2000000
#define CLINT_MTIMECMP_OFFSET 0x4000
#define CLINT_MTIMECMP_INFINITE 0x7FFFFFFFFFFFFFFFUL

void clint_set_msip(int hart);
void clint_clear_msip(int hart);
uint64_t clint_get_time();
void clint_set_mtimecmp(int hart, uint64_t val);
void clint_add_mtimecmp(int hart, uint64_t val);
void clint_print_mtimecmp(int hart);
