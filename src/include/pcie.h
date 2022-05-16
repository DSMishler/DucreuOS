#pragma once
#include <stdint.h>

#define ECAM_BASE 0x30000000

#define ECAM_COMMAND_IOSPACE_BIT          (1 << 0)
#define ECAM_COMMAND_MEMORY_SPACE_BIT     (1 << 1)
#define ECAM_COMMAND_BUS_MASTER_BIT       (1 << 2)

#define ECAM_STATUS_CAPABILITIES_BIT      (1 << 4)

// check if Bar[2:1] is 0b10
#define PCIE_BAR_IS_64_BIT(x) \
         (x & 0x6UL) == 0x4UL

// ECAM header struct courtesy of Dr. Marz in lecture nodes
typedef struct EcamHeader
{
   uint16_t vendor_id;
   uint16_t device_id;
   uint16_t command_reg;
   uint16_t status_reg;
   uint8_t revision_id;
   uint8_t prog_if;
   union
   {
      uint16_t class_code;
      struct
      {
         uint8_t class_subcode;
         uint8_t class_basecode;
      };
   };
   uint8_t cacheline_size;
   uint8_t latency_timer;
   uint8_t header_type;
   uint8_t bist;
   union
   {
      struct
      {
         uint32_t bar[6];
         uint32_t cardbus_cis_pointer;
         uint16_t sub_vendor_id;
         uint16_t sub_device_id;
         uint32_t expansion_rom_addr;
         uint8_t  capes_pointer;
         uint8_t  reserved0[3];
         uint32_t reserved1;
         uint8_t  interrupt_line;
         uint8_t  interrupt_pin;
         uint8_t  min_gnt;
         uint8_t  max_lat;
      } type0;
      struct
      {
         uint32_t bar[2];
         uint8_t  primary_bus_no;
         uint8_t  secondary_bus_no;
         uint8_t  subordinate_bus_no;
         uint8_t  secondary_latency_timer;
         uint8_t  io_base;
         uint8_t  io_limit;
         uint16_t secondary_status;
         uint16_t memory_base;
         uint16_t memory_limit;
         uint16_t prefetch_memory_base;
         uint16_t prefetch_memory_limit;
         uint32_t prefetch_base_upper;
         uint32_t prefetch_limit_upper;
         uint16_t io_base_upper;
         uint16_t io_limit_upper;
         uint8_t  capes_pointer;
         uint8_t  reserved0[3];
         uint32_t expansion_rom_addr; uint8_t  interrupt_line;
         uint8_t  interrupt_pin;
         uint16_t bridge_control;
      } type1;
      struct // use this if referring to common segments between types
      {
         uint32_t reserved0[9]; // NOT common
         uint8_t  capes_pointer;
         uint8_t  reserved1[3];
         uint32_t reserved2;
         uint8_t  interrupt_line;
         uint8_t  interrupt_pin;
         uint8_t  reserved3[2];
      } common;
   };
} ecam_header_t;

typedef struct ecam_capability
{
   uint8_t id;
   uint8_t next_offset;
   // next_offset is NOT the pointer to the next capability:
   // it is the next capability's OFFSET from the base.
} ecam_capability_t;

typedef struct PCIe_driver
{
   uint16_t vendor_id;
   uint16_t device_id;
   int (*driver_func)(struct PCIe_driver *device);
   // this is a funtion that takes a driver pointer
   // TODO: really? a driver pointer?
} PCIe_driver_t;

typedef struct PCIe_driver_llist // llist for linked list
{
   PCIe_driver_t driver;
   struct PCIe_driver_llist *next;
} PCIe_driver_llist_t;

// these match the indices in virtio.h
# define GLOBAL_VIRTIO_BARS_OFFSET_COMMON   1
# define GLOBAL_VIRTIO_BARS_OFFSET_NOTIFY   2
# define GLOBAL_VIRTIO_BARS_OFFSET_ISR      3
# define GLOBAL_VIRTIO_BARS_OFFSET_SPECIFC  4
# define GLOBAL_VIRTIO_BARS_OFFSETS_LEN     6

// to keep track of which device is on which BAR
typedef struct PCIe_bar_list
{
   uint16_t vendorIDs[16];
   uint16_t deviceIDs[16];
   uint16_t which_bar[16];
   uint16_t which_irq[16];
   uint32_t virtio_register_offsets[16][GLOBAL_VIRTIO_BARS_OFFSETS_LEN];
   char notes[16][200];
} PCIe_bar_list_t;

// helper for allocation of the largers struct
typedef struct PCIe_bar_allocation
{
   int which_bar;
   uint32_t virtio_register_offsets[GLOBAL_VIRTIO_BARS_OFFSETS_LEN];
   volatile ecam_header_t *ec;
   char notes[200];
}PCIe_bar_allocation_t;

typedef struct PCIe_IRQ_llist_node
{
   int which_irq;
   void (*irq_handler)();
   char note[40];
   struct PCIe_IRQ_llist_node *next;
} PCIe_IRQ_llist_node_t;


ecam_header_t *pcie_get_ecam_address(int bus, int device, int function,int reg);
int pcie_get_bus(volatile ecam_header_t *ec);
int pcie_get_slot(volatile ecam_header_t *ec);
int pcie_ecam_to_irq(volatile ecam_header_t *ec);


void pcie_show_cycle_all_addresses(void);
void pcie_scan_and_init(void);
void pcie_init_bridge(volatile ecam_header_t *ec, int bus);
void pcie_init_bars(volatile ecam_header_t *ec);
void pcie_show_capabilities(volatile ecam_header_t *ec);


// bars
void pcie_add_to_bar_list(PCIe_bar_allocation_t *bar_allocation);
void pcie_show_bar_list();

// driver functions
void pcie_register_driver(uint16_t vendor_id, uint16_t device_id,
                          int (*driver_func)(PCIe_driver_t *device));

PCIe_driver_t * pcie_find_driver(uint16_t vendor_id, uint16_t device_id);
void pcie_init_drivers(void);

// IRQ
void pcie_show_irq_llist();
void pcie_register_irq(int which_irq, void (*irq_function)(), char *note);
void pcie_handle_irq(int which_irq);
