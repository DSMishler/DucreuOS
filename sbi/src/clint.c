#include <clint.h>
#include <hart.h>
#include <stdint.h>
#include <printf.h>

void clint_set_msip(int hart)
{
   if(hart >= MAX_SUPPORTED_HARTS)
   {
      return;
   }

   uint32_t *clint = (uint32_t *)CLINT_BASE_ADDRESS;
   clint[hart] = 1;
}

void clint_clear_msip(int hart)
{
   if(hart >= MAX_SUPPORTED_HARTS)
   {
      return;
   }

   uint32_t *clint = (uint32_t *)CLINT_BASE_ADDRESS;
   clint[hart] = 0;
}

uint64_t clint_get_time()
{
   uint64_t tm;
   asm volatile("rdtime %0" : "=r"(tm));
   return tm;
}

void clint_set_mtimecmp(int hart, uint64_t val)
{
   if(hart >= MAX_SUPPORTED_HARTS)
   {
      return;
   }

   uint64_t *ccmp = (uint64_t *)(CLINT_BASE_ADDRESS+CLINT_MTIMECMP_OFFSET);
   ccmp[hart] = val;
}

void clint_add_mtimecmp(int hart, uint64_t val)
{
   // extra check may not be necessary.
   if(hart >= MAX_SUPPORTED_HARTS)
   {
      return;
   }

   clint_set_mtimecmp(hart, clint_get_time() + val);
}

void clint_print_mtimecmp(int hart)
{
   if(hart >= MAX_SUPPORTED_HARTS)
   {
      return;
   }

   uint64_t *ccmp = (uint64_t *)(CLINT_BASE_ADDRESS+CLINT_MTIMECMP_OFFSET);
   printf("time on hart %d: 0x%016lx (%lu)", hart, ccmp[hart], ccmp[hart]);
}
