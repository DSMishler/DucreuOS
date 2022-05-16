#include <sched.h>
#include <printf.h>
#include <process.h>
#include <kmalloc.h>
#include <utils.h>
#include <hart.h>
#include <sbi.h>
#include <lock.h>

/* Daniel Mishler implementation of CFS - Linux Completely Fair Scheduler */

mutex_t sched_mutex = MUTEX_LOCKED;

void sched_init(void)
{
   // TODO: move to process init?
   unsched_process_llist = kmalloc(sizeof(process_llist_node_start_t));
   unsched_process_llist->head = NULL;
   unsched_process_llist->mutex = MUTEX_UNLOCKED;
   sleeping_process_llist = kmalloc(sizeof(process_llist_node_start_t));
   sleeping_process_llist->head = NULL;
   sleeping_process_llist->mutex = MUTEX_UNLOCKED;
   int i;
   for(i = 0; i < MAX_SUPPORTED_HARTS; i++)
   {
      process_t *p = process_init_new(PS_PRIV_SUPERVISOR);
      if(load_idle_process(p))
      {
         ERRORMSG("load process error");
      }
      sched_assign_idle_pid(i, p);
      process_idle[i] = p;
      if(i != 0)
      {
         process_spawn(p, i);
      }
   }
   // mutex_t sched_mutex = MUTEX_UNLOCKED;
   mutex_post(&sched_mutex);
   return;
}

void sched_invoke(int hart)
{
   mutex_wait(&sched_mutex);

   static int paranoia_counter = 1;
   paranoia_counter++;
   int pcounter_start = paranoia_counter;
   sched_walk_sleeping_list(sleeping_process_llist, unsched_process_llist);
   // the scheduler was just called on this hart. There might be a process on
   // this hart to remove first
   if(hart != sbi_who_am_i())
   {
      printf("warning: scheduler called for another hart. ");
      printf("Hope you know what you're doing\n");
   }
   // printf("scheduler invoked for hart %d\n", hart);
   // process_print_llist(unsched_process_llist);
   process_t *p = process_current[hart];
   if(p == NULL)
   {
      ERRORMSG("no process on this hart!");
      printf("proceeding as if the idle process was running\n");
      p = process_idle[hart];
   }
   if(p->on_hart != hart)
   {
      ERRORMSG("hart mismatch");
   }
   p->on_hart = -1;
   process_current[hart] = NULL;
   // free the process if it was ASYNC_FREE
   if(p->process_state == PS_ASYNC_FREE)
   {
      printf("freeing process\n");
      process_free(p);
   }
   else if(p->process_state == PS_SLEEPING)
   {
      sched_sleep(p);
   }
   else
   {
      // add it back into the queue (unless it was an idle process)
      if(p == process_idle[hart])
      {
         ;
      }
      else
      {
         sched_add(p);
      }
   }
   process_t *new = sched_get();
   if(new == NULL)
   {
      new = process_idle[hart];
   }
   if(paranoia_counter != pcounter_start)
   {
      ERRORMSG("panic");
   }
   mutex_post(&sched_mutex);
   process_spawn(new, hart);
}

void sched_walk_sleeping_list(process_llist_node_start_t *sleeping_llist,
                              process_llist_node_start_t *running_llist)
{
   uint64_t time = sbi_get_clint_time();
   volatile process_llist_node_t *pln;
   pln = sleeping_llist->head;
   for(; pln != NULL; pln=pln->next)
   {
      if(pln->p->process_state != PS_SLEEPING)
      {
         ERRORMSG("nonsleeping process in sleeping list");
      }
      if(pln->p->sleep_until < time)
      {
         // DEBUGMSG("removing process from sleep");
         process_t *target = pln->p;
         target->process_state = PS_RUNNING;
         process_remove(sleeping_llist, target);
         process_insert(running_llist, target);
         /* start walking the list again */
         // TODO: I think the re-starting of the list walk is sloppy. Try
         //       another method?
         pln = sleeping_llist->head;
         if(pln == NULL)
         {
            break;
         }
      }
   }
   // DEBUGMSG("function return");
   return;
}

void sched_assign_pid(process_t *p)
{
   static int sched_pid = 2;
   p->pid = sched_pid;
   // now properly replace the satp.
   sched_pid += 1;
   p->process_frame.satp =      SATP_MODE_ON  |
                        ASID_TO_SATP(p->pid)  |
                        PPN_TO_SATP(p->ptable);
}

void sched_assign_idle_pid(int hart, process_t *p)
{
   uint64_t idle_pid = 0xfffe - hart;
   p->pid = idle_pid;
   // now properly replace the satp.
   p->process_frame.satp =      SATP_MODE_ON  |
                        ASID_TO_SATP(p->pid)  |
                        PPN_TO_SATP(p->ptable);
}

void sched_add(process_t *p)
{
   process_insert(unsched_process_llist, p);
}

void sched_sleep(process_t *p)
{
   process_insert(sleeping_process_llist, p);
}

// if a process is available to be run: get that process.
// Else: return NULL for the idle process
process_t * sched_get(void)
{
   return process_remove_lowest(unsched_process_llist);
}
