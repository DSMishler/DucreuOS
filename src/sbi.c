#include <sbi.h>

void sbi_putchar(char c)
{
   // move system call number into a7, put the parameter in a0
   // no output (nothing between colons), inputs of SBI_PUTCHAr reg and c reg.
   // finally, clobbers (registers used)
   asm volatile("mv a7, %0\nmv a0, %1\necall" :: "r"(SVRCALL_PUTCHAR), "r"(c) :
               "a7", "a0");
}

char sbi_getchar(void)
{
   char c;
   asm volatile ("mv a7, %1\necall\nmv %0, a0\n" : "=r"(c) : "r"(SVRCALL_GETCHAR)
                : "a7", "a0");
   return c;
}

void sbi_poweroff(void)
{
   asm volatile("mv a7, %0\necall" : : "r"(SVRCALL_POWEROFF) : "a7");
}


int sbi_get_hart_status(unsigned int hart)
{
   int status;
   asm volatile("mv a7, %1\nmv a0, %2\necall\nmv %0, a0\n" : "=r"(status) :
                  "r"(SVRCALL_GET_HART_STATUS), "r"(hart) : "a7", "a0");
   return status;
}

int sbi_hart_start(unsigned int hart, uint64_t address, uint64_t frame)
{
   int status;
   asm volatile("mv a7, %1\nmv a0, %2\nmv a1, %3\nmv a2, %4\necall\nmv %0, a0\n"
               : "=r"(status)
               : "r"(SVRCALL_HART_START), "r"(hart), "r"(address), "r"(frame)
               : "a7", "a0", "a1", "a2");
   return status;
}

int sbi_hart_stop(unsigned int hart)
{
   int status;
   asm volatile("mv a7, %1\nmv a0, %2\necall\nmv %0, a0\n" : "=r"(status) :
                  "r"(SVRCALL_HART_STOP), "r"(hart) : "a7", "a0");
   return status;
}

int sbi_who_am_i(void)
{
   int which_hart;
   asm volatile("mv a7, %1\necall\nmv %0, a0" : "=r"(which_hart) :
                  "r"(SVRCALL_WHO_AM_I) : "a7", "a0");
   return which_hart;
}

uint64_t sbi_get_clint_time(void)
{
   uint64_t time;
   asm volatile("mv a7, %1\necall\nmv %0, a0" : "=r"(time) :
                  "r"(SVRCALL_GET_CLINT_TIME) : "a7", "a0");
   return time;
}

void sbi_set_clint_timer(int hart, uint64_t set)
{
   asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\necall" : :
                  "r"(SVRCALL_CLINT_SET_TIME), "r"(hart), "r"(set) :
                  "a7", "a0", "a1");
   return;
}

void sbi_add_clint_timer(int hart, uint64_t add)
{
   asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\necall" : :
                  "r"(SVRCALL_CLINT_ADD_TIME), "r"(hart), "r"(add) :
                  "a7", "a0", "a1");
   return;
}

// acknowledge the clint timer is machine-wide? No, the registers are per-hart.
void sbi_acknowledge_clint_timer()
{
   asm volatile("mv a7, %0\necall" : :
                "r"(SVRCALL_CLINT_ACKNOWLEDGE) : "a7");
   return;
}

void sbi_show_clint_timer(int hart)
{
   asm volatile("mv a7, %0\nmv a0, %1\necall" : :
                  "r"(SVRCALL_CLINT_SHOW_CMP), "r"(hart) :
                  "a7", "a0");
   return;
}
