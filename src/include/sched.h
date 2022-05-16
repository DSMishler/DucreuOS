#pragma once

#include <process.h>
#include <stdint.h>

void sched_init(void);
void sched_invoke(int hart);
void sched_walk_sleeping_list(process_llist_node_start_t *sleeping_llist,
                              process_llist_node_start_t *running_llist);
void sched_assign_pid(process_t *p);
void sched_assign_idle_pid(int hart, process_t *p);
void sched_add(process_t *p);
void sched_sleep(process_t *p);
process_t * sched_get(void);
