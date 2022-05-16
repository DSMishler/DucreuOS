#include <elf.h>
#include <process.h>
#include <printf.h>
#include <utils.h>
#include <page.h>

extern uint64_t spawn_thread_start;
extern uint64_t spawn_thread_end;
extern uint64_t spawn_trap_start;
extern uint64_t spawn_trap_end;

int elf_load(process_t *p, const void *elf)
{
   Elf64_Ehdr_t *eh = (Elf64_Ehdr_t *)elf; // elf header
   Elf64_Phdr_t *ph = (Elf64_Phdr_t *)((uint64_t)elf+eh->e_phoff); // program header

   // 3 error checks:
   // 1: check for ELF file
   if(memcmp(eh->e_ident, ELFMAG, SELFMAG))
   {
      ; //matches
   }
   else
   {
      ERRORMSG("not an elf file");
      return 1;
   }

   // 2: only support RISC-V
   if(eh->e_machine != EM_RISCV)
   {
      ERRORMSG("elf file not for RISC-V");
      return 2;
   }

   // 3: ensure file is executable
   if(eh->e_type != ET_EXEC)
   {
      ERRORMSG("elf file not executable");
      return 3;
   }

   // Also... the page table should have already been set up
   if(!(p->ptable))
   {
      ERRORMSG("elf load called before process set up");
      return 5;
   }

   if(p->privilege_mode != PS_PRIV_USER)
   {
      ERRORMSG("attempt to load non-user elf process");
      // but it would be easy to fix: just load the image and don't
      // map it with the user bit!
      return 6;
   }

   // address ranges
   // find them from the program table
   // we will also need to algin to page size later
   int i;
   uint64_t vaddr_start, vaddr_end;
   vaddr_start = vaddr_end = 0;
   int program_table_loaded;
   for(i = 0; i < eh->e_phnum; i++)
   {
      if(ph[i].p_type == PT_LOAD)
      {
         if(!program_table_loaded)
         {
            vaddr_start = ph[i].p_vaddr;
            // inclass code doesn't add memsz. Possible bug?
            vaddr_end = ph[i].p_vaddr+ph[i].p_memsz;
            program_table_loaded = 1;
         }
         else
         {
            if(vaddr_start > ph[i].p_vaddr)
            {
               vaddr_start = ph[i].p_vaddr;
            }
            if(vaddr_end < ph[i].p_vaddr+ph[i].p_memsz)
            {
               vaddr_end = ph[i].p_vaddr+ph[i].p_memsz;
            }
         }
      }
      // printf("start and end addresses: [0x%016lx - 0x%016lx]\n",
                  // vaddr_start, vaddr_end);
   }

   if(!(program_table_loaded))
   {
      ERRORMSG("no program table loaded");
      return 4;
   }

   uint64_t pa_vaddr_start, pa_vaddr_end;
   pa_vaddr_start = (vaddr_start / PAGE_SIZE)             * PAGE_SIZE;
   pa_vaddr_end =   ((vaddr_end+PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;
   // Should we round up for the case where vaddr_end is
   // on a page boundary. This shouldn't be necessary, since vaddr_end is
   // the *end* of the last vaddr in the image
   uint64_t num_process_image_pages;
   num_process_image_pages = (pa_vaddr_end-pa_vaddr_start)/PAGE_SIZE;

   if((pa_vaddr_start < spawn_thread_start && spawn_thread_start < pa_vaddr_end)
      ||
      (spawn_thread_start < pa_vaddr_start && pa_vaddr_start < spawn_trap_end))
   {
      ERRORMSG("kernel addresses overlap with elf addresses");
      return 5;
   }

   // we're now all set to manipulate the process itself
   p->process_frame.sepc = eh->e_entry; // set up sepc for sret
   p->num_image_pages = num_process_image_pages;
   // physically contiguous physical address
   p->image = page_znalloc(p->num_image_pages);

   // now go back through and copy the data in
   for(i = 0; i < eh->e_phnum; i++)
   {
      if(ph[i].p_type == PT_LOAD)
      {
         uint64_t dest = (uint64_t)p->image + ph[i].p_vaddr - vaddr_start;
         uint64_t src  = (uint64_t)elf + ph[i].p_offset;
         uint64_t size = ph[i].p_memsz;
         // printf("copying %d bytes from 0x%012lx to 0x%012lx\n",
                 // size, src, dest);
         memcpy((void*)dest, (void*)src, size);
      }
   }

   // now map the range for the user
   mmu_map_multiple(p->ptable,
                    pa_vaddr_start,
                    (uint64_t)p->image,
                    num_process_image_pages * PAGE_SIZE,
                    PTEB_USER | PTEB_READ | PTEB_WRITE | PTEB_EXECUTE);

   return 0;
}
