#pragma once

#include <virtio.h>
#include <pcie.h>
#include <stdint.h>
#include <lock.h>

int virtio_pcie_gpu_driver(PCIe_driver_t *dev);

// struct courtesy of Dr. Marz' lecture notes
#define VIRTIO_GPU_EVENT_DISPLAY 1
typedef struct virtio_gpu_config
{
   uint32_t events_read;
   uint32_t events_clear;
   uint32_t num_scanouts;
   uint32_t padding;
} virtio_gpu_config_t;

// struct courtesy of Dr. Marz' lecture notes
#define VIRTIO_GPU_FLAG_FENCE 1
typedef struct gpu_control_header
{
   uint32_t control_type;
   uint32_t flags;
   uint64_t fence_id;
   uint32_t context_id;
   uint32_t padding;
} gpu_control_header_t;

// if the flags is set to VIRTIO_GPU_FLAG_FENCE,
// then the fence_id will be one of (also courtesy of lecture notes):
enum ControlType
{
   /* 2D Commands */
   GPU_CMD_GET_DISPLAY_INFO = 0x0100,
   GPU_CMD_RESOURCE_CREATE_2D,
   GPU_CMD_RESOURCE_UNREF,
   GPU_CMD_SET_SCANOUT,
   GPU_CMD_RESOURCE_FLUSH,
   GPU_CMD_TRANSFER_TO_HOST_2D,
   GPU_CMD_RESOURCE_ATTACH_BACKING,
   GPU_CMD_RESOURCE_DETACH_BACKING,
   GPU_CMD_GET_CAPSET_INFO,
   GPU_CMD_GET_CAPSET,
   GPU_CMD_GET_EDID,

   /* cursor commands */
   GPU_CMD_UPDATE_CURSOR = 0x0300,
   GPU_CMD_MOVE_CURSOR,

   /* success responses */
   GPU_RESP_OK_NODATA = 0x1100,
   GPU_RESP_OK_DISPLAY_INFO,
   GPU_RESP_OK_CAPSET_INFO,
   GPU_RESP_OK_CAPSET, GPU_RESP_OK_EDID,

   /* error responses */
   GPU_RESP_ERR_UNSPEC = 0x1200,
   GPU_RESP_ERR_OUT_OF_MEMORY,
   GPU_RESP_ERR_INVALID_SCANOUT_ID,
   GPU_RESP_ERR_INVALID_RESOURCE_ID,
   GPU_RESP_ERR_INVALID_CONTEXT_ID,
   GPU_RESP_ERR_INVALID_PARAMETER
};

typedef struct gpu_rectangle
{
   uint32_t x;       // which display (higher index = lower vertically)
                     // alternatively: the start point of a rectangle
   uint32_t y;       // which display (higher index = right horizontally)
                     // alternatively: the start point of a rectangle
   uint32_t width;   // display width
   uint32_t height;  // display height.
} gpu_rectangle_t;

typedef struct gpu_circle
{
   uint32_t x;       // higher index = right horizontally
   uint32_t y;       // higher index = lower vertically
   uint32_t radius;
} gpu_circle_t;

typedef struct gpu_display
{
   gpu_rectangle_t rectangle;
   uint32_t enabled;
   uint32_t flags;
} gpu_display_t;

#define GPU_MAX_SCANOUTS 16
typedef struct gpu_get_display_info_response
{
   gpu_control_header_t header; // should be GPU_RESP_OK_DISPLAY_INFO
   gpu_display_t displays[GPU_MAX_SCANOUTS];
} gpu_get_display_info_response_t;

// enum struct courtesy of lecture notes
enum GpuFormats
{
   B8G8R8A8_UNORM = 1,
   B8G8R8X8_UNORM = 2,
   A8R8G8B8_UNORM = 3,
   X8R8G8B8_UNORM = 4,
   R8G8B8A8_UNORM = 67,
   X8B8G8R8_UNORM = 68,
   A8B8G8R8_UNORM = 121,
   R8G8B8X8_UNORM = 134
};

typedef struct gpu_resource_create_2d_request_header
{
   gpu_control_header_t header;/* should be VIRTIO_GPU_CMD_RESOURCE_CREATE_2D */
   uint32_t resource_id;       /* We give a unique ID to each resource */
   uint32_t format;            /* color mode (enumerated above) */
   uint32_t width;
   uint32_t height;
} gpu_resource_create_2d_request_header_t;

typedef struct gpu_resource_unref_header
{
   gpu_control_header_t header;/* should be VIRTIO_GPU_CMD_RESOURCE_UNREF */
   uint32_t resource_id;       /* We give a unique ID to each resource */
   uint32_t padding;
} gpu_resource_unref_header_t;

