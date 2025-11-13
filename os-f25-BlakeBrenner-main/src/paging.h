#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "page.h"   // for struct ppage (your allocator node)

/* ===== i386 paging entry formats (exactly per assignment) ===== */
struct page_directory_entry
{
   uint32_t present       : 1;   // Page present in memory
   uint32_t rw            : 1;   // Read-only if clear, R/W if set
   uint32_t user          : 1;   // Supervisor only if clear
   uint32_t writethru     : 1;   // Cache this directory as write-thru only
   uint32_t cachedisabled : 1;   // Disable cache on this page table?
   uint32_t accessed      : 1;   // Accessed
   uint32_t pagesize      : 1;   // 0 => 4 KiB pages
   uint32_t ignored       : 2;
   uint32_t os_specific   : 3;
   uint32_t frame         : 20;  // physical address >> 12 of the page table
};

struct page
{
   uint32_t present  : 1;   // Page present in memory
   uint32_t rw       : 1;   // Read-only if clear, readwrite if set
   uint32_t user     : 1;   // Supervisor level only if clear
   uint32_t accessed : 1;   // Has the page been accessed
   uint32_t dirty    : 1;   // Has the page been written
   uint32_t unused   : 7;
   uint32_t frame    : 20;  // physical address >> 12 of the 4 KiB frame
};

/* ===== Constants ===== */
#define PAGE_SIZE    4096u
#define PD_ENTRIES   1024u
#define PT_ENTRIES   1024u

/* If your frame allocator returns >4KiB blocks (e.g., 2MiB), define PFA_PAGE_BYTES in page.h.
   Otherwise we default to 4 KiB. */
#ifndef PFA_PAGE_BYTES
#define PFA_PAGE_BYTES PAGE_SIZE
#endif

/* ===== Global, 4096-byte aligned paging structures ===== */
extern struct page_directory_entry kernel_pd[PD_ENTRIES] __attribute__((aligned(4096)));

/* Small static pool of page tables for on-demand creation (each 4 KiB aligned) */
#ifndef PT_POOL_COUNT
#define PT_POOL_COUNT 64
#endif
extern struct page kernel_pt_pool[PT_POOL_COUNT][PT_ENTRIES] __attribute__((aligned(4096)));

/* ===== Assignment API ===== */

/* Map a linked list of physical pages (pglist) starting at vaddr.
   Returns the (page-aligned) virtual address mapped. */
void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd);

/* Load CR3 (PD base, must be physical & 4KiB aligned) */
void loadPageDirectory(struct page_directory_entry *pd);

/* Enable paging: set CR0.PE (bit 0) and CR0.PG (bit 31) */
void enablePaging(void);

/* ===== Recursive paging support =====
   Call this ONCE during paging setup (before loadPageDirectory) to set PDE[1023] to point to PD.
   After enabling paging, the PD is visible at 0xFFFFF000 and PT[i] at 0xFFC00000 + i*0x1000. */
void paging_init_recursive(struct page_directory_entry *pd);

/* ===== Convenience functions that rely on recursive mapping =====
   These match the style you asked for. They require paging enabled and PDE[1023] set. */

/* Translate a virtual address to a physical address; returns NULL if not present. */
void *get_physaddr(void *virtualaddr);

/* Map a single 4KiB page: physaddr -> virtualaddr with low 12-bit flags (e.g., 0x003 for R/W|Present).
   Allocates a page table from the static pool if the PDE is not present.
   Returns 0 on success, negative on error. */
int map_page(void *physaddr, void *virtualaddr, unsigned int flags);

#endif /* PAGING_H */
