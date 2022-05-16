#include <stdint.h>
#include <printf.h>
#include <utils.h>
#include <mmu.h>
#include <process.h>
#include <page.h>


// file inspired by the need for the OS to copy to or from user

int copy_to_user(void *os_vaddr, void *user_vaddr, uint32_t size, process_t *p)
{
   uint64_t walking_os_vaddr   = (uint64_t)os_vaddr;
   uint64_t walking_user_vaddr = (uint64_t)user_vaddr;
   // I noticed that this exercise might not be necessary, since my OS
   // guarantees process images to be in contiguous memory. Nonetheless,
   // it's still good to be future proof.
   uint64_t walking_user_paddr;
   uint32_t copy_size;
   while(size > 0)
   {
      walking_user_paddr = mmu_translate(p->ptable, walking_user_vaddr);
      if(walking_user_paddr == -1UL)
      {
         ERRORMSG("user asked to copy into invalid memory");
         return 1;
      }
      copy_size = PAGE_SIZE - (walking_user_vaddr % PAGE_SIZE);
      if(copy_size > size)
      {
         copy_size = size;
      }
      // we have the user paddr identity mapped (it's in the heap!)
      memcpy((void*)walking_user_paddr, (void*)walking_os_vaddr, copy_size);
      walking_os_vaddr += copy_size;
      walking_user_vaddr += copy_size;
      size -= copy_size;
   }
   return 0;
}

int copy_to_os(void *os_vaddr, void *user_vaddr, uint32_t size, process_t *p)
{
   uint64_t walking_os_vaddr   = (uint64_t)os_vaddr;
   uint64_t walking_user_vaddr = (uint64_t)user_vaddr;
   // I noticed that this exercise might not be necessary, since my OS
   // guarantees process images to be in contiguous memory. Nonetheless,
   // it's still good to be future proof.
   uint64_t walking_user_paddr;
   uint32_t copy_size;
   while(size > 0)
   {
      walking_user_paddr = mmu_translate(p->ptable, walking_user_vaddr);
      if(walking_user_paddr == -1UL)
      {
         ERRORMSG("user asked to copy from invalid memory");
         return 1;
      }
      copy_size = PAGE_SIZE - (walking_user_vaddr % PAGE_SIZE);
      if(copy_size > size)
      {
         copy_size = size;
      }
      // we have the user paddr identity mapped (it's in the heap!)
      memcpy((void*)walking_os_vaddr, (void*)walking_user_paddr, copy_size);
      walking_os_vaddr += copy_size;
      walking_user_vaddr += copy_size;
      size -= copy_size;
   }
   return 0;
}
