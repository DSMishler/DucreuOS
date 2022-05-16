#pragma once
#include <stdint.h>

// utils for loading bitmapped images to and from GPU


#define BMP_HEADER_MAGIC   0x4d42 /* ASCII for 'B''M' (little endian) */
typedef struct bmp_img_header
{
   uint16_t identifier; // must be 0x4e4d
   uint16_t size_lower;
   uint16_t size_upper;
   uint16_t padding1;
   uint16_t padding2;
   uint16_t offset_to_pixels_lower;
   uint16_t offset_to_pixels_upper;
} bmp_img_header_t;

#define BMP_COMPRESSION_METHOD_RGB 0
typedef struct bmp_img_WINDOWS_BITMAPINFOHEADER
{
   uint32_t size; // size of *this header* (which should be 40)
   uint32_t image_width;
   uint32_t image_height;
   uint16_t num_color_panes; // must be 1
   uint16_t bits_per_pixel;
   uint32_t compression_method; // 0 is RGB
   /* uncompressed image size. could be sentinel 0 for RGB */
   uint32_t image_size_uc; 
   uint32_t horizontal_resolution;
   uint32_t vertical_resolution;
   uint32_t num_colors;
   uint32_t important_colors; // generally ignored
} bmp_img_info_header_t;



void print_bmp_header(bmp_img_header_t *bh);
void print_bmp_info_header(bmp_img_info_header_t *bh);
