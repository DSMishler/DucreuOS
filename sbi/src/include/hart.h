#pragma once
#include <stdint.h>

// NOTE: this is dupliated in src/main.c
#define MAX_SUPPORTED_HARTS 8


typedef enum hart_status
{
   HS_STOPPED,
   HS_STARTING,
   HS_STARTED,
   HS_STOPPING,
   HS_INVALID
} hart_status_t;

typedef enum hart_privilege_mode
{
   HPM_USER,
   HPM_SUPERVISOR,
   HPM_HYPERVISOR,
   HPM_MACHINE
} hart_privilege_mode_t;

typedef struct hart_data
{
   hart_status_t status;
   uint64_t target_address;
   uint64_t process_frame; // no guarantee it will be always up-to-date.
} hart_data_t;

extern hart_data_t sbi_hart_data[8];

hart_status_t get_hart_status(unsigned int hart);
int hart_start(unsigned int hart, uint64_t target, uint64_t frame_paddr);
int hart_stop(unsigned int hart);
void hart_handle_msip(unsigned int hart);
