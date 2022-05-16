#pragma once

#define PAGE_SIZE 4096

void page_init(void);
void * page_znalloc(int n);
void * page_zalloc(void);
void page_free(void *page);
void print_pages(void);
