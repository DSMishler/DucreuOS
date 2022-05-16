#include <process.h>
#include <hart.h>
#include <mmu.h>
#include <printf.h>
#include <page.h>
#include <kmalloc.h>
#include <utils.h>
#include <csr.h>
#include <sbi.h>
#include <lock.h>

// symbols from asm/spawn.S
// part of the trampoline into and out of U-mode.
// spawn thread? deescalate permissions
// spawn trap? escalate permissions
extern uint64_t spawn_thread_start;
extern uint64_t spawn_thread_end;
extern uint64_t spawn_trap_start;
extern uint64_t spawn_trap_end;
extern uint64_t process_idle_code_start;
extern uint64_t process_idle_code_end;

// array of process pointers: which process is running of which hart
// NULL for hart parked
process_t * process_current[MAX_SUPPORTED_HARTS];
process_t * process_idle[MAX_SUPPORTED_HARTS];
uint64_t process_trap_stacks[MAX_SUPPORTED_HARTS];
process_llist_node_start_t *unsched_process_llist;
process_llist_node_start_t *sleeping_process_llist;

void process_init_trap_stacks(void)
{
   int i;
   for(i = 0; i < MAX_SUPPORTED_HARTS; i++)
   {
      process_trap_stacks[i] = (uint64_t)page_zalloc();
   }
}

// init a process from thin air
process_t * process_init_new(process_priv_t mode)
{
   process_t *p;
   // can we make this not contiguous?
   // Answer: no. there's a good chance that the
   // SBI will have to handle this, so it had better be contiguous.
   p = kcalloc(sizeof(process_t));
   MALLOC_CHECK(p);

   // wait for whoever called this function to decide when the process is
   // ready to run
   p->process_state = PS_UNINIT;
   p->ptable = page_zalloc();

   // one page for the stack
   p->stack = page_zalloc();
   p->num_stack_pages = 1;
   p->quantum = PROCESS_DEFAULT_QUANTUM;

   p->on_hart = -1; // -1 for not on a hart yet.
   
   uint64_t paddr;
   uint64_t pa_tds, pa_tde, pa_tps, pa_tpe; // page aligned versions
   pa_tds = (spawn_thread_start/PAGE_SIZE)*PAGE_SIZE;
   pa_tps = (spawn_trap_start / PAGE_SIZE)*PAGE_SIZE;
   pa_tde = ((spawn_thread_end + PAGE_SIZE - 1)/PAGE_SIZE)*PAGE_SIZE;
   pa_tpe = ((spawn_trap_end + PAGE_SIZE - 1) / PAGE_SIZE)*PAGE_SIZE;
   // translate, but is probably an identity map
   paddr = mmu_translate(kernel_mmu_table, spawn_thread_start);
   // note the PTEB_USER not set. This code is for the OS in the user ptable
   mmu_map_multiple(p->ptable, pa_tds, paddr,
                    pa_tde-pa_tds,
                    PTEB_EXECUTE);
   // since the vectors are small, we could be simply remapping the same pages.
   // That's perfectly fine - just don't want to miss anything.
   paddr = mmu_translate(kernel_mmu_table, spawn_trap_start);
   mmu_map_multiple(p->ptable, pa_tps, paddr,
                    pa_tpe-pa_tps,
                    PTEB_EXECUTE);

   // now map the process frame (note the user bit is not set)
   uint64_t process_frame = (uint64_t)(&(p->process_frame));
   paddr = mmu_translate(kernel_mmu_table, process_frame);
   uint64_t frame_pages = (sizeof(process_frame_t) + PAGE_SIZE - 1)/PAGE_SIZE;
   mmu_map_multiple(p->ptable, process_frame, paddr,
                    frame_pages*PAGE_SIZE,
                    PTEB_READ | PTEB_WRITE);

   // now place what we need to not be zero into the registers
   p->process_frame.gpregs[XREG_SP] = PROCESS_DEFAULT_STACK_POINTER
                                     + p->num_stack_pages*PAGE_SIZE;
   p->process_frame.sie = SIE_SSIE | SIE_STIE | SIE_SEIE;
   // let asm/spawn.S know where to find this
   p->process_frame.sscratch  = process_frame;
   // go here when a trap is encountered
   p->process_frame.stvec     = spawn_trap_start;
   p->process_frame.trap_satp = SATP_MODE_ON         |
                        ASID_TO_SATP(KERNEL_ASID)    |
                        PPN_TO_SATP(kernel_mmu_table);

   p->vruntime = 0;
   //TEMPORARY until we load the real SATP in the scheduler
   p->pid = PROCESS_UNDEFINED_PID;
   p->process_frame.satp =      SATP_MODE_ON  |
                        ASID_TO_SATP(p->pid)  |
                        PPN_TO_SATP(p->ptable);



   // now set up with privilege mode-specific things
   if(mode == PS_PRIV_USER || mode == PS_PRIV_SUPERVISOR)
   {
      ;
   }
   else
   {
      ERRORMSG("attempt to sawn unsupported process type");
      mode = PS_PRIV_USER;
   }

   p->privilege_mode = mode;
   uint64_t sstatus_set;
   uint64_t mmu_priv_bits;
   sstatus_set = SSTATUS_FS_OFF | SSTATUS_SPIE;
   mmu_priv_bits = PTEB_WRITE | PTEB_READ;
   if(mode == PS_PRIV_USER)
   {
      sstatus_set   |= SSTATUS_SPP_USER;
      mmu_priv_bits |= PTEB_USER;
   }
   else if(mode == PS_PRIV_SUPERVISOR)
   {
      sstatus_set |= SSTATUS_SPP_SUPERVISOR;
   }
   else
   {
      ERRORMSG("programmer incompetence");
      return NULL;
   }

   p->process_frame.sstatus = sstatus_set;
   mmu_map_multiple(p->ptable,PROCESS_DEFAULT_STACK_POINTER, (uint64_t)p->stack,
                    p->num_stack_pages*PAGE_SIZE,
                    mmu_priv_bits);

   return p;
}

