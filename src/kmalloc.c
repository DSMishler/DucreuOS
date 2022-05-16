#include <kmalloc.h>

#include <page.h>
#include <printf.h>
#include <utils.h>
#include <mmu.h>
#include <lock.h>
#include <stdint.h>
#include <stddef.h>

uint64_t kernel_heap_vaddr = KERNEL_HEAP_START_VADDR;
mutex_t kmalloc_lock = MUTEX_LOCKED;

allocation_t *kmalloc_head;

// Possible TODO: put these in utils.h and make them general
// allocation_ll_insert: insert node b after node a in a doubly linked list
void allocation_dll_insert(allocation_t *a, allocation_t *b)
{
   if(a == b)
   {
      printf("you're smokin' ciggies, man. You called allocation_ll_insert ");
      printf("with a=0x%08x, b=0x%08x\n", a, b);
      return;
   }
   // set up b's pointers
   b->next = a->next;
   b->prev = a;

   // adjust the neighboring pointers to include b
   a->next = b;
   (b->next)->prev = b;
   return;
}

// remove a node from a doubly linked list
// NOTE: in a cyclic doubly linked list, this cannot remove the last node
void allocation_dll_delete(allocation_t *a)
{
   if(a->next == a)
   {
      printf("you're smokin' ciggies, man. You called allocation_ll_delete ");
      printf("but a=0x%012lx, a->next=0x%012lx\n", a, a->next);
      printf("it's the only node in the list!\n");
      return;
   }
   (a->next)->prev = a->prev;
   (a->prev)->next = a->next;
}


void init_kernel_heap(void)
{
   allocation_t *p;
   uint64_t paddr;

   p = (allocation_t *)page_zalloc();
   paddr = (uint64_t) p;

   mmu_map(kernel_mmu_table, kernel_heap_vaddr, paddr, PTEB_READ | PTEB_WRITE);
   // sfence: remove all TLB entries correspond to the Kernel (KERNEL_ASID)
   // thoughts: this might not be necessary, since we could simply just flush
   //           the virtual address
   asm volatile("sfence.vma zero, %0" : : "r"(KERNEL_ASID));

   kmalloc_head = (allocation_t *) kernel_heap_vaddr;
   kmalloc_head->size = 0; // the kmalloc head will never be alloc'ed to
   kmalloc_head->next = kmalloc_head;
   kmalloc_head->prev = kmalloc_head;
   // also fill up the page with our first free node
   p = kmalloc_head + 1; // isn't pointer arithmetic cool?
   p->size = PAGE_SIZE - 2*sizeof(allocation_t);
   allocation_dll_insert(kmalloc_head, p);
   kernel_heap_vaddr += PAGE_SIZE;
   mutex_post(&kmalloc_lock);
   return;
}

void * kmalloc(size_t bytes)
{
   return common_alloc(bytes, 0, 0);
}

void * kzalloc(size_t bytes)
{
   return common_alloc(bytes, 1, 0);
}


void * kcalloc(size_t bytes) // TODO: kcalloc lightly tested. Test more.
{
   return common_alloc(bytes, 1, 1);
}

void * common_alloc(size_t bytes, int do_zero, int request_contiguous)
{
   mutex_wait(&kmalloc_lock);
   allocation_t *p, *q;

   // we only allocate a number of bytes that will leave us at least
   // 8-byte aligned.
   // thoughts: what happens otherwise?
   // seems like my OS can handle unaligned stuff just fine
   bytes = ((bytes + 7)/8)*8;

   p = kmalloc_head->next; // start at first free node
   // continue until found a page with enough space, or searched everything
   for(; p != kmalloc_head && p->size < bytes; p=p->next)
   {
      ;
   }
   // now p might be kmalloc_head (not enough space)
   // thoughts: I choose to just get pages for the new allocation here,
   // but it's possible that a 1.5 page allocation may only require 1
   // additional page if the last node in the free list can accomodate.
   //  (and you don't need contiguous memory, of course0
   // possible optimization: check to see if a free node has within-page
   // memory, thus making it contiguous.
   if(p == kmalloc_head || request_contiguous)
   {
      // not enough space
      int pages_to_alloc = ((bytes+sizeof(allocation_t)+PAGE_SIZE-1)/PAGE_SIZE);
      p = (allocation_t *) kernel_heap_vaddr;
      if(/* !request_contiguous */ 0) //noncontiguous
      { // TODO: Get this working and find out what's going on with the 513s..
         int i;
         for(i = 0; i < pages_to_alloc; i++)
         {
            if(!(i%25))
            {
               printf("fetching you pages (%d)\n", i);
            }
            // TODO: fetching too many pages causes the OS to break.
            //       Sounds like an office hours moment
            allocation_t *newpage = (allocation_t *)page_zalloc();
            mmu_map(kernel_mmu_table,
                     kernel_heap_vaddr, (uint64_t)newpage,
                     PTEB_READ | PTEB_WRITE);
            kernel_heap_vaddr += PAGE_SIZE;
            if(newpage == NULL)
            {
               // then we can't meet this request
               printf("malloc ERROR: could not create space for your request\n");
               // free everything backwards
               for(; i >= 0; i--)
               {
                  if(!(i%25))
                  {
                     printf("Freeing page %d\n", i);
                  }
                  kernel_heap_vaddr -= PAGE_SIZE;
                  page_free((void*)(mmu_translate(
                                 kernel_mmu_table, kernel_heap_vaddr)));
                  mmu_unmap(kernel_mmu_table, kernel_heap_vaddr);
                  asm volatile("sfence.vma zero, %0" : : "r"(KERNEL_ASID));
               }
               asm volatile("sfence.vma zero, %0" : : "r"(KERNEL_ASID));
               mutex_post(&kmalloc_lock);
               return NULL;
            }
         }
      }
      else // contiguous
      {
         allocation_t *newpage = (allocation_t *)page_znalloc(pages_to_alloc);
         if(newpage == NULL)
         {
            printf("malloc ERROR: could not create space for your request\n");
            mutex_post(&kmalloc_lock);
            return NULL;
         }
         mmu_map_multiple(kernel_mmu_table,
                          kernel_heap_vaddr, (uint64_t)newpage,
                          pages_to_alloc*PAGE_SIZE,
                          PTEB_READ | PTEB_WRITE);
         kernel_heap_vaddr += pages_to_alloc*PAGE_SIZE;
      }
      // sfence: remove all TLB entries correspond to the Kernel (KERNEL_ASID)
      // thoughts: this might not be necessary, since we could simply just flush
      //           the virtual address
      asm volatile("sfence.vma zero, %0" : : "r"(KERNEL_ASID));

      p->size = pages_to_alloc*PAGE_SIZE - sizeof(allocation_t);
      // place p at the end of the list.
      allocation_dll_insert(kmalloc_head->prev, p);
   }

   // or it's a free node with enough space
   // if the free node has enough space to split
   if(p->size >= bytes + sizeof(allocation_t))
   {
      // split the node
      q = (allocation_t *)   (((char *)(p + 1)) + bytes);
      q->size = p->size - bytes - sizeof(allocation_t);
      p->size = bytes;
      allocation_dll_insert(p,q);
      allocation_dll_delete(p);
   }
   else
   {
      // there isn't enough space for the free note to split
      allocation_dll_delete(p);
   }


   // semantically not necessary for kcalloc because page alloc zero's
   // do it anyway though.
   if(do_zero)
   {
      memset(p+1, 0, p->size);
   }

   if(p == NULL)
   {
      ERRORMSG("should never reach here");
   }
   mutex_post(&kmalloc_lock);
   return (void *)(p+1); // we return q+1 because that is where the space begins
}

