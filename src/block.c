#include <block.h>
#include <virtio.h>
#include <pcie.h>
#include <printf.h>
#include <utils.h>
#include <stdint.h>
#include <kmalloc.h>
#include <mmu.h>
#include <lock.h>

block_device_t *block_devices;
int num_block_devices = 0;
extern PCIe_bar_list_t global_pcie_bars_occupied;

#define GPBO global_pcie_bars_occupied
#define VRO  virtio_register_offsets
int virtio_pcie_block_driver(PCIe_driver_t *dev)
{
   block_device_t block_device;
   int i;
   for(i = 15; i >= 0; i--)
   {
      if(global_pcie_bars_occupied.vendorIDs[i] == dev->vendor_id &&
         global_pcie_bars_occupied.deviceIDs[i] == dev->device_id)
      {
         block_device.common_config  = (virtio_pci_common_cfg_t *)(uint64_t)
               (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_COMMON]);
         block_device.notify         = (virtio_pci_notify_cfg_t *)(uint64_t)
               (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_NOTIFY]);
         block_device.isr            = (virtio_pci_isr_t *)(uint64_t)
               (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_ISR]);
         block_device.block_specific = (virtio_block_config_t *)(uint64_t)
               (0x40000000+i*0x00100000+GPBO.VRO[i][VIRTIO_CAP_CFG_DEVICE]);
         block_device.which_irq = global_pcie_bars_occupied.which_irq[i];
         break; // found the driver
      }
   }
   if(i == -1)
   {
      ERRORMSG("could not find block BAR");
      return 1;
   }


   // 1: reset device
   block_device.common_config->device_status = VIRTIO_DEV_STATUS_RESET;

   // 2: acknowledge the device
   block_device.common_config->device_status = VIRTIO_DEV_STATUS_ACKNOWLEDGE;

   // 3: acknowledge we have a driver
   block_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER;

   // 4: read features (negotiate if needed)
   // looks like it's 256 by default. Let's just ensure that's the case
   block_device.common_config->queue_size = 256;
   if(block_device.common_config->queue_size != 256)
   {
      ERRORMSG("block device not a nice negotiator");
      return 3;
   }

   // 5: features are okay
   block_device.common_config->device_status |= VIRTIO_DEV_STATUS_FEATURES_OK;

   // 6: ask the device if it agrees
   if(block_device.common_config->device_status & VIRTIO_DEV_STATUS_FEATURES_OK)
   {
      ;
   }
   else
   {
      ERRORMSG("block device not happy with requested features");
      return 2;
   }
   
   // 7: device specific setup
   block_device.common_config->queue_select = 0;
   uint16_t queue_size = block_device.common_config->queue_size;
   uint64_t vaddr, paddr;
   virtq_t *vq;
   vq = kmalloc(sizeof(virtq_t));

   // descriptor table
   vaddr = (uint64_t)kcalloc(sizeof(virtq_desc_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->descriptor = (virtq_desc_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   block_device.common_config->queue_desc = paddr;

   // driver ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*(queue_size+3));
   MALLOC_CHECK(vaddr);
   vq->driver = (virtq_driver_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   block_device.common_config->queue_driver = paddr;

   // device ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*3 +
                             sizeof(virtq_used_element_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->device = (virtq_device_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   block_device.common_config->queue_device = paddr;

   block_device.common_config->queue_enable = 1;

   // 8: device is live!
   block_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER_OK;
   // Is this technically a race condition? Office (solved) hours moment:
   // No, it's not, because we will not cause any interrupts until the vq is
   // set.
   block_device.vq = vq;
   block_device.requests_idx = 0;
   block_device.acknowledged_idx = 0;
   block_device.enabled = 1;

   // now add the device to the global array of devices
   int num_old_devices = num_block_devices;
   num_block_devices += 1;
   block_device_t *temp_bdp;
   temp_bdp = kmalloc(num_block_devices * sizeof(block_device_t));
   MALLOC_CHECK(temp_bdp);

   memcpy(temp_bdp, block_devices, num_old_devices*sizeof(block_device_t));
   memcpy(temp_bdp+num_old_devices, &block_device, sizeof(block_device_t));

   kfree(block_devices);
   block_devices = temp_bdp;

   // ready for interrupts
   char block_irq_note[40];
   sprintf(block_irq_note, "block device %d", num_old_devices);
   pcie_register_irq(block_device.which_irq, block_irq_handler, block_irq_note);

   // virtio_print_common_cfg(block_device.common_config);
   // printf("block size: %d\n", block_device.block_specific->block_size);

   return 0; // no news is good news
}

int block_read( int dev, char *buffer, uint64_t start_byte, uint64_t end_byte)
{
   return block_read_write(dev, buffer, start_byte, end_byte, 0, 0);
}

int block_write(int dev, char *buffer, uint64_t start_byte, uint64_t end_byte)
{
   return block_read_write(dev, buffer, start_byte, end_byte, 1, 0);
}

int block_read_poll( int dev, char *buffer,
                     uint64_t start_byte, uint64_t end_byte)
{
   return block_read_write(dev, buffer, start_byte, end_byte, 0, 1);
}

int block_write_poll(int dev, char *buffer,
                     uint64_t start_byte, uint64_t end_byte)
{
   return block_read_write(dev, buffer, start_byte, end_byte, 1, 1);
}

// the block device is reading to us, the users
int block_read_write(int which_device, char *buffer,
                     uint64_t start_byte, uint64_t end_byte,
                     int write_request, int do_poll)
{
   if(which_device >= num_block_devices)
   {
      ERRORMSG("out of range device");
      return 4;
   }

   block_device_t *block_device_p = block_devices+which_device;
   int block_size = block_device_p->block_specific->block_size;
   int queue_size = block_device_p->common_config->queue_size;
   if(start_byte % block_size)
   {
      ERRORMSG("start bytes not aligned");
      return 1;
   }
   if(end_byte % block_size)
   {
      ERRORMSG("end bytes not aligned");
      return 1;
   }
   if(start_byte > end_byte)
   {
      ERRORMSG("start bytes > end bytes");
      return 2;
   }
   if(start_byte == end_byte)
   {
      ERRORMSG("empty request");
      return 3; // empty read
   }

   volatile block_request_t *brq;
   brq= (block_request_t *) kcalloc(sizeof(block_request_t));
   MALLOC_CHECK(brq);

   if(write_request == 1)
   {
      brq->brq1.type = VIRTIO_BLOCK_TYPE_OUT;
   }
   else
   {
      brq->brq1.type = VIRTIO_BLOCK_TYPE_IN;
   }
   brq->brq1.sector = start_byte/block_size;

   brq->brq2.data = buffer;

   brq->brq3.status = VIRTIO_BLOCK_STATUS_RANDOM;
   // write something random so that we won't get one of the other status
   // messages by accident

   uint16_t brq1_desc_idx, brq2_desc_idx, brq3_desc_idx;
   uint64_t paddr1, paddr2, paddr3;
   uint32_t len1, len2, len3;
   uint16_t flags1, flags2, flags3;
   uint16_t next1, next2, next3;
   uint16_t driver_idx;

   brq1_desc_idx = (block_device_p->requests_idx % queue_size);
   block_device_p->requests_idx += 1;
   brq2_desc_idx = (block_device_p->requests_idx % queue_size);
   block_device_p->requests_idx += 1;
   brq3_desc_idx = (block_device_p->requests_idx % queue_size);
   block_device_p->requests_idx += 1;

   paddr1 = mmu_translate(kernel_mmu_table, (uint64_t)&(brq->brq1));
   // address of the memory to copy to
   paddr2 = mmu_translate(kernel_mmu_table, (uint64_t)brq->brq2.data);
   paddr3 = mmu_translate(kernel_mmu_table, (uint64_t)&(brq->brq3));

   len1 = sizeof(block_request_desc_1_t);
   len2 = end_byte - start_byte;
   // office (solved) hours moment: how come I pass something that is divisible
   // by 512 and the block driver incremements it by 1?
   // because that extra 1 bit is for the block driver to write to the status
   // byte that we gave it (usually it write OK)
   len3 = sizeof(block_request_desc_3_t);

   if(write_request == 1)
   {
      flags1 = VIRTQ_DESC_FLAG_NEXT;
      flags2 = VIRTQ_DESC_FLAG_NEXT;
      flags3 = VIRTQ_DESC_FLAG_WRITE;
   }
   else // read request
   {
      flags1 = VIRTQ_DESC_FLAG_NEXT;
      flags2 = VIRTQ_DESC_FLAG_NEXT | VIRTQ_DESC_FLAG_WRITE;
      flags3 = VIRTQ_DESC_FLAG_WRITE;
   }

   next1 = brq2_desc_idx;
   next2 = brq3_desc_idx;
   next3 = 0; // not used since no next flag exists

   block_device_p->vq->descriptor[brq1_desc_idx].address = paddr1;
   block_device_p->vq->descriptor[brq1_desc_idx].length  = len1;
   block_device_p->vq->descriptor[brq1_desc_idx].flags   = flags1;
   block_device_p->vq->descriptor[brq1_desc_idx].next    = next1;

   block_device_p->vq->descriptor[brq2_desc_idx].address = paddr2;
   block_device_p->vq->descriptor[brq2_desc_idx].length  = len2;
   block_device_p->vq->descriptor[brq2_desc_idx].flags   = flags2;
   block_device_p->vq->descriptor[brq2_desc_idx].next    = next2;

   block_device_p->vq->descriptor[brq3_desc_idx].address = paddr3;
   block_device_p->vq->descriptor[brq3_desc_idx].length  = len3;
   block_device_p->vq->descriptor[brq3_desc_idx].flags   = flags3;
   block_device_p->vq->descriptor[brq3_desc_idx].next    = next3;

   driver_idx = block_device_p->vq->driver->idx;
   block_device_p->vq->driver->ring[driver_idx % queue_size] = brq1_desc_idx;
   // now the device knows
   block_device_p->vq->driver->idx += 1;


   uint32_t *qnotify = (uint32_t *)block_device_p->notify;
   *qnotify = 0;

   int lookahead_idx = block_device_p->acknowledged_idx;
   // poll if asked
   if(do_poll)
   {
      // DEBUGMSG("polling");
      // printf("lookahead: %d\n", lookahead_idx);
      int request_complete = 0;
      while(!(request_complete))
      {
         if(lookahead_idx != block_device_p->vq->device->idx)
         {
            uint32_t vq_ring_idx = lookahead_idx % queue_size;
            if(block_device_p->vq->device->ring[vq_ring_idx].id ==
                                                            brq1_desc_idx)
            {
               request_complete = 1;
            }
            else
            {
               // DEBUGMSG("async request");
               // printf("there must be an async request!\n");
            }
            lookahead_idx += 1;
         }
      }
   }

   kfree((void*)brq); // discard volatile qualifier with a cast
   // TODO: move me to the interrupts function

   return 0;
}

int block_read_arbitrary( int dev, char *buffer, uint64_t start, uint64_t size)
{
   uint32_t block_size = block_devices[dev].block_specific->block_size;

   uint64_t block_read_start;
   uint64_t block_read_end;
   block_read_start = (start/block_size) * block_size;
   block_read_end = ((start+size+ block_size-1) / block_size) * block_size;
   char *tbuffer = kcalloc(block_read_end-block_read_start);
   // printf("asking the block device %d to read from %u to %u\n",
           // dev, block_read_start, block_read_end);
   if(block_read_poll(dev, tbuffer, block_read_start, block_read_end))
   {
      kfree(tbuffer);
      return 1;
   }
   uint64_t request_offset = start - block_read_start;

   memcpy(buffer, tbuffer + request_offset, size);

   kfree(tbuffer);
   return 0;
}

int block_write_arbitrary(int dev, char *buffer, uint64_t start, uint64_t size)
{
   uint32_t block_size = block_devices[dev].block_specific->block_size;

   uint64_t block_read_start;
   uint64_t block_read_end;
   block_read_start = (start/block_size) * block_size;
   block_read_end = ((start+size+ block_size-1) / block_size) * block_size;

   char *tbuffer = kcalloc(block_read_end-block_read_start);
   MALLOC_CHECK(tbuffer);
   if(block_read_poll(dev, tbuffer, block_read_start, block_read_end))
   {
      kfree(tbuffer);
      return 1;
   }
   uint64_t request_offset = start - block_read_start;

   memcpy(tbuffer + request_offset, buffer, size);

   if(block_write_poll(dev, tbuffer, block_read_start, block_read_end))
   {
      kfree(tbuffer);
      return 2;
   }
   kfree(tbuffer);
   return 0;
}

void block_irq_handler(void)
{
   int i;
   block_device_t *block_device_p;
   for(i = 0; i < num_block_devices; i++)
   {
      block_device_p = block_devices+i;
      if(block_device_p->isr->queue_interrupt)
      {
         while(block_device_p->acknowledged_idx != block_device_p->vq->device->idx)
         {
            // walk the request (assuming length 3) and print out
            int queue_size = block_device_p->common_config->queue_size;
            uint16_t ack_idx = block_device_p->acknowledged_idx;
            uint16_t desc_id;
            uint64_t paddr;
            desc_id = block_device_p->vq->device->ring[ack_idx % queue_size].id;
            desc_id = (desc_id + 2) % queue_size;
            paddr = block_device_p->vq->descriptor[desc_id].address;
            uint8_t block_status = ((block_request_desc_3_t*)paddr)->status;

            // printf("status for ack %03d: 0x%02x\n", ack_idx, block_status);
            if(block_status != VIRTIO_BLOCK_STATUS_OK)
            {
               ERRORMSG("request not satisfied");
               printf("status for ack %03d: 0x%02x\n", ack_idx, block_status);
               block_print_irq(block_device_p);
            }
            block_device_p->acknowledged_idx += 1;
            // printf("incremented acknowledged idx\n");
         }
      }
   }
   return;
}

void block_print_irq(block_device_t *block_device_p)
{
   block_device_t block_device = *block_device_p;
   // walk the request (assuming length 3) and print out
   int queue_size = block_device.common_config->queue_size;
   uint16_t ack_idx = block_device.acknowledged_idx;
   printf("acknowledging idx of %d\n", ack_idx);

   uint16_t desc_id, desc_chain_length;
   desc_id = block_device.vq->device->ring[ack_idx % queue_size].id;
   desc_chain_length =
               block_device.vq->device->ring[ack_idx % queue_size].length;
   printf("id: %d\tlen: %d\n", desc_id, desc_chain_length);


   uint64_t paddr = block_device.vq->descriptor[desc_id].address;
   printf("type: 0x%08x\t sector: 0x%016lx\n",
            ((block_request_desc_1_t*)paddr)->type,
            ((block_request_desc_1_t*)paddr)->sector);


   desc_id = (desc_id + 1) % queue_size;
   paddr = block_device.vq->descriptor[desc_id].address;
   printf("data address: 0x%016lx\n", (block_request_desc_2_t*)paddr);


   desc_id = (desc_id + 1) % queue_size;
   paddr = block_device.vq->descriptor[desc_id].address;
   printf("status: 0x%016lx\n", ((block_request_desc_3_t*)paddr)->status);
}

void block_print_virtq(void)
{
   int i;
   for(i = 0; i < num_block_devices; i++)
   {
      block_device_t *block_device_p = block_devices+i;
      int queue_size = block_device_p->common_config->queue_size;
      virtio_print_virtq_all(block_device_p->vq, queue_size);
   }
}
