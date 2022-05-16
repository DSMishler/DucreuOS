#include <csr.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>
#include <svrcall.h>
#include <clint.h>
#include <hart.h>

struct TrapFrame
{
   long gpregs[32];
   long fpregs[32];
   unsigned long pc;
};

struct TrapFrame c_sbi_trap_frame;

void c_trap_handler(void)
{
   unsigned long mcause, mhartid, *mscratch, mepc;
   CSR_READ(mcause,   "mcause");
   CSR_READ(mhartid,  "mhartid");
   CSR_READ(mscratch, "mscratch");
   CSR_READ(mepc,     "mepc");


   // check for external interrupts by `>> 63 & 1`
   int is_async = (mcause >> 63) & 1;
   mcause &= 0xff; // remove all but the 8 rightmost bits.
   if(is_async)
   {
      switch (mcause)
      {
      case 3: //MSIP (timer)
         // printf("SBI: software interrupt received (MSIP)!\n");
         /* mhartid now. It was 1 for some odd reason last I checked */
         hart_handle_msip(mhartid);
         break;
      case 7: // MTIP (timer)
         // printf("SBI: timer interrupt received (MTIP)!\n");
         // let the OS deal with it
         CSR_WRITE("mip", SIP_STIP);
         clint_set_mtimecmp(mhartid, CLINT_MTIMECMP_INFINITE);
         break;
      case 11: // 11: external interrupt
         plic_handle_irq(mhartid);
         break;
      default:
         printf("[%s]: Unhandled async interrupt %ld\n", __FILE__, mcause);
         break;
      }
   }
   else
   {
      switch (mcause)
      {
      case 9: // 9: OS call to SBI (uart)
         // (an Ecall from S mode)
         svrcall_handle(mhartid);
         // printf("System call from S mode: %ld at 0x%08lx\n", mscratch[17], mepc);
         // update mepc to contain the *next* instruction instead of returning
         // to the ecall instruction.
         CSR_WRITE("mepc", mepc + 4);
         break;
      default:
         printf("[%s]: Unhandled sync interrupt %ld\n", __FILE__, mcause);
         break;
      }
   }

}
