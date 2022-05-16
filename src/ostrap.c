#include <ostrap.h>
#include <csr.h>
#include <stdint.h>
#include <printf.h>
#include <plic.h>
#include <sbi.h>
#include <syscall.h>
#include <process.h>
#include <sched.h>



void os_trap_handler(void)
{
   uint64_t scause;
   // note: we are reading scause after entering the function. Getting the
   // cause then seems to be pre-emptible. Would it be better to have
   // the cause stored in the trap vector first and passed via assembly
   // as a0?
   // Answer: it won't happen! trap mode is this OS is not pre-emptible,
   // meaning we can't recursively trap. Therefore, there is no issue here.
   CSR_READ(scause, "scause");
   // printf("ctrap activated. scause = 0x%016lx\n", scause);

   int is_async = (scause >> 63) & 1;
   scause &= 0xffUL; /* interrupt codes are 8-bit */

   if(is_async)
   {
      switch(scause)
      {
      case 0x05: //STIP
         sbi_acknowledge_clint_timer();
         // note: the process may have exited due to a timer interrupt.
         // If this is true, let's make sure the process picks up where it
         // left off, rather than at the beginning.
         uint64_t sepc;
         CSR_READ(sepc, "sepc");
         int hart = sbi_who_am_i();
         process_current[hart]->process_frame.sepc = sepc;
         sched_invoke(hart);
         // printf("STIP received on hart %d: running a print", sbi_who_am_i());
         // process_top();
         break;
      case 0x09:
         plic_handle_irq(sbi_who_am_i());
         break;
      default:
         printf("OS trap handler. unhandled async interrupt with ");
         printf("scause = 0x%016lx\n", scause);
         break;
      }
   }
   else
   {
      switch(scause)
      {
      case 0x08: //E-call from U mode
         syscall_handle();
         break;
      default:
         printf("OS trap handler. unhandled synchronous exception with ");
         printf("scause = 0x%016lx\n", scause);
         asm volatile("j .");
         break;
      }
   }
}
