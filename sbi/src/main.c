#include <uart.h>
#include <plic.h>
#include <csr.h>
#include <lock.h>
#include <printf.h>
#include <hart.h>
#include <svrcall.h>

// found in sbi/asm/clearbss.S
void clear_bss();

// found in sbi/asm/trap.S
void sbi_trap_vector();

// found in sbi/asm/start.S
void sbi_hart_hang();

void puts(char *s)
{
   while(*s)
   {
      uart_put(*s);
      s++;
   }
   return;
}

static void pmp_init()
{
   // Largest address is 0xffff_ffff (pmpaddr0)
   // bottom of range is 0x0000_0000
   CSR_WRITE("pmpaddr0", 0xffffffff >> 2);
   // why << 2? The last two bits are removed from these registers, so it really
   // is the full range.

   //                   (whole range) (XWR bits (execute, write, read) )
   CSR_WRITE("pmpcfg0", (0b1 << 3) | (0b1 << 2) | (0b1 << 1) | (0b1 << 0));
}

long int SBI_GPREGS[8][32]; // TODO: max-hart-ify
mutex_t hart_lock = MUTEX_LOCKED;
extern hart_data_t sbi_hart_data[8];

__attribute__((naked,noreturn))
void main(int hart)
{
   if (hart != 0)
   {
      mutex_wait(&hart_lock);
      printf("hart %d now begins boot\n", hart);
      mutex_post(&hart_lock);
      sbi_hart_data[hart].status = HS_STOPPED;
      sbi_hart_data[hart].target_address = 0;
      sbi_hart_data[hart].process_frame = 0xFFFFFFFFFFFFFFFFUL;


      // uart_init(); // Testing it out for now. TODO: take me out
      pmp_init();  // all harts must set up their PMP

      CSR_WRITE("mscratch", &(SBI_GPREGS[hart][0]));
      CSR_WRITE("sscratch", hart);
      CSR_WRITE("mepc", sbi_hart_hang);
      CSR_WRITE("mtvec", sbi_trap_vector);
      CSR_WRITE("mideleg", 0); // additional harts do not handle interrupts
      CSR_WRITE("medeleg", 0); // additional harts do not handle interrupts
      // why supervisor mode? Virtual Memory
      CSR_WRITE("mstatus", MSTATUS_FS_INITIAL |
                           MSTATUS_MPP_MACHINE |
                           MSTATUS_MPIE);
      CSR_WRITE("mie", MIE_MSIE); // enable the hart to be woken up by the CLINT
      asm volatile ("mret"); // set these values and go to hart hang

   }

   // hart 0 only
   clear_bss();
   uart_init();
   plic_init(hart);

   // don't take for granted that HS_INVALID is zero
   // all harts invalid until they boot.
   int i;
   for(i = 0; i < MAX_SUPPORTED_HARTS; i++)
   {
      sbi_hart_data[i].status = HS_INVALID;
   }

   // now every other hart is able to run. Business taken care of.
   printf("hart %d now begins boot\n", hart);
   mutex_post(&hart_lock);

   pmp_init(); 

   // recall this code for bootstrap hart only
   sbi_hart_data[hart].status = HS_STARTED;
   sbi_hart_data[hart].target_address = 0;
   sbi_hart_data[hart].process_frame = 0xFFFFFFFFFFFFFFFFUL;


   // store trap frame into mscratch
   CSR_WRITE("mscratch", &(SBI_GPREGS[hart][0]));
   // also provide hart number to OS
   CSR_WRITE("sscratch", hart);

   CSR_WRITE("mepc", OS_LOAD_ADDR); // load in where to jump for the mret
   // Delegate async interrupts to supervisor mode.
   CSR_WRITE("mtvec", sbi_trap_vector);
   // 3, 7, 11 bits for Machine interrupts. This includes the timer!
   CSR_WRITE("mie", MIE_MEIE | MIE_MSIE | MIE_MTIE);
   // TODO: before we had ls 1, 5, and 7. Now we have 1, 5, and 9. Are the
   // macros wrong, or were we wrong to start out with? What do these bits do?
   CSR_WRITE("mideleg", MIP_SEIP | MIP_SSIP | MIP_STIP);
   // delegate everything that we can, except ecall in S mode.
   // TODO: Why is the macro in the inclass csr.h different from 0xB1FF?
   CSR_WRITE("medeleg", MEDELEG_ALL);
   CSR_WRITE("mstatus", MSTATUS_FS_INITIAL     |
                        MSTATUS_MPP_SUPERVISOR |
                        MSTATUS_MPIE);

   // TODO: print this message only when all harts have done all code. Print
   // this, then mret. This would require a variable length barrier.
   // technically, this is a race condition if a hart has not finished
   // setting up before it is started!
   // One way to implement is to check that all the HARTS have gone from
   // invalid to stopped!
   mutex_wait(&hart_lock);
   puts("Hello World! Daniel's SBI booted successfully.\n\n");
   mutex_post(&hart_lock);
   asm volatile("mret");
}
