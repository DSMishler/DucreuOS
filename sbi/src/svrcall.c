#include <svrcall.h>
#include <csr.h>
#include <uart.h>
#include <printf.h>
#include <hart.h>
#include <clint.h>
#include <stdint.h>

void svrcall_handle(int hart)
{
   uint64_t *mscratch;
   CSR_READ(mscratch, "mscratch");
   uint64_t svrcall_num = mscratch[XREG_A7];
   switch (svrcall_num)
   {
   case SVRCALL_PUTCHAR:
      //putc
      uart_put(mscratch[XREG_A0]);
      break;
   case SVRCALL_GETCHAR:
      //getc
      // XREG_A0 stores the return value
      mscratch[XREG_A0] =  uart_buffer_pop();
      break;
   case SVRCALL_POWEROFF:
      // imma be real with ya chief, I got this code from the inclass git
      *((unsigned short *)0x100000) = 0x5555;
   break;
   case SVRCALL_GET_HART_STATUS:
      mscratch[XREG_A0] = get_hart_status(mscratch[XREG_A0]);
      break;
   case SVRCALL_HART_START:
      mscratch[XREG_A0] = hart_start(mscratch[XREG_A0],
                                     mscratch[XREG_A1],
                                     mscratch[XREG_A2]);
      break;
   case SVRCALL_HART_STOP:
      mscratch[XREG_A0] = hart_stop(mscratch[XREG_A0]);
      break;
   case SVRCALL_WHO_AM_I:
      mscratch[XREG_A0] = hart;
      break;
   case SVRCALL_GET_CLINT_TIME:
      mscratch[XREG_A0] = clint_get_time();
      break;
   case SVRCALL_CLINT_SET_TIME:
      clint_set_mtimecmp(mscratch[XREG_A0], mscratch[XREG_A1]);
      break;
   case SVRCALL_CLINT_ADD_TIME:
      clint_add_mtimecmp(mscratch[XREG_A0], mscratch[XREG_A1]);
      break;
   case SVRCALL_CLINT_ACKNOWLEDGE:
      ; // can't start with delcarations, so we drop a no-op
      uint64_t mip;
      CSR_READ(mip, "mip");
      CSR_WRITE("mip", mip & ~SIP_STIP);
      // remove the interrupt for OS. The SBI will turn it back on when we
      // need it.
      // Recall SIP is the software interrupt pending register
      // It is a restricted view of mip.
      break;
   case SVRCALL_CLINT_SHOW_CMP:
      clint_print_mtimecmp(mscratch[XREG_A0]);
      break;
   default:
      printf("Unknown supervisor call %d on hart %d\n", svrcall_num, hart);
      break;
   }
}
