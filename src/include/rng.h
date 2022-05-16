#pragma once

#include <virtio.h>
#include <pcie.h>
#include <stdint.h>

typedef struct rng_device
{
   volatile virtio_pci_common_cfg_t *common_config;
   volatile virtio_pci_notify_cfg_t *notify;
   volatile virtio_pci_isr_t        *isr;
   volatile virtq_t                 *vq;
   int                     which_irq;
   int                     enabled;
   // at_idx: where should the driver work up to in the ring?
   uint16_t                at_idx; // I wish I could come up with a better name
   uint16_t                acknowledged_idx;
} rng_device_t;

int virtio_pcie_rng_driver(PCIe_driver_t *dev);
int rng_fill_poll(void *buffer, int size);
void rng_irq_handler(void);

char rand8(void);

void rng_print_virtq(void);