static unsigned int number_of_frees;
// practically, we have this set to 10 or 100. I'm not playing games with random
// bugs though. Set to 1 for now.
#define COALESCE_EACH 1
#define SHRINK_HEAP_EACH 1

void kfree(void * addr)
{
   // allow free of NULL
   mutex_wait(&kmalloc_lock);
   if(addr == NULL)
   {
      mutex_post(&kmalloc_lock);
      return;
   }
   // we expect that wherever we look, there had better be an allocation_t
   // we subtract 1 because the user didn't know about our bookkeeping.
   //   (we just gave the user the address of where the free space started!)
   allocation_t *q = ((allocation_t *)addr)-1;
   allocation_t *p = kmalloc_head;
   // find the node that is right before the note q
   for(; p->next < q && p->next != kmalloc_head; p=p->next)
   {
      // printf("seeking node (at node 0x%012lx with next at 0x%012lx)\n",
                                  // p, p->next);
      ;
   }
   allocation_dll_insert(p,q);
   number_of_frees++;
   if(!(number_of_frees % COALESCE_EACH))
   {
      // printf("many frees have occurred. I will now coalesce.\n");
      coalesce_free_list();
   }
   if(!(number_of_frees % SHRINK_HEAP_EACH))
   {
      heap_shrink();
   }
   // TODO: add memset if needed
   mutex_post(&kmalloc_lock);
   return;
}

void coalesce_free_list(void)
{
   allocation_t *p = kmalloc_head->next;
   for(; p != kmalloc_head; p=p->next)
   {
      while((char*)p + (p->size + sizeof(allocation_t)) == (char*)(p->next))
      {
         // nodes are contiguous
         p->size += (sizeof(allocation_t) + (p->next)->size);
         allocation_dll_delete(p->next);
      }
   }
   return;
}

// if less than the full heap size is being utilized, then free some.
void heap_shrink(void)
{
   allocation_t* last = kmalloc_head->prev;
   while(
         ((uint64_t)(last + 1)) + last->size == kernel_heap_vaddr &&
         last->size >= PAGE_SIZE
        ) 
   {
      last->size -= PAGE_SIZE;
      kernel_heap_vaddr -= PAGE_SIZE;
      // could use the fact that the MMU mapped this address
      // however, from my experience, this has not worked...
      // TODO: check again
      page_free((void*)(mmu_translate(kernel_mmu_table, kernel_heap_vaddr)));
      mmu_unmap(kernel_mmu_table, kernel_heap_vaddr);
   }
}

void heap_print_free_list(void)
{
   allocation_t *p;
   p = kmalloc_head;
   printf("heap goes from 0x%012lx to 0x%012lx.\n", p, kernel_heap_vaddr);
   printf("kmalloc head of size %6lu at location 0x%012lx\n",
         p->size, (unsigned long*)p);
   for(p = kmalloc_head->next; p != kmalloc_head; p=p->next)
   {
      printf("free node of size    %6lu at location 0x%012lx\n",
            p->size, (unsigned long*)p);
   }
   return;
}

void heap_size_of_allocation(void *allocation)
{
   // we assume that the pointer p is an allocation
   allocation_t *p = (allocation_t *) allocation;
   p -= 1;
   printf("size of allocation at 0x%012lx: %u\n", allocation, p->size);
   return;
}
