#include <printf.h>
#include <bmp.h>
#include <stdint.h>


void print_bmp_header(bmp_img_header_t *bh)
{
   printf("bitmap image header at: 0x%012lx\n", (uint64_t)bh);
   printf("identifier:       0x%04x ('%c%c')\n", bh->identifier,
        *(char*)&(bh->identifier),*((char*)&(bh->identifier)+1));
   printf("size:             %6u\n", (bh->size_upper << 16) + bh->size_lower);
   printf("padding1:         0x%04x\n", bh->padding1);
   printf("padding2:         0x%04x\n", bh->padding2);
   printf("offset_to_pixels: %6u\n",
         (bh->offset_to_pixels_upper << 16) + bh->offset_to_pixels_lower);
}

void print_bmp_info_header(bmp_img_info_header_t *bh)
{
   printf("bitmap info image header at: 0x%012lx\n", (uint64_t)bh);
   printf("header size:        %6u\n", bh->size);
   printf("image width:        %6u\n", bh->image_width);
   printf("image height:       %6u\n", bh->image_height);
   printf("num_color_panes:    %6u\n", bh->num_color_panes);
   printf("bits_per_pixel:     %6u\n", bh->bits_per_pixel);
   printf("compression_method: %6u\n", bh->compression_method);
   printf("image_size_uc:      %6u\n", bh->image_size_uc);
   printf("horizontal_res:     %6u\n", bh->horizontal_resolution);
   printf("vertical_res:       %6u\n", bh->vertical_resolution);
   printf("num_colors:         %6u\n", bh->num_colors);
   printf("important_colors:   %6u\n", bh->important_colors);
}
