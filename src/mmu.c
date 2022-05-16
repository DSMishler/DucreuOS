#include <mmu.h>
#include <lock.h>
#include <page.h>
#include <printf.h>
#include <utils.h>

page_table_t *kernel_mmu_table;
mutex_t kernel_mmu_table_lock = MUTEX_UNLOCKED;

int mmu_map(page_table_t *pt, uint64_t vaddr, uint64_t paddr, uint64_t bits)
{
   if (!(bits & 0xe))
   {
      // this implies a branch at level zero
      printf("No can do, chief. All pt entries at level zero must have some");
      printf(" permissions enabled.\n");
      return -1;
   }
   return mmu_set(pt, vaddr, paddr, bits | PTEB_VALID);
}

int mmu_unmap(page_table_t *pt, uint64_t vaddr)
{
   return mmu_set(pt, vaddr, 0, 0);
}

int mmu_set(page_table_t *pt, uint64_t vaddr, uint64_t paddr, uint64_t bits)
{
   uint64_t vpn[] = // as per the riscv-privileged spec
   {
      (vaddr >> 12) & 0x1FF,
      (vaddr >> 21) & 0x1FF,
      (vaddr >> 30) & 0x1FF
   };
   uint64_t ppn[] = // once again per the riscv-privileged spec. Note that
                    // the level two physical address may be much larger.
   {
      (paddr >> 12) & 0x1FF,
      (paddr >> 21) & 0x1FF,
      (paddr >> 30) & 0x3FFFFFF
   };


   uint64_t entry;

   mutex_wait(&kernel_mmu_table_lock);
   // begin at second level page table.
   entry = pt->entries[vpn[2]];
   // possibly allocate a new page table.
   if(!(entry & PTEB_VALID))
   {
      page_table_t *new_table = page_zalloc();
      entry = (((uint64_t)new_table) >> 2) | PTEB_VALID; // there is extra junk
                                             // in that address, possibly. We
                                             // only set some bits here.
      pt->entries[vpn[2]] = entry;
   }
   pt = (page_table_t *) ((entry << 2) & ~0xFFFUL); // pt = pt->lower

   // now to first level
   entry = pt->entries[vpn[1]];
   // possibly allocate a new page table.
   if(!(entry & PTEB_VALID))
   {
      page_table_t *new_table = page_zalloc();
      entry = (((uint64_t)new_table) >> 2) | PTEB_VALID; // there is extra junk
                                             // in that address, possibly. We
                                             // only set some bits here.
      pt->entries[vpn[1]] = entry;
   }
   pt = (page_table_t *) ((entry << 2) & ~0xFFFUL); // pt = pt->lower

   // now at level 0
   entry = (ppn[2] << 28) | (ppn[1] << 19) | (ppn[0] << 10) | bits;
   pt->entries[vpn[0]] = entry;


   mutex_post(&kernel_mmu_table_lock);
   return 0; // no news is good news.
}

void mmu_free(page_table_t *pt)
{
   uint64_t entry;
   int i;
   for(i = 0; i < 512; i++)
   {
      entry = pt->entries[i];
      if(entry & 1) // possibly dig deeper
      {
         if(!(entry & 0xe)) // or 0b1110, for XWR
         {
            mmu_free((page_table_t *) ((entry << 2) & ~0xFFFUL));// pass pt->lower
         }
         else
         {
            // entry was a leaf
            pt->entries[i] = 0;
         }
      }
   }
   page_free(pt);
}

