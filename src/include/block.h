#pragma once

#include <virtio.h>
#include <pcie.h>
#include <stdint.h>
#include <lock.h>

#define VIRTIO_BLOCK_TYPE_IN       0
#define VIRTIO_BLOCK_TYPE_OUT      1
#define VIRTIO_BLOCK_TYPE_FLUSH    4
#define VIRTIO_BLOCK_TYPE_DISCARD  11
#define VIRTIO_BLOCK_TYPE_ZERO     13/* write zeroes */

#define VIRTIO_BLOCK_STATUS_OK           0
#define VIRTIO_BLOCK_STATUS_IOERROR      1
#define VIRTIO_BLOCK_STATUS_UPSUPPORTED  2
#define VIRTIO_BLOCK_STATUS_RANDOM       77

// 0xfe00, or 65024
#define BLOCK_MAX_REQUEST_SIZE     0xfe00 // only reads bottom 12 bits
// so, if you have 512 as the block size, there are only 7 bits for the length.

typedef struct virtio_block_geometry
{
   uint16_t cylinders;
   uint8_t heads;
   uint8_t sectors;
} virtio_block_geometry_t;

typedef struct virito_block_topology
{
   uint8_t physical_block_exp; // number of logical blocks per physical block
   uint8_t alignment_offset;   // offset of first aligned logical block
   uint16_t min_io_size;       // suggested minimum I/O size in blocks
   uint32_t max_io_size;       // suggested maximum I/O size in blocks (optimal)
} virtio_block_topology_t;

// this structure taken from the class notes, courtesy of Dr. Marz.
typedef struct virtio_block_config
{
   uint64_t capacity;
   uint32_t size_max;
   uint32_t seg_max;
   virtio_block_geometry_t geometry;
   uint32_t block_size; // size of a sector (usually 512)
   virtio_block_topology_t topology;
   uint8_t  writeback;
   uint8_t  padding0[3];
   uint32_t max_discard_sectors;
   uint32_t max_discard_seg;
   uint32_t discard_sector_alignment;
   uint32_t max_write_zeroes_sectors;
   uint32_t max_write_zeroes_seg;
   uint8_t  write_zeroes_may_unmap;
   uint8_t  padding1[3];
} virtio_block_config_t;


typedef struct block_device
{
   volatile virtio_pci_common_cfg_t *common_config;
   volatile virtio_pci_notify_cfg_t *notify;
   volatile virtio_pci_isr_t        *isr;
   volatile virtio_block_config_t   *block_specific; // device specific
   volatile virtq_t                 *vq;
   int which_irq;
   int enabled;
   uint16_t requests_idx;
   uint16_t acknowledged_idx;
   mutex_t block_mutex;
} block_device_t;

typedef struct block_request_desc_1
{
   uint32_t type;
   uint32_t padding;
   uint64_t sector;
} block_request_desc_1_t;

typedef struct block_reqest_desc_2
{
   char *data; // which will be contiguous space capable of holding however
               // many bytes were requested (possibly aligned up to 512)
} block_request_desc_2_t;

typedef struct block_reqest_desc_3
{
   uint8_t status; // stores a code for the device to return to us
} block_request_desc_3_t;

typedef struct block_request
{
   block_request_desc_1_t brq1; // header
   block_request_desc_2_t brq2; // data
   block_request_desc_3_t brq3; // status
} block_request_t;


extern block_device_t *block_devices;
extern int num_block_devices;


int virtio_pcie_block_driver(PCIe_driver_t *dev);
int block_read( int dev, char *buffer, uint64_t start_byte, uint64_t end_byte);
int block_write(int dev, char *buffer, uint64_t start_byte, uint64_t end_byte);
int block_read_poll( int dev, char *buffer, uint64_t s_byte, uint64_t e_byte);
int block_write_poll(int dev, char *buffer, uint64_t s_byte, uint64_t e_byte);
int block_read_write(int which_device, char *buffer,
                     uint64_t start_byte, uint64_t end_byte,
                     int write_request, int do_poll);

// these functions will also poll
int block_read_arbitrary( int dev, char *buffer, uint64_t start, uint64_t size);
int block_write_arbitrary(int dev, char *buffer, uint64_t start, uint64_t size);

void block_irq_handler(void);

// print
void block_print_irq(block_device_t *block_device_p);
void block_print_virtq(void);
