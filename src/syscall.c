#include <syscall.h>
#include <syscodes.h>
#include <csr.h>
#include <utils.h>
#include <printf.h>
#include <sbi.h>
#include <input.h>
#include <ring.h>
#include <uaccess.h>
#include <kmalloc.h>
#include <sched.h>

#include <regions.h>
#include <main.h>
#include <symbols.h>
#include <process.h>

// handle a system call from U-mode to S-mode
void syscall_handle(void)
{
   uint64_t *sscratch;
   CSR_READ(sscratch, "sscratch");

   // return to the instruction *after* the ecall. Don't want to be
   // hearing about the same request again...
   uint64_t sepc;
   CSR_READ(sepc, "sepc");
   CSR_WRITE("sepc", sepc+4);
   int hart = sbi_who_am_i();
   process_t *p = process_current[hart];
   void *user_buffer;
   void *os_buffer;
   /* used for drawing circles and rectangles */
   gpu_rectangle_t gpu_r;
   gpu_circle_t gpu_c;
   pixel_t gpu_p;

   uint64_t syscall_num = sscratch[XREG_A7];
   switch(syscall_num)
   {
   case SYSCALL_EXIT:
      p->process_state = PS_ASYNC_FREE;
      sched_invoke(hart);
      break;
   case SYSCALL_PUTCHAR:
      sbi_putchar(sscratch[XREG_A0]);
      break;
   case SYSCALL_GETCHAR:
      printf("getchar unimplemented\n");
      break;
   case SYSCALL_CONSOLE:
      CSR_WRITE("sepc", console);
      CSR_WRITE("stvec", os_trap_vector);
      CSR_WRITE("sstatus", SSTATUS_FS_INITIAL     |
                           SSTATUS_SPP_SUPERVISOR |
                           SSTATUS_SPIE);
      uint64_t sscratch_addr = (uint64_t)sscratch;
      uint64_t *satp_addr = (uint64_t*)(sscratch_addr+536); // see asm/spawn.S
      uint64_t os_satp;
      CSR_READ(os_satp, "satp");
      *satp_addr = os_satp;
      sscratch[XREG_SP] = (uint64_t) sym_end(stack);
      sscratch[XREG_G ] = ((unsigned long)&__global_pointer$);
      break;
   case SYSCALL_SLEEP:
      p->process_state = PS_SLEEPING;
      uint64_t curtime = sbi_get_clint_time();
      uint64_t quantums = sscratch[XREG_A0];
      p->sleep_until = curtime + quantums * PROCESS_DEFAULT_CONTEXT_TIMER;
      /* when the process spawns again, spawn it after that sleep call */
      process_current[hart]->process_frame.sepc = sepc+4;
      sched_invoke(hart);
      break;
   case SYSCALL_GET_INPUT:
      ;
      uint32_t len, size;
      user_buffer = (void*)sscratch[XREG_A0];
      len = sscratch[XREG_A1];
      size = len*sizeof(input_event_t);
      /* now get the events */
      os_buffer = kmalloc(size);
      mutex_wait(&ie_ring_mutex);
      unsigned int i;
      for(i = 0; i < len; i++)
      {
         ((input_event_t *)(os_buffer))[i] =
                        ie_ring_buffer_pop(global_input_events_ring_buffer);
      }
      mutex_post(&ie_ring_mutex);

      /* and copy them to user */
      copy_to_user(os_buffer, user_buffer, size, p);

      kfree(os_buffer);
      break;
   case SYSCALL_GET_REGION:
      ;
      uint32_t x_raw, y_raw;
      x_raw = sscratch[XREG_A0];
      y_raw = sscratch[XREG_A1];
      uint32_t x_pct, y_pct;
      x_pct = x_raw * 100 / 0x8000;
      y_pct = y_raw * 100 / 0x8000;
      int region;
      region = regions_value_of_point(global_regions_start, x_pct, y_pct);
      // p->process_frame.gpregs[XREG_A0] = region;
      sscratch[XREG_A0] = region;
      break;
   case SYSCALL_ADD_REGION:
      ;
      uint32_t x_start, x_end;
      uint32_t y_start, y_end;
      int value;
      x_start = sscratch[XREG_A0];
      x_end   = sscratch[XREG_A1];
      y_start = sscratch[XREG_A2];
      y_end   = sscratch[XREG_A3];
      value   = sscratch[XREG_A4];
      region_llist_t *region_node;
      region_node = regions_new(x_start, x_end, y_start, y_end, value);
      if(regions_prepend(global_regions_start, region_node))
      {
         ERRORMSG("");
      }
      break;
   case SYSCALL_DRAW_RECTANGLE:
      ;
      user_buffer = (void*)sscratch[XREG_A0];
      copy_to_os((void*)&gpu_r, user_buffer, sizeof(gpu_rectangle_t), p);

      user_buffer = (void*)sscratch[XREG_A1];
      copy_to_os((void*)&gpu_p, user_buffer, sizeof(pixel_t), p);

      gpu_fill_rectangle(&(gpu_device.fb), &gpu_r, gpu_p);
      gpu_transfer_and_flush_some(&gpu_r);
      break;
   case SYSCALL_DRAW_CIRCLE:
      ;
      user_buffer = (void*)sscratch[XREG_A0];
      copy_to_os((void*)&gpu_c, user_buffer, sizeof(gpu_circle_t), p);

      user_buffer = (void*)sscratch[XREG_A1];
      copy_to_os((void*)&gpu_p, user_buffer, sizeof(pixel_t), p);

      gpu_fill_circle(&(gpu_device.fb), &gpu_c, gpu_p);
      gpu_circle_to_rectangle(&gpu_r, &gpu_c);
      gpu_transfer_and_flush_some(&gpu_r);
      break;
   case SYSCALL_GET_SCREEN_X:
      sscratch[XREG_A0] = gpu_device.fb.width;
      break;
   case SYSCALL_GET_SCREEN_Y:
      sscratch[XREG_A0] = gpu_device.fb.height;
      break;
   default:
      ERRORMSG("unknown syscall");
      printf("syscall number: %d\n", syscall_num);
      break;
   }
   return;
}
