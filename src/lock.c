#include <lock.h>


/* return 1 if the lock was successfully acquired */
int mutex_try(mutex_t *mutex)
{
   mutex_t previous;
   asm volatile("amoswap.w.aq %0, %1, (%2)" : "=r"(previous) :
                   "r"(MUTEX_LOCKED), "r"(mutex));
   return (previous == MUTEX_LOCKED ? 0 : 1);
}
/* halt until the 'lock' is available. Lock atomically before prodeeding. */
void mutex_wait(mutex_t *mutex)
{
   while(mutex_try(mutex) != 1)
   {
      ;
   }
}

void mutex_post(mutex_t *mutex)
{
   mutex_t previous;
   asm volatile("amoswap.w.aq %0, %1, (%2)" : "=r"(previous) :
                   "r"(MUTEX_UNLOCKED), "r"(mutex));
   // no need to double check things
   return;
}


int sem_try(sem_t *sem)
{
   sem_t previous;
   asm volatile("amoadd.w.aq %0, %1, (%2)" : "=r"(previous) :
                   "r"(-1), "r"(sem));
   if(previous <= 0)
   {
      // we failed to get the lock, but we will replace the value with a post
      sem_post(sem);
      return 0;
   }
   // else
   return 1;
}
/* halt until the 'lock' is available. Lock atomically before prodeeding. */
void sem_wait(sem_t *sem)
{
   while(sem_try(sem) != 1)
   {
      ;
   }
}

void sem_post(sem_t *sem)
{
   sem_t previous;
   asm volatile("amoadd.w.aq %0, %1, (%2)" : "=r"(previous) :
                   "r"(1), "r"(sem));
   // no need to double check things
   return;
}


