#include <virtio.h>
#include <printf.h>
#include <mmu.h>

void virtio_show_vendor_specific_cap(virtio_pci_cap_t *cp)
{
   printf("   capability vendor : 0x%02x\n", cp->cap_vendor);
   printf("   capability nextp  : 0x%02x\n", cp->cap_next);
   printf("   capability vendor : 0x%02x\n", cp->cap_length);
   printf("   configuration type: 0x%02x\n", cp->config_type);
   printf("   bar               : 0x%02x\n", cp->bar);
   printf("   offset            : 0x%08x\n", cp->offset);
   printf("   length            : 0x%08x\n", cp->length);
   return;
}

void virtio_print_common_cfg(volatile virtio_pci_common_cfg_t *ccfg)
{
   printf("virtio common config:   0x%016x\n", ccfg);
   printf("device_feature_select:  0x%016x\n", ccfg->device_feature_select);
   printf("device_feature:         0x%016x\n", ccfg->device_feature);
   printf("driver_feature_select:  0x%016x\n", ccfg->driver_feature_select);
   printf("driver_feature:         0x%016x\n", ccfg->driver_feature);
   printf("msix_config:            0x%016x\n", ccfg->msix_config);
   printf("num_queues:             0x%016x\n", ccfg->num_queues);
   printf("device_status:          0x%016x\n", ccfg->device_status);
   printf("config_generation:      0x%016x\n", ccfg->config_generation);
   printf("queue_select:           0x%016x\n", ccfg->queue_select);
   printf("queue_size:             0x%016x\n", ccfg->queue_size);
   printf("queue_msix_vector:      0x%016x\n", ccfg->queue_msix_vector);
   printf("queue_enable:           0x%016x\n", ccfg->queue_enable);
   printf("queue_notify_off:       0x%016x\n", ccfg->queue_notify_off);
   printf("queue_desc:             0x%016x\n", ccfg->queue_desc);
   printf("queue_driver:           0x%016x\n", ccfg->queue_driver);
   printf("queue_device:           0x%016x\n", ccfg->queue_device);
   printf("\n");
}

void virtio_print_virtq_desc(volatile virtq_desc_t *vqd, int queue_size)
{
   printf("virq desc table at 0x%012lx\n", (uint64_t)vqd);
   printf("   element     : address           , length    , flags , next\n");
   int i;
   for(i = 0; i < queue_size; i++)
   {
      // printf("    vaddr 0x%012lx -> paddr 0x%012lx\n",
         // (uint64_t)(vqd+i), mmu_translate(kernel_mmu_table, (uint64_t)(vqd+i)));
      printf("   desc[0x%04x]: 0x%016x, 0x%08x, 0x%04x, 0x%04x\n",
               i, vqd[i].address, vqd[i].length, vqd[i].flags, vqd[i].next);
      if(i >= MAX_PRINTF_LINES_VIRTIO)
      {
         printf("   ...\n");
         break;
      }
   }
}

void virtio_print_virtq_driver(volatile virtq_driver_t *vqd, int queue_size)
{
   printf("virq driver at 0x%012lx\n", (uint64_t)vqd);
   printf("flags: 0x%04x\n", vqd->flags);
   printf("idx  : 0x%04x\n", vqd->idx);
   int i;
   for(i = 0; i < queue_size; i++)
   {
      printf("   ring[0x%04x]: 0x%04x\n", i, vqd->ring[i]);
      if(i >= MAX_PRINTF_LINES_VIRTIO)
      {
         printf("   ...\n");
         break;
      }
   }
}

void virtio_print_virtq_device(volatile virtq_device_t *vqd, int queue_size)
{
   printf("virq device at 0x%012lx\n", (uint64_t)vqd);
   printf("flags: 0x%04x\n", vqd->flags);
   printf("idx  : 0x%04x\n", vqd->idx);
   int i;
   printf("                 id        , length\n");
   for(i = 0; i < queue_size; i++)
   {
      printf("   ring[0x%04x]: 0x%08x, 0x%08x\n",
               i, vqd->ring[i].id, vqd->ring[i].length);
      if(i >= MAX_PRINTF_LINES_VIRTIO)
      {
         printf("   ...\n");
         break;
      }
   }
}

void virtio_print_virtq_all(   volatile virtq_t   *vq , int queue_size)
{
   printf("\n");
   printf("showing ALL of a virtq\n");
   printf("----------------------------------------------------------------\n");
   printf("virtq at 0x%012lx of queue size %d\n", (uint64_t) vq, queue_size);
   printf("\n");
   virtio_print_virtq_desc(vq->descriptor, queue_size);
   printf("\n");
   virtio_print_virtq_driver(vq->driver, queue_size);
   printf("\n");
   virtio_print_virtq_device(vq->device, queue_size);
   printf("\n");
}