// assumes RGBA pixel
typedef struct pixel
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
} pixel_t;

typedef struct gpu_frame_buffer
{
   uint32_t width;
   uint32_t height;
   pixel_t *pixels;
} gpu_frame_buffer_t;

typedef struct gpu_resource_attach_backing_header
{
   gpu_control_header_t header;/* will be GPU_CMD_RESOURCE_ATTACH_BACKING */
   uint32_t resource_id;       /* We give a unique ID to each resource */
   uint32_t num_entries;
} gpu_resource_attach_backing_header_t;

typedef struct gpu_memory_entry
{
   uint64_t paddr;
   uint32_t length;
   uint32_t padding;
} gpu_memory_entry_t;

typedef struct gpu_set_scanout_header
{
   gpu_control_header_t header; /* will be GPU_CMD_SET_SCANOUT */
   gpu_rectangle_t      rectangle;
   uint32_t             scanout_id;
   uint32_t             resource_id;
} gpu_set_scanout_header_t;

typedef struct gpu_transfer_to_host_2d_header
{
   gpu_control_header_t header; /* will be GPU_CMD_TRANSFER_TO_HOST_2D */
   gpu_rectangle_t      rectangle;
   uint64_t             offset;
   uint32_t             resource_id;
   uint32_t             padding;
} gpu_transfer_to_host_2d_header_t;

typedef struct gpu_resource_flush_header
{
   gpu_control_header_t header; /* will be GPU_CMD_RESROUCE_FLUSH */
   gpu_rectangle_t      rectangle;
   uint32_t             resource_id;
   uint32_t             padding;
} gpu_resource_flush_header_t;

typedef struct gpu_device
{
   volatile virtio_pci_common_cfg_t *common_config;
   volatile virtio_pci_notify_cfg_t *notify;
   volatile virtio_pci_isr_t        *isr;
   // TODO: virtio_pci_gpu config? If so, then also change block driver.
   volatile virtio_gpu_config_t     *gpu_specific;
   volatile virtq_t                 *vq;
   volatile gpu_frame_buffer_t      fb; // note: NOT a pointer
   int which_irq;
   int enabled;
   int active;
   uint16_t requests_idx;
   uint16_t acknowledged_idx;
   mutex_t gpu_mutex;
} gpu_device_t;

extern gpu_device_t gpu_device;

int gpu_request_read_poll(void *request,  int request_size,
                     void *data,     int data_size,
                     void *response, int response_size);
int gpu_request_write_poll(void *request,  int request_size,
                     void *data,     int data_size,
                     void *response, int response_size);
int gpu_request_poll(void *request,  int request_size,
                     void *data,     int data_size,
                     void *response, int response_size,
                     int do_write);
int gpu_get_display_info(gpu_get_display_info_response_t* disp_data);
int gpu_init(void);
int gpu_transfer_and_flush(void);
int gpu_transfer_and_flush_some(gpu_rectangle_t *r);

// drawing
uint32_t gpu_get_fb_row(uint64_t offset);
uint32_t gpu_get_fb_column(uint64_t offset);
uint64_t gpu_get_fb_offset(uint32_t row, uint32_t column);
uint64_t *gpu_get_fb_offset_into_rectangle(uint32_t row, uint32_t max_row,
                                           uint32_t column, uint32_t max_column,
                                           gpu_rectangle_t *r);

void gpu_circle_to_rectangle(gpu_rectangle_t *fill, gpu_circle_t *src);

void gpu_fill_rectangle(volatile gpu_frame_buffer_t *fb,
                        gpu_rectangle_t *r,
                        pixel_t color);
void gpu_fill_circle(volatile gpu_frame_buffer_t *fb,
                     gpu_circle_t *c,
                     pixel_t color);
void gpu_fill_white(volatile gpu_frame_buffer_t *fb);
void gpu_fill_with_block_device(volatile gpu_frame_buffer_t *fb);
int gpu_fill_with_bmp(volatile gpu_frame_buffer_t *fb);
int gpu_fill_with_block_device_and_flush(void);
int gpu_fill_with_bmp_and_flush(void);

void gpu_irq_handler(void);

uint32_t gpu_x_pixel_to_percent(uint32_t y_pixel);
uint32_t gpu_y_pixel_to_percent(uint32_t y_pixel);
uint32_t gpu_percent_to_x_pixel(uint32_t percent);
uint32_t gpu_percent_to_y_pixel(uint32_t percent);

// interact with input device
void gpu_regions_test(void);
void gpu_draw_from_cursor(void);
void gpu_draw_endless(void);

// printing
void gpu_print_gdi_response(gpu_get_display_info_response_t *gdir);
void gpu_print_device_specific(volatile virtio_gpu_config_t *gcfg);
void gpu_print_virtq(void);
