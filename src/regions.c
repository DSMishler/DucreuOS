#include <regions.h>
#include <kmalloc.h>
#include <utils.h>
#include <printf.h>

region_start_t *global_regions_start;

void regions_init(void)
{
   global_regions_start = kmalloc(sizeof(region_start_t));
   global_regions_start->head = NULL;
}

int regions_list_len(region_start_t *list)
{
   int i = 0;
   region_llist_t *rl = list->head;
   for(; rl != NULL; rl = rl->next)
   {
      ;
   }
   return i;
}

/* note: this will return NULL if you're over the length */
region_llist_t *regions_get_at_pos(region_start_t *list, int pos)
{
   if(pos >= regions_list_len(list))
   {
      // ERRORMSG("list too short");
      return NULL;
   }
   int i;
   region_llist_t *rl = list->head;
   for(i = 0; i < pos; i++)
   {
      rl = rl->next;
   }
   return rl;
}

/* insert a region right before the region at 'pos' */
int regions_insert_at_position(region_start_t *list,
                              region_llist_t *insert,
                              int pos)
{
   if(pos > regions_list_len(list))
   {
      ERRORMSG("list too short");
      return 1;
   }
   if(pos == 0)
   {
      insert->next = list->head;
      list->head = insert;
      return 0;
   }
   region_llist_t *prev;
   region_llist_t *next;
   prev = regions_get_at_pos(list, pos-1);
   next = regions_get_at_pos(list, pos);
   prev->next = insert;
   insert->next = next;
   return 0;
}

int regions_append(region_start_t *list, region_llist_t *insert)
{
   return (regions_insert_at_position(list, insert, regions_list_len(list)));
}

int regions_prepend(region_start_t *list, region_llist_t *insert)
{
   return (regions_insert_at_position(list, insert, 0));
}


region_llist_t *regions_new(uint32_t x_start, uint32_t x_end,
                            uint32_t y_start, uint32_t y_end, int value)
{
   region_llist_t *new;
   new = kmalloc(sizeof(region_llist_t));
   new->x_start = x_start;
   new->x_end   = x_end;
   new->y_start = y_start;
   new->y_end   = y_end;
   new->value   = value;
   new->next = NULL;
   return new;
}


int regions_pixel_in_range(region_llist_t *rl, uint32_t x, uint32_t y)
{
   if(x < rl->x_start)
   {
      return 0;
   }
   if(x > rl->x_end)
   {
      return 0;
   }
   if(y < rl->y_start)
   {
      return 0;
   }
   if(y > rl->y_end)
   {
      return 0;
   }
   return 1;
}

/* returns the value of the *first* region that matches */
int regions_value_of_point(region_start_t *list, uint32_t x, uint32_t y)
{
   region_llist_t *rl;
   rl = list->head;
   for(; rl != NULL; rl = rl->next)
   {
      if(regions_pixel_in_range(rl, x, y))
      {
         return rl->value;
      }
   }
   return 0;
}


void regions_print_list(region_start_t *list)
{
   printf("regions llist:");
   region_llist_t *rl = list->head;
   for(; rl != NULL; rl = rl->next)
   {
      printf("region at 0x%012lx\n", rl);
      printf("vertical  : [%03u,%03u]\n", rl->y_start, rl->y_end);
      printf("horizontal: [%03u,%03u]\n", rl->x_start, rl->x_end);
      printf("value: %d\n", rl->value);
      printf("next region at 0x%012lx\n", rl->next);
      printf("\n");
   }
   return;
}