// frees a process that has been fully allocated
void process_free(process_t *p)
{
   if(!p)
   {
      ERRORMSG("free of unalloc'ed process");
      return;
   }

   if(p->on_hart != -1)
   {
      ERRORMSG("free of process still on hart");
      p->process_state = PS_ASYNC_FREE;
      return;
   }

   if(p->image)
   {
      page_free(p->image);
   }
   else
   {
      DEBUGMSG("free of process with no image");
   }
   if(p->stack)
   {
      page_free(p->stack);
   }
   else
   {
      DEBUGMSG("free of process with no stack");
   }
   if(p->heap)
   {
      page_free(p->heap);
   }
   else
   {
      DEBUGMSG("free of process with no heap");
   }
   if(p->ptable)
   {
      page_free(p->ptable);
   }
   else
   {
      DEBUGMSG("free of process with no page table");
   }


   kfree(p);
   return;
}

int process_spawn(process_t *p, int target_hart)
{
   // printf("spawning process stored at 0x%012lx on hart %d\n",
           // (uint64_t)p, target_hart);
   int who_am_i = sbi_who_am_i();
   int target_hart_status = sbi_get_hart_status(target_hart);
   if(who_am_i == target_hart)
   {
      ;
   }
   else
   {
      if(target_hart_status == HS_INVALID)
      {
         ERRORMSG("attempt to spawn on invalid hart");
         return 1;
      }
      // not sure why inclass code doesn't allow this
      if(target_hart_status != HS_STOPPED)
      {
         DEBUGMSG("starting process on unstopped hart");
      }
   }

   p->on_hart = target_hart;
   process_current[target_hart] = p;
   // give the process a trap stack
   p->process_frame.trap_stack = process_trap_stacks[target_hart];

   // process_print_frame(p);
   // set the context switch timer to wake it up later
   p->vruntime += p->quantum * PROCESS_DEFAULT_CONTEXT_TIMER;
   sbi_add_clint_timer(target_hart, p->quantum * PROCESS_DEFAULT_CONTEXT_TIMER);
   if(who_am_i == target_hart)
   {
      // we can use the virtual address of the process frame
      CSR_WRITE("sscratch", p->process_frame.sscratch);
      // call the spawn thread start like it's a function (C is crazy, no?)
      ((void (*)(void))spawn_thread_start)();
   }
   else
   {
      // sbi will need to start another hart. get the right physical address.
      uint64_t frame_paddr;
      frame_paddr = mmu_translate(kernel_mmu_table, p->process_frame.sscratch);
      sbi_hart_start(target_hart, spawn_thread_start, frame_paddr);
   }
   // technically, this function should never really return
   return 0;
}

