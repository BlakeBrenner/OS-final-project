#ifndef PAGE_H
#define PAGE_H

#include <stddef.h>   // for size_t, NULL
#include <stdint.h>   // for uintptr_t, uint32_t

// Each page is 2 MiB
#define PFA_PAGE_BYTES (2u * 1024u * 1024u)

// Page descriptor structure
struct ppage {
    struct ppage *next;   // next page in list
    struct ppage *prev;   // previous page in list
    void *physical_addr;  // physical start address of this page
};

// Global head of the free list (defined in page.c)
extern struct ppage *free_list_head;

// Initializes the allocator and builds the free list
void init_pfa_list(void);

// Allocates npages from the free list
struct ppage *allocate_physical_pages(unsigned int npages);

// Frees a list of physical pages
void free_physical_pages(struct ppage *ppage_list);

// Returns the number of pages currently on the free list
unsigned int pfa_free_count(void);

#endif // PAGE_H
