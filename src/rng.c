#include <rng.h>
#include <virtio.h>
#include <pcie.h>
#include <kmalloc.h>
#include <mmu.h>
#include <utils.h>

#include <printf.h>

rng_device_t rng_device;
extern PCIe_bar_list_t global_pcie_bars_occupied;

#define GPBO global_pcie_bars_occupied
#define VRO  virtio_register_offsets
int virtio_pcie_rng_driver(PCIe_driver_t *dev)
{
   // find the device in the BARS struct
   int i;
   for(i = 15; i >= 0; i--)
   {
      // TODO: will this strategy work for mouse + tablet? Seems like it
      // might not, since they have same vendor and device ID but are different
      // devices.
      if(global_pcie_bars_occupied.vendorIDs[i] == dev->vendor_id &&
         global_pcie_bars_occupied.deviceIDs[i] == dev->device_id )
      {
         // TODO: programmatically determine offsets
         rng_device.common_config = (virtio_pci_common_cfg_t *)(uint64_t)
              (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_COMMON]);
         rng_device.notify        = (virtio_pci_notify_cfg_t *)(uint64_t)
              (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_NOTIFY]);
         rng_device.isr           = (virtio_pci_isr_t *)       (uint64_t)
              (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_ISR   ]);
         rng_device.which_irq     = global_pcie_bars_occupied.which_irq[i];
         break;
      }
   }
   if(i == -1)
   {
      ERRORMSG("Couldn't find the rng device BAR!");
      return 1;
   }
   // 1: reset the device
   rng_device.common_config->device_status = VIRTIO_DEV_STATUS_RESET;

   // 2: acknowledge the device
   rng_device.common_config->device_status = VIRTIO_DEV_STATUS_ACKNOWLEDGE;

   // 3: we know how to drive the device
   rng_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER;

   // 4: no features to read
   ;

   // 5: there were no features to read so I guess the features are OK
   rng_device.common_config->device_status |= VIRTIO_DEV_STATUS_FEATURES_OK;

   // 6: double check
   if(rng_device.common_config->device_status & VIRTIO_DEV_STATUS_FEATURES_OK)
   {
      ; // good to go
   }
   else
   {
      printf("oh no, the RNG device isn't playing nice. It didn't accept\n");
      printf("the features we requested. We didn't even request features!\n");
      return 2;
   }

   // 7: device specific setup
   // setup queue 0 (the only queue)
   rng_device.common_config->queue_select = 0;
   uint16_t queue_size = rng_device.common_config->queue_size;
   uint64_t vaddr;
   virtq_t *vq;
   vq = kmalloc(sizeof(virtq_t));

   // descriptor table
   vaddr = (uint64_t)kcalloc(sizeof(virtq_desc_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->descriptor = (virtq_desc_t *)vaddr;
   rng_device.common_config->queue_desc = mmu_translate(kernel_mmu_table,vaddr);

   // driver (aka available) ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*(queue_size+3));
   MALLOC_CHECK(vaddr);
   vq->driver = (virtq_driver_t *)vaddr;
   rng_device.common_config->queue_driver=mmu_translate(kernel_mmu_table,vaddr);

   // device (aka used) ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*3+
                             sizeof(virtq_used_element_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->device = (virtq_device_t *)vaddr;
   rng_device.common_config->queue_device=mmu_translate(kernel_mmu_table,vaddr);


   rng_device.common_config->queue_enable = 1;     // queue live

   // 8: fire up that device!
   rng_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER_OK;
   rng_device.vq = vq;
   rng_device.at_idx = 0;
   rng_device.acknowledged_idx = 0;
   rng_device.enabled = 1;


   pcie_register_irq(rng_device.which_irq, rng_irq_handler, "rng");

   // no news is good news
   return 0;
}


int rng_fill_poll(void *buffer, int size)
{
   if(rng_device.enabled == 0)
   {
      // device isn't on yet.
      return 1;
   }
   if(size == 0)
   {
      // we don't do zero requests
      // apparently the device warns the OS about it
      // (Isaac's OS, didn't warn me)
      return 2;
   }
   uint64_t paddr;
   uint16_t queue_size;
   uint16_t at_idx; // just grabbing it for shorthand
   uint16_t driver_idx;
   paddr = mmu_translate(kernel_mmu_table, (uint64_t)buffer);
   // TODO: feel like exploring? I wonder if we can hot-swap the queue size.
   queue_size = rng_device.common_config->queue_size;
   at_idx = rng_device.at_idx % queue_size;

   rng_device.vq->descriptor[at_idx].address = paddr;
   rng_device.vq->descriptor[at_idx].length  = size;
   rng_device.vq->descriptor[at_idx].flags   = VIRTQ_DESC_FLAG_WRITE;
   rng_device.vq->descriptor[at_idx].next    = 0; // no next flag, no next.
   // place it in the driver ring
   driver_idx = rng_device.vq->driver->idx;
   // I just wanted to put this comment in this code to let you know
   // that I, the programmer, had the bug of driver_idx & queue_size.
   // Yes. I did.
   rng_device.vq->driver->ring[driver_idx % queue_size] = at_idx;
   // now the RNG device knows about the descriptor
   rng_device.vq->driver->idx += 1; // naturally loops at 65536
   rng_device.at_idx += 1;
   // virtio_print_virtq_all(rng_device.vq, queue_size);

   // office hours moment: this qnotify struct is *dead empty*
   // TODO: find it in the capabilities
   // printf("struct     0x%016lx\n", *(uint64_t*)&rng_device.notify->cap);
   // printf("struct     0x%016lx\n", *(((uint64_t*)&rng_device.notify->cap)+1));
   // printf("offset     0x%08x\n", rng_device.notify->cap.offset);
   // printf("multiplier 0x%08x\n", rng_device.notify->notify_off_multiplier);

   // now ding the qnotify
   // printf("notify     0x%08x\n", (rng_device.notify));
   uint32_t *qnotify = (uint32_t *)rng_device.notify;
   *qnotify = 0;

   // now wait for the request to complete
   uint16_t lookahead_idx = rng_device.acknowledged_idx;
   int request_complete = 0;
   while(!(request_complete))
   {
      if(lookahead_idx != rng_device.vq->device->idx)
      {
         if(rng_device.vq->device->ring[lookahead_idx % queue_size].id ==
                                                       at_idx)
         {
            // DEBUGMSG("request satisfied");
            request_complete = 1;
         }
         lookahead_idx += 1;
      }
   }


   return 0; // no news is good news
}

void rng_irq_handler(void)
{
   if(rng_device.isr->queue_interrupt) // and BAM, it's zero again.
   {
      /* check for yourself */
      // printf("rng_device.isr->queue_interrupt = %d\n", 
                // rng_device.isr->queue_interrupt);
      ; /* Nothing else needs to be done. Just needed to read the register. */
      rng_device.acknowledged_idx = rng_device.vq->device->idx;
   }
}


char rand8(void)
{
   char r;
   rng_fill_poll(&r, 1);
   return r;
}

void rng_print_virtq(void)
{
   int queue_size = rng_device.common_config->queue_size;
   virtio_print_virtq_all(rng_device.vq, queue_size);
}

