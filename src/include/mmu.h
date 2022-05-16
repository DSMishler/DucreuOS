#pragma once

#include<stdint.h>
#include<stdbool.h> // never to late to start using these


typedef struct page_table
{
   uint64_t entries[512];
} page_table_t;

// macros copied over from from Dr. Marz' code
// PTEB = page table entry bits
#define PTEB_NONE      0
#define PTEB_VALID     (1UL << 0)
#define PTEB_READ      (1UL << 1)
#define PTEB_WRITE     (1UL << 2)
#define PTEB_EXECUTE   (1UL << 3)
#define PTEB_USER      (1UL << 4)
#define PTEB_GLOBAL    (1UL << 5)
#define PTEB_ACCESS    (1UL << 6)
#define PTEB_DIRTY     (1UL << 7)

#define SATP_MODE_ON   (8UL << 60)
#define SATP_MODE_OFF  0
#define SATP_ASID_BIT  44
#define PPN_TO_SATP(x)  ((((uint64_t)x) >> 12) & 0xFFFFFFFFFFFUL) // 44 bit
#define ASID_TO_SATP(x) ((((uint64_t)x) & 0xFFFF) << 44)
#define KERNEL_ASID 0xffff

int mmu_map(page_table_t *pt, uint64_t vaddr, uint64_t paddr, uint64_t bits);
int mmu_unmap(page_table_t *pt, uint64_t vaddr);
int mmu_set(page_table_t *pt, uint64_t vaddr, uint64_t paddr, uint64_t bits);
void mmu_free(page_table_t *pt);
uint64_t mmu_translate(page_table_t *pt, uint64_t vaddr);
void mmu_map_multiple(page_table_t *pt, uint64_t start_vaddr,
                       uint64_t start_paddr, uint64_t bytes, uint64_t bits);
void mmu_show_page_table(page_table_t *base,
                         page_table_t *pt,
                         int level,
                         uint64_t vaddr);


extern page_table_t *kernel_mmu_table;