uint64_t mmu_translate(page_table_t *pt, uint64_t vaddr)
{
   int i;
   uint64_t vpn[] =
   {
      (vaddr >> 12) & 0x1FF,
      (vaddr >> 21) & 0x1FF,
      (vaddr >> 30) & 0x1FF
   };
   for(i = 2; i >= 0; i--)
   {
      uint64_t entry = pt->entries[vpn[i]];
      if(!(entry & PTEB_VALID))
      {
         printf("Error: mmu asked to translate invalid virtual address.\n");
         printf("    - Entry is 0x%012lx is invalid in page table at 0x%08x\n",
                 entry, (unsigned long *)pt);
         printf("    - Entry is found at 0x%08x\n",
                 (unsigned int *) &(pt->entries[vpn[i]]));
         printf("    - vpn is %d (at i=%d)\n", vpn[i], i);
         return -1UL; // maximum bits of F for error code.
      }
      else if((entry & 0xe)) // entry is a leaf
      {
         uint64_t ppn[] = 
         {
            (entry >> 10) & 0x1FF,
            (entry >> 19) & 0x1FF,
            (entry >> 28) & 0x3FFFFFF,
         };
         if(i == 2)
         {
            return ppn[2] << 30 | (vaddr & 0x3FFFFFFF);
         }
         else if(i == 1)
         {
            return ppn[2] << 30 | ppn[1] << 21 | (vaddr & 0x1FFFFF);
            //TODO: oi there was a bug here in class source. Missing the 1
            //in 0x1FFFFF
         }
         else if(i == 0)
         {
            return ppn[2] << 30 | ppn[1] << 21 | ppn[0] << 12 | (vaddr & 0xFFF);
         }
         else
         {
            ERRORMSG("i decrement");
            return -1UL;
         }
      }
      else
      {
         //pt = pt->lower
         pt = (page_table_t *)((entry << 2) & ~0xFFFUL);
      }
   }
   // function should have returned by now.
   ERRORMSG("unfound address");
   return -1UL;
}

// TODO: to propagate errors, this must return the code of the first
// error-giving process.
// I might prefer instead of num_bytes to allocate num_pages.
void mmu_map_multiple(page_table_t *pt, uint64_t start_vaddr,
                      uint64_t start_paddr, uint64_t bytes,
                       uint64_t bits)
{
   for(uint64_t i = 0; i < bytes; i += PAGE_SIZE)
   {
      if(mmu_map(pt, start_vaddr + i, start_paddr + i, bits) != 0)
      {
         printf("multiple MMU map error. Aborting mapping.\n");
         return;
      }
   }
}

// first entry tells you first two bits. It will only be 0 (0x0), 1(0x4)...  3.
// second entry tells you next nine bits. Third tells you next nine bits.
// That makes 20 bits. The rest is not translated!
void mmu_show_page_table(page_table_t *base,
                         page_table_t *pt,
                         int level, uint64_t
                         vaddr)
{
   uint64_t entry;
   int virtaddr_leftshift;
   int i;
   char *header;
   switch(level)
   {
      case 2:
         printf("page table at %012lx\n", pt);
         virtaddr_leftshift = 30;
         header = "";
         break;
      case 1:
         header = "    ";
         virtaddr_leftshift = 21;
         break;
      case 0:
         header = "        ";
         virtaddr_leftshift = 12;
         break;
      default:
         return;
   }
   int leaf_printed = 0;
   for(i = 0; i < 512; i++)
   {
      entry = pt->entries[i];
      if(entry & 1) // possibly dig deeper
      {
         // don't print too many page table leaves (heap too big)
         if (leaf_printed > 16) // so - up to seventeen printings
         {
             break;
         }
         printf("%s%03d: ", header, i);
         if(!(entry & 0xe)) // or 0b1110, for XWR
         {
            printf("branch (vaddr 0x%012lx + ...) ", vaddr);
            printf("points to page table at 0x%08x\n",
                     (unsigned long *)((entry << 2) & ~0xFFFUL));
            // pass pt->lower
            mmu_show_page_table(base,
                                (page_table_t *)((entry << 2) & ~0xFFFUL),
                                level-1,
                                vaddr);
         }
         else
         {
            // entry was a leaf
            leaf_printed++;
            
            printf("leaf (0b%08b) (", entry & 0xff);
            if(entry & PTEB_EXECUTE)
            {
               printf("X");
            }
            else
            {
               printf("-");
            }
            if(entry & PTEB_WRITE)
            {
               printf("W");
            }
            else
            {
               printf("-");
            }
            if(entry & PTEB_READ)
            {
               printf("R");
            }
            else
            {
               printf("-");
            }
            printf(") (vaddr 0x%012lx : paddr 0x%08x)\n",
                   vaddr, mmu_translate(base, vaddr));
         }
      }
      vaddr += 1 << virtaddr_leftshift;
   }
}
