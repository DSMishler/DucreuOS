#include <ring.h>
#include <stdint.h>
#include <input.h>
#include <kmalloc.h>
#include <utils.h>
#include <printf.h>
#include <input.h>

mutex_t ie_ring_mutex = MUTEX_LOCKED;

ie_ring_buffer_t *ie_ring_buffer_init(uint32_t size, uint32_t behavior)
{
   ie_ring_buffer_t *rb;
   rb = kmalloc(sizeof(ie_ring_buffer_t));
   MALLOC_CHECK(rb);
   rb->mutex    = MUTEX_LOCKED;
   rb->start    = 0;
   rb->occupied = 0;
   rb->capacity = size;
   rb->behavior = behavior;
   rb->elements = kmalloc(sizeof(input_event_t)*size);
   MALLOC_CHECK(rb->elements);
   
   mutex_post(&(rb->mutex));
   mutex_post(&ie_ring_mutex);

   return rb;
}

int ie_ring_buffer_push(ie_ring_buffer_t *rb, input_event_t ev)
{
   // mutex_wait(&(rb->mutex));
   // DEBUGMSG("pushing to ring buffer\n");
   // printf("   ring occupied: %d\n", rb->occupied);
   // input_print_event(&ev);
   if(rb->occupied == rb->capacity)
   {
      switch(rb->behavior)
      {
      case RB_OVERWRITE:
         rb->elements[(rb->start + rb->occupied) % rb->capacity] = ev;
         rb->start = (rb->start + 1) % rb->capacity;
         break;
      case RB_IGNORE:
         break;
      case RB_ASK_FOR_MANAGER:
         ERRORMSG("events ring buffer full");
         printf("    (fully clearing it manually)\n");
         // reset it for now
         rb->start = 0;
         rb->occupied = 0;
         mutex_post(&(rb->mutex));
         return 1;
         break;
      default:
         ERRORMSG("unknown behavior");
         mutex_post(&(rb->mutex));
         return 2;
      }
   }
   else
   {
      rb->elements[(rb->start + rb->occupied) % rb->capacity] = ev;
      rb->occupied += 1;
   }
   // mutex_post(&(rb->mutex));
   return 0;
}

input_event_t ie_ring_buffer_pop(ie_ring_buffer_t *rb)
{
   // mutex_wait(&(rb->mutex));
   input_event_t ev = {0,0,0};
   if(rb->occupied == 0)
   {
      // DEBUGMSG("pop from empty ring buffer");
      // possible TODO: make a code for this as an input event
      mutex_post(&(rb->mutex));
      return ev;
   }
   // else
   ev = rb->elements[rb->start];
   rb->start = (rb->start + 1) % rb->capacity;
   rb->occupied -= 1;
   // mutex_post(&(rb->mutex));
   return ev;
}

void ie_ring_buffer_print(ie_ring_buffer_t *rb)
{
   // mutex_wait(&(rb->mutex));
   printf("ring buffer at 0x%012lx\n", (uint64_t)rb);
   printf("start:          %d\n", rb->start);
   printf("total elements: %d\n", rb->occupied);
   printf("capacity:       %d\n", rb->capacity);
   printf("full policy:    %d\n", rb->behavior);
   uint32_t i, modi, num;
   for(i = rb->start, num = 0; i < rb->start + rb->occupied; i++, num++)
   {
      modi = i % rb->capacity;
      printf("%03d: ring[%03d]: ", num, modi);
      input_print_event(rb->elements+modi);
   }
   // mutex_post(&(rb->mutex));
   return;
}
