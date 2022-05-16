#pragma once
#include <stdint.h>
#include <process.h>

int copy_to_user(void *os_vaddr, void *user_vaddr, uint32_t size, process_t *p);
int copy_to_os(void *os_vaddr, void *user_vaddr, uint32_t size, process_t *p);
