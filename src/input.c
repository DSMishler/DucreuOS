#include <input.h>
#include <virtio.h>
#include <pcie.h>
#include <utils.h>
#include <printf.h>
#include <lock.h>
#include <kmalloc.h>
#include <mmu.h>
#include <page.h>
#include <ring.h>


input_device_t *input_devices;
int num_input_devices = 0;
ie_ring_buffer_t *global_input_events_ring_buffer;
extern PCIe_bar_list_t global_pcie_bars_occupied;


#define GPBO global_pcie_bars_occupied
#define VRO  virtio_register_offsets
int virtio_pcie_input_driver(PCIe_driver_t *dev)
{
   // DEBUGMSG("input driver");
   input_device_t input_device;
   int i;
   // TODO: replace this hard-coded 16 with a better way to talk/walk BARS
   for(i = 15; i >= 0; i--)
   {
      if(global_pcie_bars_occupied.vendorIDs[i] == dev->vendor_id &&
         global_pcie_bars_occupied.deviceIDs[i] == dev->device_id)
      {
         input_device.common_config = (virtio_pci_common_cfg_t *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_COMMON]);
         input_device.notify        = (virtio_pci_notify_cfg_t *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_NOTIFY]);
         input_device.isr           = (virtio_pci_isr_t        *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_ISR]);
         input_device.input_specific= (virtio_input_config_t   *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_DEVICE]);
         input_device.which_irq = GPBO.which_irq[i];
         break;
      }
   }
   if(i == -1)
   {
      ERRORMSG("could not find input BAR");
   }
   // DEBUGMSG("input device BARS set");

   // 1: reset device
   input_device.common_config->device_status = VIRTIO_DEV_STATUS_RESET;

   // 2: acknowledge the device
   input_device.common_config->device_status = VIRTIO_DEV_STATUS_ACKNOWLEDGE;

   // 3: acknowledge we have a driver
   input_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER;

   // 4: read features (negotiate if needed)
   // looks like it's 64 by default. Let's just ensure that's the case
   input_device.common_config->queue_size = 64;
   if(input_device.common_config->queue_size != 64)
   {
      ERRORMSG("input device not a nice negotiator");
      return 3;
   }

   // 5: features are okay
   input_device.common_config->device_status |= VIRTIO_DEV_STATUS_FEATURES_OK;

   // 6: ask the device if it agrees
   if(input_device.common_config->device_status & VIRTIO_DEV_STATUS_FEATURES_OK)
   {
      ;
   }
   else
   {
      ERRORMSG("input device not happy with requested features");
      return 2;
   }

   // 7: device specific setup
   input_device.common_config->queue_select = 0;
   uint16_t queue_size = input_device.common_config->queue_size;
   uint64_t vaddr, paddr;
   virtq_t *vq;
   vq = kmalloc(sizeof(virtq_t));

   // descriptor table
   vaddr = (uint64_t)kcalloc(sizeof(virtq_desc_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->descriptor = (virtq_desc_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   input_device.common_config->queue_desc = paddr;

   // driver ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*(queue_size+3));
   MALLOC_CHECK(vaddr);
   vq->driver = (virtq_driver_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   input_device.common_config->queue_driver = paddr;

   // device ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*3 +
                             sizeof(virtq_used_element_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->device = (virtq_device_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   input_device.common_config->queue_device = paddr;

   input_device.common_config->queue_enable = 1;

   // 8: device is live!
   input_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER_OK;
   input_device.vq = vq;
   input_device.requests_idx = 0;
   input_device.acknowledged_idx = 0;
   input_device.enabled = 1;



   // now the input device requires a buffer to store its event info in
   // this is our very first and only request to the device: to write
   // interrupts in these areas
   int num_pages_input_events;
   int bytes_input_events = queue_size * sizeof(input_event_t);
   num_pages_input_events = (bytes_input_events + PAGE_SIZE - 1) / PAGE_SIZE;
   input_event_t *input_events_buffer;
   input_events_buffer = (input_event_t *)page_znalloc(num_pages_input_events);

   
   // for the global buffer, we don't know how to handle disjoint events yet.
   // As a result, we throw errors if the buffer is ever full
   if(global_input_events_ring_buffer == NULL)
   {
      global_input_events_ring_buffer =
          ie_ring_buffer_init(INPUT_RB_SIZE, RB_ASK_FOR_MANAGER);
   }

   // now add the input device to the global array of devices
   int num_old_devices = num_input_devices;
   num_input_devices += 1;
   input_device_t *temp_idp;
   temp_idp = kmalloc(num_input_devices*sizeof(input_device_t));
   MALLOC_CHECK(temp_idp);

   memcpy(temp_idp, input_devices, num_old_devices*sizeof(input_device_t));
   memcpy(temp_idp+num_old_devices, &input_device, sizeof(input_device_t));

   kfree(input_devices);
   input_devices = temp_idp;

   // now we say: bring it on, interrupts
   char input_irq_note[40];
   sprintf(input_irq_note, "input device %d", num_old_devices);
   pcie_register_irq(input_device.which_irq, input_irq_handler, input_irq_note);

   // uint64_t paddr, vaddr; already declared from earlier;
   uint16_t driver_idx;
   for(i = 0; i < queue_size; i++)
   {
      vaddr = (uint64_t)(input_events_buffer + i);
      paddr = mmu_translate(kernel_mmu_table, vaddr);
      input_device.vq->descriptor[i].address = paddr;
      input_device.vq->descriptor[i].length  = sizeof(input_event_t);
      input_device.vq->descriptor[i].flags   = VIRTQ_DESC_FLAG_WRITE;
      input_device.vq->descriptor[i].next    = 0;

      driver_idx = input_device.vq->driver->idx % queue_size;
      input_device.vq->driver->ring[driver_idx] = i;
      input_device.vq->driver->idx += 1;
      input_device.requests_idx += 1;
   }
   // note: we don't even ding qnotify
   // office (solved) hours moment: it's because the input device doesn't
   // require us to (in fact, the features bits tell us that it won't listen)
   

   // virtio_print_common_cfg(input_device.common_config);
   // input_print_device_specific(input_device.input_specific);
   return 0;
}


void input_irq_handler(void)
{
   int i;
   /*
    * don't want two different interrupts pushed at the same time *
    * this mutex may not be necessary, though, until we delegate handling *
    * of this interrupt to multiple harts
    */
   // printf("input interrupt handling by hart %d\n", sbi_who_am_i());
   mutex_wait(&ie_ring_mutex);
   for(i = 0; i < num_input_devices; i++)
   {
      if(input_devices[i].isr->queue_interrupt)
      {
         // DEBUGMSG("input IRQ confirmed");
         uint16_t desc_idx, queue_size;
         uint64_t event_paddr;
         input_event_t event;
         queue_size = input_devices[i].common_config->queue_size;
         while(input_devices[i].acknowledged_idx != input_devices[i].vq->device->idx)
         {
            static int input_interrupts = 0;
            input_interrupts += 1;
            desc_idx = input_devices[i].acknowledged_idx % queue_size;
            event_paddr = input_devices[i].vq->descriptor[desc_idx].address;
            event = *((input_event_t*)event_paddr);
            input_devices[i].acknowledged_idx += 1;
            ie_ring_buffer_push(global_input_events_ring_buffer, event);
            // input_print_event(&event);
            // input_print_pixel(&event);
   
            // now allow more space in the driver ring.
            input_devices[i].vq->driver->idx  += 1;
            input_devices[i].requests_idx     += 1;
         }
      }
   }
   mutex_post(&ie_ring_mutex);
   return;
}

void input_print_pixel(input_event_t *iev)
{
   if(iev->type == IE_TYPE_TABLET)
   {
      if(iev->code == IE_TABLET_CODE_X)
      {
         printf("x pixel: %u%c\n", iev->value*100/0x8000, '%');
      }
      if(iev->code == IE_TABLET_CODE_Y)
      {
         printf("y pixel: %u%c\n", iev->value*100/0x8000, '%');
      }
   }
}

void input_print_event(input_event_t *iev)
{
   printf("input event at 0x%012lx\n", iev);
   printf("   type : 0x%04x\n", iev->type);
   printf("   code : 0x%04x\n", iev->code);
   printf("   value: 0x%08x\n", iev->value);
}

void input_print_device_specific(volatile virtio_input_config_t *icfg)
{
   printf("input device specific register at 0x%012lx\n",icfg);
   printf("select      : 0x%08x\n", icfg->select);
   printf("subselect   : 0x%08x\n", icfg->subsel);
   printf("size        : 0x%08x\n", icfg->size  );
   printf("union:\n");
   int i;
   // Might print differently depending on selection
   for(i = 0; i < 16; i++)
   {
      printf("   bitmap[%03d-%03d]: 0x%08x\n", i*8, i*8+7, icfg->bitmap[i]  );
   }
}

void input_print_virtq(void)
{
   int i;
   input_device_t input_device;
   for(i = 0; i < num_input_devices; i++)
   {
      input_device = input_devices[i];
      int queue_size = input_device.common_config->queue_size;
      virtio_print_virtq_all(input_device.vq, queue_size);
   }
}
