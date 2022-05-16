#pragma once
#include <stdint.h>


#include <mmu.h>
#include <hart.h>
#include <lock.h>
// office (solved) hours moment: why not reinclude these files?
// Because it's important that the OS is not dependent on the SBI,
// so it should have its own maximum number of harts, etc.


#define PROCESS_DEFAULT_QUANTUM 100
#define PROCESS_DEFAULT_CONTEXT_TIMER 10000
#define PROCESS_DEFAULT_STACK_POINTER 0x1defa0000UL
#define PROCESS_UNDEFINED_PID    0xfffe - MAX_SUPPORTED_HARTS

typedef enum
{
   PS_DEAD,
   PS_UNINIT,
   PS_RUNNING,  // *eligible* to run
   PS_SLEEPING, // interruptible sleep
   PS_WAITING,  // uninterruptible sleep
   PS_ASYNC_FREE// The state of a process which needs to be deleted
} process_state_t;

typedef enum
{
   PS_PRIV_UNKNOWN = 0,
   PS_PRIV_USER,
   PS_PRIV_SUPERVISOR,
   PS_PRIV_MACHINE
} process_priv_t;


// set up to match asm/spawn.S
typedef struct process_frame
{
   uint64_t gpregs[32];
   double   fpregs[32];
   uint64_t sepc;
   uint64_t sstatus;
   uint64_t sie;
   uint64_t satp;
   uint64_t sscratch;
   uint64_t stvec;
   uint64_t trap_satp;
   uint64_t trap_stack;
} process_frame_t;


// struct taken from lecture notes
// trap frame at top makes assembly easy
typedef struct Process
{
   process_frame_t process_frame;
   process_state_t process_state;
   uint64_t        sleep_until;
   uint16_t        quantum;
   uint16_t        pid;
   void           *image;
   uint64_t        num_image_pages;
   void           *stack;
   uint64_t        num_stack_pages;
   void           *heap;
   uint64_t        num_heap_pages;
   page_table_t   *ptable;
   process_priv_t  privilege_mode;
   int             on_hart;
   uint64_t        vruntime;
} process_t;

void process_init_trap_stacks(void);

extern process_t *process_current[MAX_SUPPORTED_HARTS];
extern process_t *process_idle[MAX_SUPPORTED_HARTS];
extern uint64_t process_trap_stacks[MAX_SUPPORTED_HARTS];

process_t * process_init_new(process_priv_t mode);
void process_free(process_t *p);
int process_spawn(process_t *p, int target_hart);

int load_idle_process(process_t *p);

void process_print_frame_offsets(void);
void process_print_frame(process_t *p);

void process_top();

// for the scheduler
// assumed to be sorted in ascending order by vruntime
typedef struct process_llist_node
{
   process_t *p;
   volatile struct process_llist_node *next;
   volatile struct process_llist_node *prev;
} process_llist_node_t;

typedef struct process_llist_node_start
{
   volatile process_llist_node_t *head;
   mutex_t mutex;
} process_llist_node_start_t;

extern process_llist_node_start_t *unsched_process_llist;
extern process_llist_node_start_t *sleeping_process_llist;

void process_remove(process_llist_node_start_t *pl, process_t *remove);
process_t * process_remove_lowest(process_llist_node_start_t *pl);
void process_insert(process_llist_node_start_t *pl, process_t *p);
int process_llist_len(process_llist_node_start_t *pl);
void process_print_llist(process_llist_node_start_t *pl);
