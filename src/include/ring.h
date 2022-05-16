#pragma once

#include <stdint.h>
#include <input.h>
#include <lock.h>

enum ring_buffer_behaviors
{
   RB_OVERWRITE,         // overwrite old input with new input
   RB_IGNORE,            // ignore new input
   RB_ASK_FOR_MANAGER    // give an error message
};

typedef struct input_events_ring_buffer
{
   uint32_t start;
   uint32_t occupied;
   uint32_t capacity;
   uint32_t behavior;
   input_event_t *elements;// doesn't have to be physically contiguous
   mutex_t mutex;          // TODO: this mutex might be extraneous
} input_events_ring_buffer_t;
typedef input_events_ring_buffer_t ie_ring_buffer_t;



extern ie_ring_buffer_t *global_input_events_ring_buffer;
extern mutex_t ie_ring_mutex;

ie_ring_buffer_t *ie_ring_buffer_init(uint32_t size,uint32_t behavior);
int ie_ring_buffer_push(ie_ring_buffer_t *rb, input_event_t ev);
input_event_t ie_ring_buffer_pop(ie_ring_buffer_t *rb);
void ie_ring_buffer_print(ie_ring_buffer_t *rb);
