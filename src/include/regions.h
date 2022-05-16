#pragma once
#include <gpu.h>


/* regions are defined as percentages of the screen */
typedef struct region_llist
{
   uint32_t x_start;
   uint32_t x_end;
   uint32_t y_start;
   uint32_t y_end;
   int value;
   struct region_llist *next;
} region_llist_t;

typedef struct region_start
{
   region_llist_t *head;
} region_start_t;

extern region_start_t *global_regions_start;

void regions_init(void);

int regions_list_len(region_start_t *list);
region_llist_t *regions_get_at_pos(region_start_t *list, int pos);
int regions_insert_at_position(region_start_t *list,
                              region_llist_t *insert,
                              int pos);
int regions_append(region_start_t *list, region_llist_t *insert);
int regions_prepend(region_start_t *list, region_llist_t *insert);


region_llist_t *regions_new(uint32_t x_start, uint32_t x_end,
                            uint32_t y_start, uint32_t y_end, int value);

int regions_pixel_in_range(region_llist_t *rl, uint32_t x, uint32_t y);
int regions_value_of_point(region_start_t *list, uint32_t x, uint32_t y);

void regions_print_list(region_start_t *list);