int load_idle_process(process_t *p)
{
   if(p->privilege_mode != PS_PRIV_SUPERVISOR)
   {
      ERRORMSG("attempt to load non-supervisor idle process");
      // but it would be easy to fix: just load the image and don't
      // map it with the user bit!
      return 1;
   }
   uint64_t vaddr_start, vaddr_end;
   uint64_t pa_vaddr_start, pa_vaddr_end;
   uint64_t num_process_image_pages;
   vaddr_start = process_idle_code_start;
   vaddr_end =   process_idle_code_end;
   pa_vaddr_start = (vaddr_start / PAGE_SIZE) * PAGE_SIZE;
   pa_vaddr_end =   ((vaddr_end+PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;
   num_process_image_pages = (pa_vaddr_end-pa_vaddr_start)/PAGE_SIZE;
   if((pa_vaddr_start < spawn_thread_start && spawn_thread_start < pa_vaddr_end)
      ||
      (spawn_thread_start < pa_vaddr_start && pa_vaddr_start < spawn_trap_end))
   {
      ERRORMSG("kernel addresses overlap (damn the linker)");
      return 2;
   }
   p->process_frame.sepc = vaddr_start;
   p->num_image_pages = num_process_image_pages;
   p->image = page_znalloc(p->num_image_pages);
   // load in the code
   uint64_t dest = (uint64_t)p->image;
   uint64_t src  = pa_vaddr_start;
   uint64_t size = pa_vaddr_end-pa_vaddr_start;
   memcpy((void*)dest, (void*)src, size);
   mmu_map_multiple(p->ptable,
                    pa_vaddr_start,
                    (uint64_t)p->image,
                    num_process_image_pages * PAGE_SIZE,
                    PTEB_READ | PTEB_WRITE | PTEB_EXECUTE);
   return 0;
}

void process_print_frame_offsets(void)
{
   process_t *p = NULL;
   uint64_t p_base = (uint64_t)p;
   process_frame_t *pf = &(p->process_frame);
   printf("%04d:  frame struct\n", (uint64_t)(pf) - p_base);
   printf("%04d:  gpregs\n",    (uint64_t)(&(pf->gpregs))     - p_base);
   printf("%04d:  fpregs\n",    (uint64_t)(&(pf->fpregs))     - p_base);
   printf("%04d:  sepc\n",      (uint64_t)(&(pf->sepc))       - p_base);
   printf("%04d:  sstatus\n",   (uint64_t)(&(pf->sstatus))    - p_base);
   printf("%04d:  sie\n",       (uint64_t)(&(pf->sie))        - p_base);
   printf("%04d:  satp\n",      (uint64_t)(&(pf->satp))       - p_base);
   printf("%04d:  sscratch\n",  (uint64_t)(&(pf->sscratch))   - p_base);
   printf("%04d:  stvec\n",     (uint64_t)(&(pf->stvec))      - p_base);
   printf("%04d:  trap_satp\n", (uint64_t)(&(pf->trap_satp))  - p_base);
   printf("%04d:  trap_stack\n",(uint64_t)(&(pf->trap_stack)) - p_base);
   return;
}

void process_print_frame(process_t *p)
{
   process_frame_t *pf = &(p->process_frame);
   printf("printing process frame at 0x%012lx\n", (uint64_t)pf);
   int i;
   for(i = 0; i < 32; i++)
   {
      printf("0x%016lx:  (gpregs[%d])\n",    pf->gpregs[i], i);
   }
   for(i = 0; i < 32; i++)
   {
      printf("0x%016lx:  (fpregs[%d])\n",    pf->fpregs[i], i);
   }
   printf("0x%016lx:  (sepc)\n",      pf->sepc       );
   printf("0x%016lx:  (sstatus)\n",   pf->sstatus    );
   printf("0x%016lx:  (sie)\n",       pf->sie        );
   printf("0x%016lx:  (satp)\n",      pf->satp       );
   printf("0x%016lx:  (sscratch)\n",  pf->sscratch   );
   printf("0x%016lx:  (stvec)\n",     pf->stvec      );
   printf("0x%016lx:  (trap_satp)\n", pf->trap_satp  );
   printf("0x%016lx:  (trap_stack)\n",pf->trap_stack );
   return;
}

void process_top()
{
   // TODO: make a system call for the SBI to tell the OS how many
   // harts are active
   printf("top\n");
   printf("on harts\n");
   printf("-------------------------------------------------------\n");
   int i;
   for(i = 0; i < MAX_SUPPORTED_HARTS; i++)
   {
      if(process_current[i] == NULL)
      {
         printf("hart %d: invactive\n", i);
      }
      else if(process_current[i] == process_idle[i])
      {
         printf("hart %d: idle\n", i);
      }
      else
      {
         printf("hart %d: pid 0x%04x vruntime %010u\n",
                 i, process_current[i]->pid, process_current[i]->vruntime);
      }
   }
   printf("\n");
   printf("floating processes\n");
   printf("-------------------------------------------------------\n");
   volatile process_llist_node_t *l;
   if(unsched_process_llist == NULL)
   {
      printf("scheduler not initialized yet\n");
      return;
   }
   l = unsched_process_llist->head;
   for(; l != NULL; l = l->next)
   {
      printf("unscheduled: pid 0x%04x vruntime %010u\n",
               l->p->pid, l->p->vruntime);
   }
   printf("\n");
   printf("sleeping processes\n");
   printf("-------------------------------------------------------\n");
   if(sleeping_process_llist == NULL)
   {
      printf("scheduler not initialized yet\n");
      return;
   }
   l = sleeping_process_llist->head;
   for(; l != NULL; l = l->next)
   {
      printf("sleeping: pid 0x%04x vruntime %010u\n",
               l->p->pid, l->p->vruntime);
   }
   printf("\n");
}



////////////////////////////////////////////////////////////////////////////////
// process list for the scheduler
////////////////////////////////////////////////////////////////////////////////

void process_remove(process_llist_node_start_t *pl, process_t *remove)
{
   // office hours moment: why were there bugs BEFORE I implemented this?
   // That doesn't make sense to me - there should only be one scheduler
   // calling these functions at a time. And yet... it was necessary.
   mutex_wait(&(pl->mutex));
   volatile process_llist_node_t *n = pl->head;
   for(; n != NULL; n = n->next)
   {
      if(n->p == remove)
      {
         break;
      }
   }
   if(n == NULL)
   {
      ERRORMSG("not in list");
      mutex_post(&(pl->mutex));
      return;
   }
   if(n->prev)
   {
      (n->prev)->next = n->next;
   }
   else
   {
      pl->head = n->next;
   }
   if(n->next)
   {
      (n->next)->prev = n->prev;
   }

   kfree((void*)n);
   mutex_post(&(pl->mutex));
}

process_t *process_remove_lowest(process_llist_node_start_t *pl)
{
   // office hours moment: why were there bugs BEFORE I implemented this?
   // That doesn't make sense to me - there should only be one scheduler
   // calling these functions at a time. And yet... it was necessary.
   mutex_wait(&(pl->mutex));
   volatile process_llist_node_t *start = pl->head;
   if(start == NULL)
   {
      mutex_post(&(pl->mutex));
      return NULL;
   }
   pl->head = start->next;

   // if there is still an element left in the list, remove its reference
   // to the element we just removed
   if(pl->head != NULL)
   {
      pl->head->prev = NULL;
   }

   process_t *ret = start->p;
   kfree((void*)start);
   mutex_post(&(pl->mutex));
   return ret;
}

void process_insert(process_llist_node_start_t *pl, process_t *p)
{
   if(p == NULL)
   {
      ERRORMSG("insertion of null process");
      return;
   }
   mutex_wait(&(pl->mutex));
   volatile process_llist_node_t *start;
   start = pl->head;
   volatile process_llist_node_t *new = kmalloc(sizeof(process_llist_node_t));
   MALLOC_CHECK(new);
   new->p = p;
   if(start == NULL)
   {
      new->next = NULL;
      new->prev = NULL;
      pl->head = new;
      mutex_post(&(pl->mutex));
      return;
   }
   // printf("inserting node of vruntime=%u, pid=%d into this list:\n",
           // new->p->vruntime, new->p->pid);
   // process_print_llist(pl);
   // printf("ll len BEGIN: %d\n", process_llist_len(pl));
   // else
   volatile process_llist_node_t *l;
   // printf("pl->head: 0x%012lx start: 0x%012lx\n", pl->head, start);
   // l = start = pl->head; // this was also a way to fix the bug
   l = start;
   if(new->p->vruntime <= l->p->vruntime)
   {
      // printf("adding node in beginning\n");
      // printf("pl->head: 0x%012lx l: 0x%012lx\n", pl->head, l);
      pl->head = new;
      l->prev = new;
      new->next = l;
      new->prev = NULL;
      // printf("showing list after setting\n");
      // process_print_llist(pl);
      // printf("done showing list after setting\n");
      // DEBUGMSG("adding node at start");
   }
   else
   {
      for(l = l->next; l != NULL; l = l->next)
      {
         if(new->p->vruntime <= l->p->vruntime)
         {
            // then insert the node new right before l (l is the first node with
            // higher vruntime)
            // printf("adding node in middle\n");
            volatile process_llist_node_t *oldprev = l->prev;
            oldprev->next = new;
            l->prev = new;
            new->next = l;
            new->prev = oldprev;
            break;
         }
      }
   }
   if(l == NULL)
   {
      // else the node new has a higher vruntime that everyone
      for(l = start; l->next != NULL; l = l->next)
         ;
      // printf("adding node in end\n");
      l->next = new;
      new->prev = l;
      new->next = NULL;
      // DEBUGMSG("adding node at end");
   }
   // printf("ll len  END : %d\n", process_llist_len(pl));
   // process_print_llist(pl);
   // printf("\n\n\n\n\n\n\n");
   mutex_post(&(pl->mutex));
   return;
}

int process_llist_len(process_llist_node_start_t *pl)
{
   volatile process_llist_node_t *ll = pl->head;
   int i;
   i = 0;
   for(ll = pl->head; ll != NULL; ll = ll->next)
   {
      i++;
   }
   return i;
}


void process_print_llist(process_llist_node_start_t *pl)
{
   volatile process_llist_node_t *l;
   printf("printing process linked list\n");
   for(l = pl->head; l != NULL; l = l->next)
   {
      printf("node at 0x%012lx:\n", (uint64_t)l);
      printf("   prev    at 0x%012lx:\n", (uint64_t)l->prev);
      printf("   next    at 0x%012lx:\n", (uint64_t)l->next);
      printf("   process at 0x%012lx (pid %u):\n", (uint64_t)l->p, l->p->pid);
      printf("   vruntime: 0x%016lx (%lu)\n", l->p->vruntime, l->p->vruntime);
   }
}



