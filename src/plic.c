#include <plic.h>
#include <pcie.h>
#include <printf.h>
#include <utils.h>

void plic_set_priority(int interrupt_id, char priority)
{
    uint32_t *base = (uint32_t *)PLIC_PRIORITY(interrupt_id);
    *base = priority & 0x7;
}
void plic_set_threshold(int hart, char priority)
{
    uint32_t *base = (uint32_t *)PLIC_THRESHOLD(hart, PLIC_MODE_SUPERVISOR);
    *base = priority & 0x7;
}
void plic_enable(int hart, int interrupt_id)
{
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_SUPERVISOR);
    base[interrupt_id / 32] |= 1UL << (interrupt_id % 32);
}
void plic_disable(int hart, int interrupt_id)
{
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_SUPERVISOR);
    base[interrupt_id / 32] &= ~(1UL << (interrupt_id % 32));
}
uint32_t plic_claim(int hart)
{
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_SUPERVISOR);
    return *base;
}
void plic_complete(int hart, int id)
{
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_SUPERVISOR);
    *base = id;
}

void plic_handle_irq(int hart)
{
   int irq = plic_claim(hart);
   switch(irq)
   {
   case 0: // means that we have handled every interrupt
      ERRORMSG("plic claim irq of 0"); 
      break;
   case PLIC_INT_CODE_IRQ0: // ISO C doesn't support [[fallthrough]] :/
   case PLIC_INT_CODE_IRQ1: // fallthrough
   case PLIC_INT_CODE_IRQ2: // fallthrough
   case PLIC_INT_CODE_IRQ3:
      pcie_handle_irq(irq);
      // did you notice that perhaps there is a race condition if we get
      // interrupted here right before completing the interrupt.
      // TODO: with this in mind, should we call plic_complete first?
      break;
   default:
      ERRORMSG("plic received interrupt it could not handle");
      break;
   }
   plic_complete(hart, irq);
}

void plic_init(int hart)
{
   // PCIE
   plic_enable(hart, PLIC_INT_CODE_IRQ0);
   plic_enable(hart, PLIC_INT_CODE_IRQ1);
   plic_enable(hart, PLIC_INT_CODE_IRQ2);
   plic_enable(hart, PLIC_INT_CODE_IRQ3);

   plic_set_priority(PLIC_INT_CODE_IRQ0, 6);
   plic_set_priority(PLIC_INT_CODE_IRQ1, 6);
   plic_set_priority(PLIC_INT_CODE_IRQ2, 6);
   plic_set_priority(PLIC_INT_CODE_IRQ3, 6);

   plic_set_threshold(hart, 0);
}
