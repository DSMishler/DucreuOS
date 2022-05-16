#include <hart.h>
#include <lock.h>
#include <csr.h>
#include <printf.h>
#include <clint.h>

hart_data_t sbi_hart_data[8];
mutex_t sbi_hart_lock[8] = {
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED,
                              MUTEX_UNLOCKED
                           };
void sbi_hart_hang();

hart_status_t get_hart_status(unsigned int hart)
{
   if (hart >= MAX_SUPPORTED_HARTS)
   {
      return HS_INVALID;
   }
   /* else */
   return sbi_hart_data[hart].status;
}

int hart_start(unsigned int hart, uint64_t target, uint64_t frame_paddr)
{
   if(!(mutex_try(sbi_hart_lock+hart)))
   {
      // not only do we not have access, but this should really never happen.
      return 1;
   }
   if(sbi_hart_data[hart].status != HS_STOPPED)
   {
      // we only start stopped harts here
      mutex_post(sbi_hart_lock+hart);
      return 2;
   }
   sbi_hart_data[hart].status = HS_STARTING;
   sbi_hart_data[hart].target_address = target;
   sbi_hart_data[hart].process_frame = frame_paddr;
   clint_set_msip(hart);
   mutex_post(sbi_hart_lock+hart);
   return 0; // no news is good news
}

// TODO: what about stopping a hart that a process is on...?
// remember, the process frame isn't necessarily up to date.
int hart_stop(unsigned int hart)
{
   if(!(mutex_try(sbi_hart_lock + hart)))
   {
      return 1;
   }
   // TODO: add in the smip settings
   if(sbi_hart_data[hart].status != HS_STARTED)
   {
      // we only stop starting harts here
      mutex_post(sbi_hart_lock+hart);
      return 2;
   }

   sbi_hart_data[hart].status = HS_STOPPING;
   sbi_hart_data[hart].target_address = 0;
   sbi_hart_data[hart].process_frame = 0;
   clint_set_msip(hart); // the target hart needs to execute this code.
   mutex_post(sbi_hart_lock+hart);

   return 0;
}

void hart_handle_msip(unsigned int hart)
{
   mutex_wait(sbi_hart_lock+hart);
   clint_clear_msip(hart);
   printf("hart %d spawning a process\n", hart);

   if(sbi_hart_data[hart].status != HS_STARTING &&
      sbi_hart_data[hart].status != HS_STOPPING)
   {
      printf("[%s] ERROR: spawning/exiting on a non-transitioning hart\n",
             __FILE__);
      mutex_post(sbi_hart_lock+hart);
      return;
   }
   if(sbi_hart_data[hart].status == HS_STARTING)
   {
      CSR_WRITE("sscratch", sbi_hart_data[hart].process_frame);
      CSR_WRITE("mepc",     sbi_hart_data[hart].target_address);
      // no need to reset mtvec
      // office hours moment: why MIE_STIE
      CSR_WRITE("mie", MIE_MEIE | MIE_MSIE | MIE_MTIE | MIE_STIE);
      CSR_WRITE("mideleg", SIP_SEIP | SIP_SSIP | SIP_STIP);
      CSR_WRITE("medeleg", MEDELEG_ALL);
      // start spawn_ktread_start with supervisor mode.
      // The target mode is stored in frame
      CSR_WRITE("mstatus", MSTATUS_FS_INITIAL     |
                           MSTATUS_MPP_SUPERVISOR |
                           MSTATUS_MPIE);
      sbi_hart_data[hart].status = HS_STARTED;
      
      mutex_post(sbi_hart_lock+hart);
      asm volatile("mret");
   }
   if(sbi_hart_data[hart].status == HS_STOPPING)
   {
      // don't change mscratch
      CSR_WRITE("sscratch", 0);
      CSR_WRITE("stvec", 0);
      CSR_WRITE("sepc", 0);
      CSR_WRITE("satp", 0);
      CSR_WRITE("mepc", sbi_hart_hang);
      CSR_WRITE("medeleg", 0);
      CSR_WRITE("mideleg", 0);
      CSR_WRITE("mip", 0);
      CSR_WRITE("mstatus", MSTATUS_FS_INITIAL  |
                           MSTATUS_MPP_MACHINE |
                           MSTATUS_MPIE);
      uint64_t stopped_mie = MIE_MSIE;
      if(hart == 0)
      {
         stopped_mie |= MIE_MEIE; // Office hours moment: why?
      }
      CSR_WRITE("mie", stopped_mie);
      clint_set_mtimecmp(hart, CLINT_MTIMECMP_INFINITE);
      sbi_hart_data[hart].status = HS_STOPPED;
      mutex_post(sbi_hart_lock+hart);
      asm volatile("mret");
   }
}
