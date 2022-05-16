#pragma once

#include <virtio.h>
#include <pcie.h>
#include <lock.h>
#include <stdint.h>

#define INPUT_RB_SIZE 500000

#define IE_TYPE_SYNC   0x0000
#define IE_TYPE_KEY    0x0001
#define IE_TYPE_TABLET 0x0003

#define IE_TABLET_CODE_X 0x0000
#define IE_TABLET_CODE_Y 0x0001

#define IE_KEY_CODE_LMOUSE  0x0110

#define IE_KEY_VALUE_RELEASE 0x0000
#define IE_KEY_VALUE_PRESS   0x0001

typedef struct input_event
{
   uint16_t type;
   uint16_t code;
   uint32_t value;
} input_event_t;

typedef struct input_cursor_position
{
   uint32_t x;
   uint32_t y;
} input_cursor_position_t;

// taken from lecture notes
enum virtio_input_config_select
{
    INPUT_CFG_UNSET       = 0x00,
    INPUT_CFG_ID_NAME     = 0x01,
    INPUT_CFG_ID_SERIAL   = 0x02,
    INPUT_CFG_ID_DEVIDS   = 0x03,
    INPUT_CFG_PROP_BITS   = 0x10,
    INPUT_CFG_EV_BITS     = 0x11,
    INPUT_CFG_ABS_INFO    = 0x12
};

// taken from lecture notes. Seems unused...
// Could be accessed for additional information but not needed for the OS now.
typedef struct input_absinfo {
    uint32_t min;
    uint32_t max;
    uint32_t fuzz;
    uint32_t flat;
    uint32_t res;
} input_absinfo_t;

// taken from lecture notes. Seems unused...
// Would help identify what device if used
typedef struct input_devids {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
} input_devids_t;

typedef struct virtio_input_config {
    uint8_t select; uint8_t subsel; uint8_t size;
    uint8_t padding[5];
    union {
        char            string[128];
        uint8_t         bitmap[128];
        input_absinfo_t abs;
        input_devids_t  ids;
    };
} virtio_input_config_t;

typedef struct input_device
{
   volatile virtio_pci_common_cfg_t *common_config;
   volatile virtio_pci_notify_cfg_t *notify;
   volatile virtio_pci_isr_t        *isr;
   volatile virtio_input_config_t   *input_specific;
   volatile virtq_t                 *vq;
   int which_irq;
   int enabled;
   uint16_t requests_idx;
   uint16_t acknowledged_idx;
   mutex_t input_mutex;
} input_device_t;



int virtio_pcie_input_driver(PCIe_driver_t *dev);


void input_irq_handler(void);

// printing functions
void input_print_pixel(input_event_t *iev);
void input_print_event(input_event_t *iev);
void input_print_device_specific(volatile virtio_input_config_t *icfg);
void input_print_virtq(void);
