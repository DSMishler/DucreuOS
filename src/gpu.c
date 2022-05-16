#include <gpu.h>
#include <virtio.h>
#include <pcie.h>
#include <utils.h>
#include <printf.h>
#include <kmalloc.h>
#include <mmu.h>
#include <block.h>
#include <bmp.h>
#include <input.h>
#include <ring.h>

#include <regions.h>
#include <rng.h>

gpu_device_t gpu_device;
extern PCIe_bar_list_t global_pcie_bars_occupied;

#define GPBO global_pcie_bars_occupied
#define VRO  virtio_register_offsets
int virtio_pcie_gpu_driver(PCIe_driver_t *dev)
{
   int i;
   // TODO: replace this hard-coded 16 with a better way to talk/walk BARS
   for(i = 15; i >= 0; i--)
   {
      if(global_pcie_bars_occupied.vendorIDs[i] == dev->vendor_id &&
         global_pcie_bars_occupied.deviceIDs[i] == dev->device_id)
      {
         gpu_device.common_config = (virtio_pci_common_cfg_t *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_COMMON]);
         gpu_device.notify        = (virtio_pci_notify_cfg_t *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_NOTIFY]);
         gpu_device.isr           = (virtio_pci_isr_t        *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_ISR]);
         gpu_device.gpu_specific  = (virtio_gpu_config_t     *)(uint64_t)
              (0x40000000 + i*0x00100000 + GPBO.VRO[i][VIRTIO_CAP_CFG_DEVICE]);
         gpu_device.which_irq = GPBO.which_irq[i];
         break;
      }
   }
   if(i == -1)
   {
      ERRORMSG("could not find gpu BAR");
   }

   // 1: reset device
   gpu_device.common_config->device_status = VIRTIO_DEV_STATUS_RESET;

   // 2: acknowledge the device
   gpu_device.common_config->device_status = VIRTIO_DEV_STATUS_ACKNOWLEDGE;

   // 3: acknowledge we have a driver
   gpu_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER;

   // 4: read features (negotiate if needed)
   // looks like it's 64 by default. Let's just ensure that's the case
   gpu_device.common_config->queue_size = 12;
   if(gpu_device.common_config->queue_size != 12)
   {
      ERRORMSG("gpu device not a nice negotiator");
      return 3;
   }

   // 5: features are okay
   gpu_device.common_config->device_status |= VIRTIO_DEV_STATUS_FEATURES_OK;

   // 6: ask the device if it agrees
   if(gpu_device.common_config->device_status & VIRTIO_DEV_STATUS_FEATURES_OK)
   {
      ;
   }
   else
   {
      ERRORMSG("gpu device not happy with requested features");
      return 2;
   }

   // 7: device specific setup
   gpu_device.common_config->queue_select = 0;
   uint16_t queue_size = gpu_device.common_config->queue_size;
   uint64_t vaddr, paddr;
   virtq_t *vq;
   vq = kmalloc(sizeof(virtq_t));

   // descriptor table
   vaddr = (uint64_t)kcalloc(sizeof(virtq_desc_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->descriptor = (virtq_desc_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   gpu_device.common_config->queue_desc = paddr;

   // driver ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*(queue_size+3));
   MALLOC_CHECK(vaddr);
   vq->driver = (virtq_driver_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   gpu_device.common_config->queue_driver = paddr;

   // device ring
   vaddr = (uint64_t)kcalloc(sizeof(uint16_t)*3 +
                             sizeof(virtq_used_element_t)*queue_size);
   MALLOC_CHECK(vaddr);
   vq->device = (virtq_device_t *)vaddr;
   paddr = mmu_translate(kernel_mmu_table, vaddr);
   gpu_device.common_config->queue_device = paddr;

   gpu_device.common_config->queue_enable = 1;

   // 8: device is live!
   gpu_device.common_config->device_status |= VIRTIO_DEV_STATUS_DRIVER_OK;
   // TODO: is this technically a race condition? Should we set the vq first?
   gpu_device.vq = vq;
   gpu_device.requests_idx = 0;
   gpu_device.acknowledged_idx = 0;
   gpu_device.fb.pixels = NULL;
   gpu_device.enabled = 1;
   gpu_device.active = 0;
   gpu_device.gpu_mutex = MUTEX_UNLOCKED;

   pcie_register_irq(gpu_device.which_irq, gpu_irq_handler, "gpu");

   // virtio_print_common_cfg(gpu_device.common_config);
   // gpu_print_device_specific(gpu_device.gpu_specific);
   return 0;
}
int gpu_request_write_poll(void *request,  int request_size,
                     void *data,     int data_size,
                     void *response, int response_size)
{
   return gpu_request_poll(request, request_size,
                           data, data_size,
                           response, response_size,
                           1);
}

int gpu_request_read_poll(void *request,  int request_size,
                     void *data,     int data_size,
                     void *response, int response_size)
{
   return gpu_request_poll(request, request_size,
                           data, data_size,
                           response, response_size,
                           0);
}

int gpu_request_poll(void *request,  int request_size,
                     void *data,     int data_size,
                     void *response, int response_size,
                     int do_write)
{
   int queue_size = gpu_device.common_config->queue_size;
   uint64_t paddr;
   uint16_t request_idx, prev_request_idx, first_request_idx;
   uint16_t driver_idx, lookahead_idx;
   uint16_t virtq_data_flag;
   if(request == NULL)
   {
      ERRORMSG("cannot have null request");
      return 1;
   }

   if(do_write && data != NULL && response != NULL)
   {
      ; // prepared for this
   }
   else if(data == NULL && response != NULL) // with either read or write
   {
      ; // prepared for this
   }
   else
   {
      // don't guarantee that we can respond to it
      ERRORMSG("GPU_request_poll not prepared for inputs");
      return 2;
   }

   if(do_write) // do_write means: "I'm giving the GPU data that I write to"
   {
      // then there should also be a response
      virtq_data_flag = 0;
   }
   else
   {
      // then there should also be a response
      virtq_data_flag = VIRTQ_DESC_FLAG_WRITE;
   }

   // set up order, which will be <request> (<data>) (<reponse>)
   // only the request field is required.


   request_idx = (gpu_device.requests_idx % queue_size);

   paddr = mmu_translate(kernel_mmu_table, (uint64_t)request);
   gpu_device.vq->descriptor[request_idx].address = paddr;
   gpu_device.vq->descriptor[request_idx].length  = request_size;
   gpu_device.vq->descriptor[request_idx].flags   = 0;  // might be overwritten
   gpu_device.vq->descriptor[request_idx].next    = 0;  // might be overwritten

   first_request_idx = prev_request_idx = request_idx;
   gpu_device.requests_idx += 1;
   request_idx = (gpu_device.requests_idx % queue_size);

   if(data == NULL)
   {
      ; // do nothing
   }
   else
   {
      gpu_device.vq->descriptor[prev_request_idx].flags |= VIRTQ_DESC_FLAG_NEXT;
      gpu_device.vq->descriptor[prev_request_idx].next = request_idx;
      paddr = mmu_translate(kernel_mmu_table, (uint64_t)data);
      gpu_device.vq->descriptor[request_idx].address = paddr;
      gpu_device.vq->descriptor[request_idx].length  = data_size;
      gpu_device.vq->descriptor[request_idx].flags   = virtq_data_flag;
      gpu_device.vq->descriptor[request_idx].next    = 0;// might be overwritten

      prev_request_idx = request_idx;
      gpu_device.requests_idx += 1;
      request_idx = (gpu_device.requests_idx % queue_size);
   }

   if(response == NULL)
   {
      ERRORMSG("response shouldn't be NULL");
      return 4;
   }
   else
   {
      gpu_device.vq->descriptor[prev_request_idx].flags |= VIRTQ_DESC_FLAG_NEXT;
      gpu_device.vq->descriptor[prev_request_idx].next = request_idx;
      paddr = mmu_translate(kernel_mmu_table, (uint64_t)response);
      gpu_device.vq->descriptor[request_idx].address = paddr;
      gpu_device.vq->descriptor[request_idx].length  = response_size;
      gpu_device.vq->descriptor[request_idx].flags   = VIRTQ_DESC_FLAG_WRITE;
      gpu_device.vq->descriptor[request_idx].next    = 0; //won't be read anyway

      // prev_request_idx = request_idx;
      gpu_device.requests_idx += 1;
      request_idx = (gpu_device.requests_idx % queue_size);
   }

   driver_idx = gpu_device.vq->driver->idx;
   gpu_device.vq->driver->ring[driver_idx % queue_size] = first_request_idx;
   gpu_device.vq->driver->idx += 1;

   uint32_t *qnotify = (uint32_t *)gpu_device.notify;
   *qnotify = 0;

   // now wait for the request to complete
   lookahead_idx = gpu_device.acknowledged_idx;
   int request_complete = 0;
   while(!(request_complete))
   {
      if(lookahead_idx != gpu_device.vq->device->idx)
      {
         if(gpu_device.vq->device->ring[lookahead_idx % queue_size].id ==
                                                       first_request_idx)
         {
            // DEBUGMSG("request satisfied");
            request_complete = 1;
         }
         lookahead_idx += 1;
      }
   }


   return 0;
}

int gpu_get_display_info(gpu_get_display_info_response_t* disp_data)
{
   gpu_control_header_t *header;

   header = kcalloc(sizeof(gpu_control_header_t));
   MALLOC_CHECK(header);

   header->control_type = GPU_CMD_GET_DISPLAY_INFO;
   header->flags        = 0;
   header->fence_id     = 0;
   header->context_id   = 0;


   int retval;
   retval = gpu_request_read_poll(header,    sizeof(gpu_control_header_t),
                        NULL, 0,
                        disp_data, sizeof(gpu_get_display_info_response_t)
                        );
   kfree(header);
   if(retval != 0)
   {
      return retval;
   }

   if(disp_data->header.control_type != GPU_RESP_OK_DISPLAY_INFO)
   {
      ERRORMSG("GPU not returning display info");
      return 5;
   }
   if(!disp_data->displays[0].enabled)
   {
      ERRORMSG("display not enabled");
      return 1;
   }


   return 0;
}

int gpu_init(void)
{
   gpu_get_display_info_response_t *gpu_info;
   gpu_control_header_t *response_header; // for responses
   gpu_resource_create_2d_request_header_t *resource_request_header;
   gpu_resource_attach_backing_header_t *attach_backing_header;
   gpu_memory_entry_t *memory_entry;
   gpu_set_scanout_header_t *set_scanout_header;
   int fb_size;
   int active;

   mutex_wait(&gpu_device.gpu_mutex);
   active = gpu_device.active;
   if(active == 0)
   {
      gpu_device.active = 1;
   }
   else
   {
      ERRORMSG("init-ing an active GPU");
   }
   mutex_post(&gpu_device.gpu_mutex);
   if(active != 0)
   {
      return 10;
   }


   response_header = kcalloc(sizeof(gpu_control_header_t));
   MALLOC_CHECK(response_header);
   gpu_info = kcalloc(sizeof(gpu_get_display_info_response_t));
   MALLOC_CHECK(gpu_info);

   // 1: get display info
   if(gpu_get_display_info(gpu_info))
   {
      return 1;
   }

   // and use the display info to finish initializing gpu_device parametres
   gpu_device.fb.width  = gpu_info->displays[0].rectangle.width;
   gpu_device.fb.height = gpu_info->displays[0].rectangle.height;
   fb_size = sizeof(pixel_t) * gpu_device.fb.width * gpu_device.fb.height;
   gpu_device.fb.pixels = kcalloc(fb_size);
   MALLOC_CHECK(gpu_device.fb.pixels);
   // no need to free the gpu_device's frame buffer unless the GPU resizes
   kfree(gpu_info);

   // TODO: consider making more of these into functions like the first one
   //  OR just removing the first function
   // 2: create a 2D resource
   // make frame buffer for display 0
   resource_request_header = kcalloc(
                        sizeof(gpu_resource_create_2d_request_header_t));
   MALLOC_CHECK(resource_request_header);

   resource_request_header->header.control_type = GPU_CMD_RESOURCE_CREATE_2D;
   resource_request_header->header.flags        = 0;
   resource_request_header->header.fence_id     = 0;
   resource_request_header->header.context_id   = 0;
   resource_request_header->format      = R8G8B8A8_UNORM;
   resource_request_header->height      = gpu_device.fb.height;
   resource_request_header->width       = gpu_device.fb.width;
   resource_request_header->resource_id = 1;

   if(gpu_request_write_poll(resource_request_header,
                             sizeof(gpu_resource_create_2d_request_header_t),
                             NULL, 0,
                             response_header, sizeof(gpu_control_header_t)))
   {
      ERRORMSG("2D resource setup error");
      return 2;
   }
   if(response_header->control_type != GPU_RESP_OK_NODATA)
   {
      ERRORMSG("2D resource setup response blank");
      return 2;
   }
   kfree(resource_request_header);
   response_header->control_type = 0; // will be resused: reset for error check

   // 3: Attach 2D resource
   attach_backing_header = kcalloc(
                        sizeof(gpu_resource_attach_backing_header_t));
   MALLOC_CHECK(attach_backing_header);

   attach_backing_header->header.control_type = GPU_CMD_RESOURCE_ATTACH_BACKING;
   attach_backing_header->header.flags        = 0;
   attach_backing_header->header.fence_id     = 0;
   attach_backing_header->header.context_id   = 0;
   attach_backing_header->resource_id = 1;
   attach_backing_header->num_entries = 1;

   memory_entry = kcalloc(sizeof(gpu_memory_entry_t));
   MALLOC_CHECK(memory_entry);
   uint64_t fb_vaddr = (uint64_t)gpu_device.fb.pixels;
   memory_entry->paddr   = mmu_translate(kernel_mmu_table, fb_vaddr);
   memory_entry->length  = fb_size;
   memory_entry->padding = 0; // we don't play games with endianness

   if(gpu_request_write_poll(attach_backing_header,
                             sizeof(gpu_resource_attach_backing_header_t),
                             memory_entry, sizeof(gpu_memory_entry_t),
                             response_header, sizeof(gpu_control_header_t)))
   {
      ERRORMSG("attach backing");
      return 2;
   }
   if(response_header->control_type != GPU_RESP_OK_NODATA)
   {
      ERRORMSG("attach backing response blank");
      return 2;
   }
   kfree(attach_backing_header);
   kfree(memory_entry);
   response_header->control_type = 0; // resused: reset for error check
   
   // 4: set scanout and connect to the resource
   set_scanout_header = kcalloc(sizeof(gpu_set_scanout_header_t));
   MALLOC_CHECK(set_scanout_header);
   set_scanout_header->header.control_type = GPU_CMD_SET_SCANOUT;
   set_scanout_header->header.flags        = 0;
   set_scanout_header->header.fence_id     = 0;
   set_scanout_header->header.context_id   = 0;
   set_scanout_header->rectangle.x       = 0;
   set_scanout_header->rectangle.y       = 0;
   set_scanout_header->rectangle.width   = gpu_device.fb.width;
   set_scanout_header->rectangle.height  = gpu_device.fb.height;
   set_scanout_header->scanout_id    = 0;
   set_scanout_header->resource_id   = 1;
   if(gpu_request_write_poll(set_scanout_header,
                             sizeof(gpu_set_scanout_header_t),
                             NULL, 0,
                             response_header, sizeof(gpu_control_header_t)))
   {
      ERRORMSG("set scanout");
      return 3;
   }
   if(response_header->control_type != GPU_RESP_OK_NODATA)
   {
      ERRORMSG("set scanout response blank");
      return 3;
   }
   kfree(set_scanout_header);
   kfree(response_header);


   // 5: draw something to the screen
   // gpu_rectangle_t first_r = {1,1,100,100};
   // pixel_t first_c = {255,0,0,255};
   // gpu_fill_rectangle(&(gpu_device.fb), &first_r, first_c);
   // gpu_rectangle_t screen_r;
   // screen_r.x = 0;
   // screen_r.y = 0;
   // screen_r.width = gpu_device.fb.width;
   // screen_r.height= gpu_device.fb.height;
   // vfs_bmp_file_to_gpu(v_dir_root, "joeyD.bmp", &screen_r);

   if(gpu_fill_with_bmp(&(gpu_device.fb)))
   {
      // then find something else to fill with
      gpu_rectangle_t first_r = {1,1,100,100}; // you're kidding. This works?
      pixel_t first_c = {255,0,0,255};
      gpu_fill_rectangle(&(gpu_device.fb), &first_r, first_c);
      // office horus moment?
   }

   // 6 and 7: in this function
   if(gpu_transfer_and_flush())
   {
      return 6;
   }

   return 0;
}

// steps 6 and 7 from init
int gpu_transfer_and_flush(void)
{
   gpu_rectangle_t all;
   all.x = 0;
   all.y = 0;
   all.width  = gpu_device.fb.width;
   all.height = gpu_device.fb.height;
   return gpu_transfer_and_flush_some(&all);
}

int gpu_transfer_and_flush_some(gpu_rectangle_t *r)
{
   gpu_control_header_t *response_header; // for responses
   gpu_transfer_to_host_2d_header_t *transfer_2d_header;
   gpu_resource_flush_header_t *flush_header;
   response_header = kcalloc(sizeof(gpu_control_header_t));
   MALLOC_CHECK(response_header);

   uint64_t copy_offset = (r->y * gpu_device.fb.width + r->x) * sizeof(pixel_t);
   // 6: transfer the framebuffer to the host
   transfer_2d_header = kcalloc(sizeof(gpu_transfer_to_host_2d_header_t));
   MALLOC_CHECK(transfer_2d_header);
   transfer_2d_header->header.control_type = GPU_CMD_TRANSFER_TO_HOST_2D;
   transfer_2d_header->header.flags        = 0;
   transfer_2d_header->header.fence_id     = 0;
   transfer_2d_header->header.context_id   = 0;
   transfer_2d_header->rectangle.x       = r->x;
   transfer_2d_header->rectangle.y       = r->y;
   transfer_2d_header->rectangle.width   = r->width;
   transfer_2d_header->rectangle.height  = r->height;
   transfer_2d_header->offset        = copy_offset;
   transfer_2d_header->resource_id   = 1;
   transfer_2d_header->padding       = 0;

   if(gpu_request_write_poll(transfer_2d_header,
                             sizeof(gpu_transfer_to_host_2d_header_t),
                             NULL, 0,
                             response_header, sizeof(gpu_control_header_t)))
   {
      ERRORMSG("transfer 2d");
      return 6;
   }
   if(response_header->control_type != GPU_RESP_OK_NODATA)
   {
      ERRORMSG("transfer 2d response blank");
      return 6;
   }
   kfree(transfer_2d_header);
   response_header->control_type = 0; // resused: reset for error check


   // 7: flush the GPU data (causing GPU to draw)
   flush_header = kcalloc(sizeof(gpu_resource_flush_header_t));
   MALLOC_CHECK(transfer_2d_header);
   flush_header->header.control_type = GPU_CMD_RESOURCE_FLUSH;
   flush_header->header.flags        = 0;
   flush_header->header.fence_id     = 0;
   flush_header->header.context_id   = 0;
   flush_header->rectangle.x       = r->x;
   flush_header->rectangle.y       = r->y;
   flush_header->rectangle.width   = r->width;
   flush_header->rectangle.height  = r->height;
   flush_header->resource_id   = 1;
   flush_header->padding       = 0;

   if(gpu_request_write_poll(flush_header,
                             sizeof(gpu_resource_flush_header_t),
                             NULL, 0,
                             response_header, sizeof(gpu_control_header_t)))
   {
      ERRORMSG("resource flush");
      return 7;
   }
   if(response_header->control_type != GPU_RESP_OK_NODATA)
   {
      ERRORMSG("resource flush response blank");
      return 7;
   }
   kfree(flush_header);
   kfree(response_header);

   mutex_wait(&gpu_device.gpu_mutex);
   gpu_device.active = 0;
   mutex_post(&gpu_device.gpu_mutex);
   
   return 0;
}

int gpu_fill_with_block_device_and_flush(void)
{
   gpu_fill_with_block_device(&(gpu_device.fb));
   return gpu_transfer_and_flush();
}

int gpu_fill_with_bmp_and_flush(void)
{
   gpu_fill_with_bmp(&(gpu_device.fb));
   return gpu_transfer_and_flush();
}

uint32_t gpu_get_fb_row(uint64_t offset)
{
   return (offset / gpu_device.fb.width);
}

uint32_t gpu_get_fb_column(uint64_t offset)
{
   return (offset % gpu_device.fb.width);
}

uint64_t gpu_get_fb_offset(uint32_t row, uint32_t column)
{
   if(row >= gpu_device.fb.height)
   {
      return 0xffffffff;
   }
   if(column >= gpu_device.fb.width)
   {
      return 0xffffffff;
   }
   return (row*gpu_device.fb.width + column);
}

uint64_t *gpu_get_fb_offset_into_rectangle(uint32_t row, uint32_t max_row,
                                           uint32_t column, uint32_t max_column,
                                           gpu_rectangle_t *r)
{
   uint64_t gpu_offset, *gpu_offsets;
   int horizontal_num_pixels;
   int vertical_num_pixels;
   // TODO: super TODO: revisit this barbaric method of scaling up */
   horizontal_num_pixels = ((r->width / max_row) + 1);
   vertical_num_pixels = ((r->height / max_column) + 1);
   int len_pixels = horizontal_num_pixels * vertical_num_pixels;
   gpu_offsets = kmalloc((len_pixels + 1) * sizeof(uint64_t));
   int i, j;
   for(i = 0; i < horizontal_num_pixels; i++)
   {
      for(j = 0; j < vertical_num_pixels; j++)
      {
         gpu_offset = r->y * gpu_device.fb.width + r->x; // base
         gpu_offset += (r->height * row / max_row) * gpu_device.fb.width;
         gpu_offset += (r->width  * column / max_column);
         gpu_offset += i;
         gpu_offset += j*gpu_device.fb.width;
         if(gpu_offset >= gpu_device.fb.width * gpu_device.fb.height)
         {
            // TODO:
            // DEBUGMSG("you need to fix this");
            gpu_offset = gpu_device.fb.width * gpu_device.fb.height - 1;
         }
         gpu_offsets[i + j*horizontal_num_pixels] = gpu_offset;
      }
   }
   gpu_offsets[i + j*horizontal_num_pixels] = 0xffffffffffffffff;

   return gpu_offsets;
}

void gpu_circle_to_rectangle(gpu_rectangle_t *fill, gpu_circle_t *src)
{
   gpu_rectangle_t r;
   r.width = 0;
   r.height = 0;
   /* boundaries with top and right */
   if(src->x < src->radius)
   {
      r.width += src->x;
      r.x = 0;
   }
   else
   {
      r.width += src->radius;
      r.x = src->x - src->radius;
   }
   if(src->y < src->radius)
   {
      r.height += src->y;
      r.y = 0;
   }
   else
   {
      r.height += src->radius;
      r.y = src->y - src->radius;
   }

   /* boundaries with bottom and left */
   if(src->y + src->radius > gpu_device.fb.height)
   {
      r.height = gpu_device.fb.height - r.y;
   }
   else
   {
      r.height += src->radius;
   }
   if(src->x + src->radius > gpu_device.fb.width)
   {
      r.width = gpu_device.fb.width - r.x;
   }
   else
   {
      r.width += src->radius;
   }
   *fill = r;
   return;
}

void gpu_fill_rectangle(volatile gpu_frame_buffer_t *fb,
                        gpu_rectangle_t *r,
                        pixel_t color)
{
   // note: the 'bottom' as it appears on the screen
   // has higher y index than the top. 0,0 is top left!
   uint32_t r_left  = r->x;
   uint32_t r_right = r->x + r->width;
   uint32_t r_top   = r->y;
   uint32_t r_bot   = r->y + r->height;

   uint32_t row, col, fb_offset;

   for(row = r_top; row <= r_bot; row++)
   {
      for(col = r_left; col <= r_right; col++)
      {
         fb_offset = fb->width * row   +  col;
         fb->pixels[fb_offset] = color;
      }
   }
}

void gpu_fill_circle(volatile gpu_frame_buffer_t *fb,
                     gpu_circle_t *c,
                     pixel_t color)
{
   uint32_t c_left  = c->x - c->radius;
   uint32_t c_right = c->x + c->radius;
   uint32_t c_top   = c->y - c->radius;
   uint32_t c_bot   = c->y + c->radius;

   uint32_t row, col, fb_offset;
   int32_t x_off, y_off;
   uint32_t x_off2, y_off2, diag;
   for(row = c_top; row <= c_bot; row++)
   {
      for(col = c_left; col <= c_right; col++)
      {
         x_off = col-c->x;
         y_off = row-c->y;
         x_off2 = x_off * x_off;
         y_off2 = y_off * y_off;
         diag = isqrt(x_off2 + y_off2);
         if(diag < c->radius)
         {
            fb_offset = fb->width * row   +  col;
            fb->pixels[fb_offset] = color;
         }
      }
   }
}

void gpu_fill_white(volatile gpu_frame_buffer_t *fb)
{
   memset(fb->pixels, 0xff, fb->width*fb->height*4);
   return;
}

// TODO: I'm an old function that might be able to be purged freely
void gpu_fill_with_block_device(volatile gpu_frame_buffer_t *fb)
{
   char *buffer;
   buffer = kcalloc(512*100);

   block_read_poll(0, buffer, 0, 512*100);

   int i;
   for(i = 0; i < 512*100; i++)
   {
      ((char*)fb->pixels)[i] = buffer[i];
   }


   kfree(buffer);
}

// read from block device and assume it's a bmp file
int gpu_fill_with_bmp(volatile gpu_frame_buffer_t *fb)
{
   char *buffer;
   int bsz; // bsz: block size
   int which_dev; // which block device
   bmp_img_header_t *bh;
   bmp_img_info_header_t *bmp_info;
   for(which_dev = 0; which_dev < num_block_devices; which_dev++)
   {
      bsz = block_devices[which_dev].block_specific->block_size;
      buffer = kcalloc(bsz);
      MALLOC_CHECK(buffer);
      block_read_poll(which_dev, buffer,0, bsz);
      bh = (bmp_img_header_t *) buffer;
      bmp_info = ((bmp_img_info_header_t *) (buffer+sizeof(bmp_img_header_t)));

      if(bh->identifier == BMP_HEADER_MAGIC)
      {
         break;
      }
      else
      {
         kfree(buffer);
         // keep looking
      }
   }
   if(which_dev == num_block_devices)
   {
      printf("GPU: looks like none of your hard drives are bmp files-\n");
      printf("   I'm giving you the default background\n");
      return 1;
   }

   int offset_to_pixels = 54; // TODO: programmatically determine
   int bmp_width  = bmp_info->image_width;
   int bmp_height = bmp_info->image_height;
   // TODO: add file size error check
   // uint64_t file_size = bmp_info->image_size_uc;
   // printf("file claims to be %u bytes", file_size);
   if(bmp_info->compression_method != BMP_COMPRESSION_METHOD_RGB)
   {
      ERRORMSG("unsupported compression type");
   }
   if(offset_to_pixels >= bsz)
   {
      ERRORMSG("unsupported large header");
   }

   uint64_t buffer_index, block_index, which_pixel, max_pixel;
   uint32_t pixel_row, pixel_col;
   pixel_t pixel_to_gpu;
   uint64_t gpu_fb_offset;
   // bmp_index will be used to track where a pixel is in the array
   which_pixel = 0;
   max_pixel = bmp_width*bmp_height;

   // don't just do one at a time: that seems to cause problems: get many.
   bsz *= 10;
   kfree(buffer);
   buffer = kcalloc(bsz);
   MALLOC_CHECK(buffer);
   uint64_t which_block = 0;
   // reload it (redundant)
   block_read_poll(which_dev, buffer, which_block*bsz, which_block*bsz+bsz);
   for(which_pixel = 0; which_pixel < max_pixel; which_pixel++)
   {
      block_index = offset_to_pixels + which_pixel * 3;

      // b
      if(!(block_index % bsz)) // crossed into new block boundary
      {
         which_block+=1;
         block_read_poll(which_dev,buffer,which_block*bsz,which_block*bsz+bsz);
      }
      buffer_index = block_index % bsz;
      pixel_to_gpu.b = buffer[buffer_index];
      block_index+=1;


      // g
      if(!(block_index % bsz)) // crossed into new block boundary
      {
         which_block+=1;
         block_read_poll(which_dev,buffer,which_block*bsz,which_block*bsz+bsz);
      }
      buffer_index = block_index % bsz;
      pixel_to_gpu.g = buffer[buffer_index];
      block_index+=1;

      // r
      if(!(block_index % bsz)) // crossed into new block boundary
      {
         which_block+=1;
         block_read_poll(which_dev,buffer,which_block*bsz,which_block*bsz+bsz);
      }
      buffer_index = block_index % bsz;
      pixel_to_gpu.r = buffer[buffer_index];


      // a
      pixel_to_gpu.a = 0xff;

      // it's mirrored vertically. Don't ask me why...
      pixel_row = (max_pixel-which_pixel-1) / bmp_width;
      pixel_col = (which_pixel) % bmp_width;
      gpu_fb_offset = gpu_get_fb_offset(pixel_row, pixel_col);
      if (gpu_fb_offset == 0xffffffff)
      {
         // DEBUGMSG("ingoring pixel");
         // printf("row %d col %d\n", pixel_row, pixel_col);
         continue;
      }
      fb->pixels[gpu_fb_offset] = pixel_to_gpu;
   }


   kfree(buffer);
   return 0;
}


void gpu_irq_handler(void)
{
   if(gpu_device.isr->queue_interrupt)
   {
      // DEBUGMSG("gpu IRQ confirmed");
      if(gpu_device.gpu_specific->events_read != 0)
      {
         gpu_device.gpu_specific->events_clear = 1;
         // we need to re-init the frame buffer
         if(gpu_device.fb.pixels == NULL) // not initialized yet
         {
            gpu_init();
         }
         else
         {
            DEBUGMSG("resizing GPU");
            int active;
            mutex_wait(&(gpu_device.gpu_mutex));
            active = gpu_device.active;
            if(active == 0)
            {
               gpu_device.active = 1;
            }
            else
            {
               ERRORMSG("interrupting an active GPU");
            }
            mutex_post(&(gpu_device.gpu_mutex));
            active = 0;
            if(active == 0)
            {
               printf("re-init-ing gpu\n");
               // re-init
               gpu_resource_unref_header_t *unref_header;
               gpu_control_header_t        *r_header;
               unref_header = kcalloc(sizeof(gpu_resource_unref_header_t));
               MALLOC_CHECK(unref_header);
               r_header = kcalloc(sizeof(gpu_control_header_t));
               MALLOC_CHECK(r_header);
            
               unref_header->header.control_type = GPU_CMD_RESOURCE_UNREF;
               unref_header->header.flags        = 0;
               unref_header->header.fence_id     = 0;
               unref_header->header.context_id   = 0;
               unref_header->resource_id = 1;
               unref_header->padding     = 0;
            
               if(gpu_request_write_poll(unref_header,
                                       sizeof(gpu_resource_unref_header_t),
                                       NULL, 0,
                                       r_header, sizeof(gpu_control_header_t)))
               {
                  ERRORMSG("2D resource setup error");
                  printf("won't return though\n");
                  // return;
               }
               if(r_header->control_type != GPU_RESP_OK_NODATA)
               {
                  ERRORMSG("2D resource setup response blank");
                  printf("won't return though\n");
                  // return;
               }
               kfree(unref_header);
               kfree(r_header);
   
               kfree(gpu_device.fb.pixels);
               mutex_wait(&(gpu_device.gpu_mutex));
               gpu_device.active = 0;
               mutex_post(&(gpu_device.gpu_mutex));
               gpu_init();
            }
            mutex_wait(&(gpu_device.gpu_mutex));
            gpu_device.active = 0;
            mutex_post(&(gpu_device.gpu_mutex));
         }
      }
      // don't just add 1. You might be servicing more than 1!
      gpu_device.acknowledged_idx = gpu_device.vq->device->idx;;
   }
   return;
}

uint32_t gpu_x_pixel_to_percent(uint32_t x_pixel)
{
   return (x_pixel*100/gpu_device.fb.width);
}

uint32_t gpu_y_pixel_to_percent(uint32_t y_pixel)
{
   return (y_pixel*100/gpu_device.fb.height);
}

uint32_t gpu_percent_to_x_pixel(uint32_t percent)
{
   return (percent*gpu_device.fb.width/100);
}

uint32_t gpu_percent_to_y_pixel(uint32_t percent)
{
   return (percent*gpu_device.fb.height/100);
}


typedef enum
{
   cursor_tracker_init,
   cursor_tracker_default,
   cursor_tracker_lclick,
   cursor_tracker_xpos,
   cursor_tracker_ypos,
   cursor_tracker_expect_sync,
} cursor_tracker_states_t;

void gpu_regions_test(void)
{
   int number_to_draw = global_input_events_ring_buffer->occupied;
   input_event_t i_event;
   int i;
   input_cursor_position_t icp;
   cursor_tracker_states_t state = cursor_tracker_default;
   for(i = 0; i < number_to_draw; i++)
   {
      i_event = ie_ring_buffer_pop(global_input_events_ring_buffer);
      // input_print_event(&i_event);
      switch(state)
      {
      case cursor_tracker_init:
         if(i_event.type == IE_TYPE_SYNC)
         {
            ;
         }
         else if(i_event.type == IE_TYPE_TABLET)
         {
            if(i_event.code == IE_TABLET_CODE_X)
            {
               icp.x = i_event.value;
               state = cursor_tracker_xpos;
            }
            else
            {
               DEBUGMSG("skipping unexpected code");
            }
         }
         else
         {
            DEBUGMSG("unexpected event");
         }
         break;
      case cursor_tracker_default:
         if(i_event.type == IE_TYPE_SYNC)
         {
            ;
         }
         else if(i_event.type == IE_TYPE_TABLET)
         {
            if(i_event.code == IE_TABLET_CODE_X)
            {
               icp.x = i_event.value;
               state = cursor_tracker_xpos;
            }
            else
            {
               DEBUGMSG("skipping unexpected code");
               input_print_event(&i_event);
            }
         }
         else if(i_event.type == IE_TYPE_KEY)
         {
            if(i_event.code == IE_KEY_CODE_LMOUSE)
            {
               if(i_event.value == IE_KEY_VALUE_PRESS)
               {
                  uint32_t x_pos = icp.x * gpu_device.fb.width  / 0x8000;
                  uint32_t y_pos = icp.y * gpu_device.fb.height / 0x8000;
                  uint32_t x_pct = icp.x * 100  / 0x8000;
                  uint32_t y_pct = icp.y * 100 / 0x8000;
                  // printf("x pixel: %8u\ty:pixel %8u\n", x_pos, y_pos);
                  gpu_circle_t c;
                  c.x = x_pos;
                  c.y = y_pos;
                  c.radius = 5;
                  pixel_t first_c = {rand8(),rand8(),rand8(),255};
                  gpu_fill_circle(&(gpu_device.fb), &c, first_c);
                  gpu_rectangle_t r;
                  gpu_circle_to_rectangle(&r, &c);
                  gpu_transfer_and_flush_some(&r);
                  int region_value;
                  region_value = regions_value_of_point(global_regions_start,
                                                         x_pct, y_pct);
                  printf("region value: %d\n", region_value);
                  state = cursor_tracker_lclick;
               }
               else
               {
                  DEBUGMSG("mouse release");
               }
            }
            else
            {
               DEBUGMSG("other key press");
            }
         }
         else
         {
            DEBUGMSG("unexpected event");
         }
         break;
      case cursor_tracker_xpos:
         if(i_event.type == IE_TYPE_TABLET)
         {
            if(i_event.code == IE_TABLET_CODE_Y)
            {
               icp.y = i_event.value;
               state = cursor_tracker_ypos;
            }
            else
            {
               DEBUGMSG("skipping unexpected code");
            }
         }
         else
         {
            DEBUGMSG("unexpected event");
         }
         break;
      case cursor_tracker_ypos:
      case cursor_tracker_lclick:
      case cursor_tracker_expect_sync:
         if(i_event.type == IE_TYPE_SYNC)
         {
            state = cursor_tracker_default;
         }
         else
         {
            DEBUGMSG("unexpected event (expected sync)");
         }
         break;
      default:
         ERRORMSG("unknown state");
         break;
      }
   }
   return;
}


void gpu_draw_from_cursor(void)
{
   int number_to_draw = 150;
   input_event_t i_event[number_to_draw];
   int i;
   for(i = 0; i < number_to_draw; i++)
   {
      i_event[i] = ie_ring_buffer_pop(global_input_events_ring_buffer);
   }
   input_cursor_position_t icp;
   icp.x = 0;
   icp.y = 0;
   int valid_events_seen = 0;
   for(i = 0; i < number_to_draw; i++)
   {
      if(i_event[i].type == IE_TYPE_TABLET)
      {
         if (i_event[i].code == IE_TABLET_CODE_X)
         {
            icp.x = i_event[i].value;
            valid_events_seen++;
         }
         if (i_event[i].code == IE_TABLET_CODE_Y)
         {
            icp.y = i_event[i].value;
            valid_events_seen++;
         }
      }
      if(i_event[i].type == IE_TYPE_SYNC)
      {
         if(valid_events_seen != 2)
         {
            // ERRORMSG("have not seen enough valid events yet");
         }
         else
         {
            // printf("input cursor event read:\n");
            // printf("x: 0x%08x\ty: 0x%08x\n", icp.x, icp.y);
            uint32_t x_pos = icp.x * gpu_device.fb.width  / 0x8000;
            uint32_t y_pos = icp.y * gpu_device.fb.height / 0x8000;
            // printf("x pixel: %8u\ty:pixel %8u\n", x_pos, y_pos);
            gpu_circle_t c;
            c.x = x_pos;
            c.y = y_pos;
            c.radius = 5;
            pixel_t first_c = {255,0,0,255};
            gpu_fill_circle(&(gpu_device.fb), &c, first_c);
            gpu_rectangle_t r;
            gpu_circle_to_rectangle(&r, &c);
            gpu_transfer_and_flush_some(&r);
         }
         valid_events_seen = 0;
      }
   }
   return;
}

void gpu_draw_endless(void)
{
   while(1)
   {
      if(global_input_events_ring_buffer->occupied >= 150)
      {
         // printf("drawing. Events: %d\n",
                  // global_input_events_ring_buffer->occupied);
         gpu_draw_from_cursor();
         // printf("done drawing. Events: %d\n",
                  // global_input_events_ring_buffer->occupied);
      }
   }
}

void gpu_print_gdi_response(gpu_get_display_info_response_t *gdir)
{
   gpu_rectangle_t gdir_rect;
   int i;
   for(i = 0; i < GPU_MAX_SCANOUTS; i++)
   {
      printf("GPU display %02d:", i);
      gdir_rect = gdir->displays[i].rectangle;
      printf("   x: %d\ty: %d\twidth: %3d\theight: %3d\n",
               gdir_rect.x, gdir_rect.y, gdir_rect.width, gdir_rect.height);
   }
   return;
}

void gpu_print_device_specific(volatile virtio_gpu_config_t *gcfg)
{
   printf("GPU device specific register at 0x%012lx\n",gcfg);
   printf("events read : 0x%08x\n", gcfg->events_read );
   printf("events clear: 0x%08x\n", gcfg->events_clear);
   printf("num scanouts: 0x%08x\n", gcfg->num_scanouts);
}

void gpu_print_virtq(void)
{
   int queue_size = gpu_device.common_config->queue_size;
   virtio_print_virtq_all(gpu_device.vq, queue_size);
}
