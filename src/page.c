#include <page.h>
#include <symbols.h>
#include <stddef.h>
#include <stdint.h>
#include <lock.h>
#include <csr.h>
#include <printf.h>
#include <utils.h>

mutex_t page_lock = MUTEX_LOCKED;


// TODO: fix the strange nature of these functions: I don't like the way
// that I have them right now. Too much like the class code.
int is_taken(char *begin, int page_n)
{
   int which_byte = page_n / 4;
   int which_bit = 2 * (page_n%4);
   return(((*(begin + which_byte)) >> which_bit) & 1);
}

void clear_taken(char *begin, int page_n)
{
   int which_byte = page_n / 4;
   int which_bit = 2 * (page_n%4);
   begin[which_byte] &= ~(1 << which_bit);
}

void set_taken(char *begin, int page_n)
{
   int which_byte = page_n / 4;
   int which_bit = 2 * (page_n%4);
   begin[which_byte] |= (1 << which_bit);
}

int is_last(char *begin, int page_n)
{
   int which_byte = page_n / 4;
   int which_bit = 2 * (page_n%4)+1;
   return(((*(begin + which_byte)) >> which_bit) & 1);
}

void clear_last(char *begin, int page_n)
{
   int which_byte = page_n / 4;
   int which_bit = 2 * (page_n%4) + 1;
   begin[which_byte] &= ~(1 << which_bit);
}

void set_last(char *begin, int page_n)
{
   int which_byte = page_n / 4;
   int which_bit = 2 * (page_n%4) + 1;
   begin[which_byte] |= (1 << which_bit);
}

void page_init(void)
{
   uint64_t start = sym_start(heap);
   char *char_start = (char *)start;
   uint64_t end = sym_end(heap);
   uint64_t heap_memory = end-start;
   uint64_t bookkeeping_bytes = heap_memory/ PAGE_SIZE / 4; // 2 bits per page
   memset((void*)start, 0, bookkeeping_bytes);
   uint64_t pages_for_bookkeeping =
            ((bookkeeping_bytes + PAGE_SIZE - 1) & (-PAGE_SIZE))/PAGE_SIZE;
   int i;
   for(i = 0; (uint64_t)i < pages_for_bookkeeping; i++)
   {
      set_taken(char_start, i);
   }
   set_last(char_start, i-1);
}

void * page_znalloc(int n)
{
   uint64_t start = sym_start(heap);
   char *char_start = (char *)start;
   uint64_t end = sym_end(heap);
   uint64_t heap_memory = end-start;
   uint64_t total_pages = heap_memory / PAGE_SIZE;
   uint64_t bookkeeping_bytes = total_pages / 4; // 2 bits per page

   int consecutive = 0;
   int i, j;
   for(i = 0; (uint64_t) i < total_pages/10; i++)
   {
      consecutive = 0; // consecutive = 0 set many times.
      if(!(is_taken(char_start, i)))
      {
         for (j = i; (uint64_t) j < total_pages; j++)
         {
            if(is_taken(char_start, j))
            {
               consecutive = 0;
               break;
            }
            // else
            consecutive++;
            if(consecutive >= n)
            {
               // a long enough list has been reached.
               for(j = 0; j < consecutive; j++)
               {
                  set_taken(char_start, i+j);
               }
               set_last(char_start, i+j-1);
               // below from in class code: clever way to cut up remainder
               bookkeeping_bytes = (bookkeeping_bytes + (4095)) & -4096;
               // memset returns address on success
               return memset((void *)(start+bookkeeping_bytes+PAGE_SIZE*i),
                             0,
                             PAGE_SIZE*consecutive);
            }
         }
      }
      else
      {
         consecutive = 0;
      }
   }
   return NULL; // failed to find a matching page size to meet request.
}

void * page_zalloc(void)
{
   return page_znalloc(1);
}

void page_free(void *p)
{
   uint64_t page = (uint64_t)p;
   uint64_t start = sym_start(heap);
   char *char_start = (char *)start;
   uint64_t end = sym_end(heap);
   uint64_t heap_memory = end-start;
   uint64_t total_pages = heap_memory / PAGE_SIZE;
   uint64_t bookkeeping_bytes = total_pages / 4; // 2 bits per page
   uint64_t location = (page - bookkeeping_bytes - start) / PAGE_SIZE;
            // or "page number"

   if (page < start || page > end)
   {
      printf("error: symbol %012lx not in heap!\n", page);
      printf("heap: [%012lx - %012lx]\n", start,end);
      return; // symbol wasn't in the heap to begin with
   }

   // possibly we are freeing a page in the middle of a larger allocation
   // (since this OS is striving to allow shrinking such allocations)
   // This means we'd have to set the 'last' bit on the previous page
   uint64_t bk_pages;
   bk_pages = ((bookkeeping_bytes + PAGE_SIZE - 1) & (-PAGE_SIZE))/PAGE_SIZE;
   if(location < bk_pages)
   {
      // should never happen. That's part of the page table's bookkeeping!
      // We can set the last element of the bookkeeping to last again, no sweat
      ERRORMSG("free of bookkeeping pages");
   }
   if(is_taken(char_start, location-1))
   {
      set_last(char_start, location-1);
   }


   do
   {
      if(!(is_taken(char_start, location)))
      {
         // double free. Make some noise here.
         ERRORMSG("double free of a page");
         printf("page number #%d\n",location);
         return;
      }
      else if(is_last(char_start, location))
      {
         clear_taken(char_start, location);
         clear_last(char_start, location);
         break;
      }
      // else
      clear_taken(char_start, location);
      location+= 1;
   } while(1);
}

void print_pages()
{
   uint64_t start = sym_start(heap);
   char *char_start = (char *)start;
   uint64_t end = sym_end(heap);
   uint64_t heap_memory = end-start;
   uint64_t total_pages = heap_memory / PAGE_SIZE;
   uint64_t bookkeeping_bytes = total_pages / 4; // 2 bits per page
   printf("total heap size: 0x%x\n", heap_memory);
   printf("total pages: 0x%x (%d)\n", total_pages, total_pages);
   printf("total bookkeeping bytes: 0x%x (%d)\n", bookkeeping_bytes, bookkeeping_bytes);
   printf("amount of pages needed for bookkeeping_bytes: %d\n",
              ((bookkeeping_bytes + PAGE_SIZE - 1) & (-PAGE_SIZE))/PAGE_SIZE);
   int i;
   for(i = 0; (uint64_t) i < total_pages; i++)
   {
      if(is_taken(char_start,i))
      {
         printf("page %4d alloc'ed (0x%02x)\n", i, (int)char_start[i/4]);
      }
   }
}

//TODO: what if not using contiguous memory allocation?

