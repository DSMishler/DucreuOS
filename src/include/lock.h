#pragma once

typedef int mutex_t;
typedef int sem_t;

int mutex_try(mutex_t *mutex);
void mutex_wait(mutex_t *mutex);
void mutex_post(mutex_t *mutex);

int sem_try(mutex_t *mutex);
void sem_wait(mutex_t *mutex);
void sem_post(mutex_t *mutex);

#define MUTEX_LOCKED 0
#define MUTEX_UNLOCKED 1

