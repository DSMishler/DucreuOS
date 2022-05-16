#pragma once
/* matches the OS copies of these structs */
typedef struct gpu_rectangle
{
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} gpu_rectangle_t;

typedef struct gpu_circle
{
   uint32_t x;       // higher index = right horizontally
   uint32_t y;       // higher index = lower vertically
   uint32_t radius;
} gpu_circle_t;

typedef struct pixel
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
} pixel_t;
