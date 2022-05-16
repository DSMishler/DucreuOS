#pragma once

#include <stdint.h>

#define VIRTIO_CAP_CFG_COMMON 1
#define VIRTIO_CAP_CFG_NOTIFY 2
#define VIRTIO_CAP_CFG_ISR    3
#define VIRTIO_CAP_CFG_DEVICE 4
#define VIRTIO_CAP_CFG_PCI    5

// is there a way to do this in a macro except you can return instead?
// TODO: can I make it such that I can say char *cape_name = GET_CAP_NAME(j);?
#define GET_CAP_NAME(x, i)                \
   switch(i)                              \
   {                                      \
   case(VIRTIO_CAP_CFG_COMMON):           \
      x = "common config  ";              \
      break;                              \
   case(VIRTIO_CAP_CFG_NOTIFY):           \
      x = "notifications  ";              \
      break;                              \
   case(VIRTIO_CAP_CFG_ISR):              \
      x = "isr status     ";              \
      break;                              \
   case(VIRTIO_CAP_CFG_DEVICE):           \
      x = "device specific";              \
      break;                              \
   case(VIRTIO_CAP_CFG_PCI):              \
      x = "PCIe config    ";              \
      break;                              \
   default:                               \
      x = "";                             \
   }

#define VIRTIO_DEV_STATUS_RESET           0
#define VIRTIO_DEV_STATUS_ACKNOWLEDGE     (1 << 0)
#define VIRTIO_DEV_STATUS_DRIVER          (1 << 1)
#define VIRTIO_DEV_STATUS_DRIVER_OK       (1 << 2)
#define VIRTIO_DEV_STATUS_FEATURES_OK     (1 << 3)

typedef struct virtio_pci_cap
{
   uint8_t cap_vendor;
   uint8_t cap_next;
   uint8_t cap_length;
   uint8_t config_type; // which structure? (1 of 5)
   uint8_t bar;         // which bar
   uint8_t padding[3];  // for alignment
   uint32_t offset;     // where to look to find the BAR
   uint32_t length;     // length of the structure (bytes)
} virtio_pci_cap_t;


/* courtesy of class code */
typedef struct virtio_pci_common_cfg /* capability corresponding to type 1 */
{
   uint32_t device_feature_select;     /* RW (read-write for driver) */
   uint32_t device_feature;            /* R- (read-only  for driver) */
   uint32_t driver_feature_select;     /* RW */
   uint32_t driver_feature;            /* RW */
   uint16_t msix_config;               /* RW */
   uint16_t num_queues;                /* R- */
   uint8_t device_status;              /* RW */
   uint8_t config_generation;          /* R- */
   /* about a specific virtqueue */
   uint16_t queue_select;              /* RW */
   uint16_t queue_size;                /* RW */
   uint16_t queue_msix_vector;         /* RW */
   uint16_t queue_enable;              /* RW */
   uint16_t queue_notify_off;          /* R- */
   uint64_t queue_desc;                /* RW */
   uint64_t queue_driver;              /* RW */
   uint64_t queue_device;              /* RW */
} virtio_pci_common_cfg_t;

typedef struct virtio_pci_notify_cfg /* type 2 */
{
   virtio_pci_cap_t cap;            // all of the original structure, and
   uint32_t notify_off_multiplier;  // for queue_notify_off
} virtio_pci_notify_cfg_t;


typedef struct virtio_pci_isr /* type 3 */
{
   union
   {
      struct
      {
         unsigned queue_interrupt: 1;
         unsigned device_cfg_interrupt: 1;
         unsigned reserved: 30;
      };
      unsigned int isr_cap;
   };
} virtio_pci_isr_t;

/* buffer should continue */
#define VIRTQ_DESC_FLAG_NEXT      1 << 0
/* buffer is device write-only (else read-only for the device) */
#define VIRTQ_DESC_FLAG_WRITE     1 << 1
/* buffer contains a list of buffer descriptors */
#define VIRTQ_DESC_FLAG_INDIRECT  1 << 2

typedef struct virtq_desc
{
   uint64_t address; /* physical */
   uint32_t length;
   uint16_t flags;
   uint16_t next;    /* next descriptor if flags call for it */
} virtq_desc_t;

/* do not interrupt */
#define VIRTQ_DRIVER_FLAG_NO_INTERRUPT 1 << 0

typedef struct virtq_driver
{
   uint16_t flags;
   uint16_t idx;
   uint16_t ring[]; // needs to be contiguous.
   // uint16_t device_event;
} virtq_driver_t;

typedef struct virtq_used_element
{
   /* id is only 16 bit but 32 are used for padding */
   uint32_t id;     /* index of start of used descriptor chain */
   uint32_t length; /* total length of descriptor chain */
} virtq_used_element_t;

#define VIRTQ_DEVICE_FLAG_NO_NOTIFY 1 << 0
typedef struct virtq_device
{
   uint16_t flags;
   uint16_t idx;
   virtq_used_element_t ring[];
   // uint16_t driver_event;
} virtq_device_t;

typedef struct virtq
{
   virtq_desc_t *descriptor;
   virtq_driver_t *driver;
   virtq_device_t *device;
} virtq_t;



// for printing to the screen

#define MAX_PRINTF_LINES_VIRTIO 16
// technically, it prints this many + 1
// but we're more worried about not printing 512

void virtio_show_vendor_specific_cap(virtio_pci_cap_t *cp);
void virtio_print_common_cfg(  volatile virtio_pci_common_cfg_t *ccfg);
void virtio_print_virtq_desc(  volatile virtq_desc_t   *vqd, int queue_size);
void virtio_print_virtq_driver(volatile virtq_driver_t *vqd, int queue_size);
void virtio_print_virtq_device(volatile virtq_device_t *vqd, int queue_size);
void virtio_print_virtq_all(   volatile virtq_t        *vq , int queue_size);

