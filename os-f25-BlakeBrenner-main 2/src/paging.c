#include "paging.h"

/* ===== Global paging structures (must be global + 4096-aligned) ===== */
struct page_directory_entry kernel_pd[PD_ENTRIES] __attribute__((aligned(4096)));
struct page kernel_pt_pool[PT_POOL_COUNT][PT_ENTRIES] __attribute__((aligned(4096)));
static uint32_t kernel_pt_count = 0;

/* ===== Helpers ===== */
static inline uint32_t align_down(uint32_t x, uint32_t a) { return x & ~(a - 1u); }
static inline uint32_t vaddr_pdi(uint32_t v) { return (v >> 22) & 0x3FFu; }
static inline uint32_t vaddr_pti(uint32_t v) { return (v >> 12) & 0x3FFu; }

static void bzero_bytes(void *dst, uint32_t bytes) {
    uint32_t *p = (uint32_t*)dst;
    for (uint32_t n = bytes >> 2; n; --n) { *p++ = 0; }
    uint8_t *q = (uint8_t*)p;
    for (uint32_t i = 0; i < (bytes & 3u); ++i) q[i] = 0;
}

static inline void invlpg(void *addr) {
    __asm__ __volatile__("invlpg (%0)" :: "r"(addr) : "memory");
}

/* Create (& zero) a new page table from the static pool; return NULL if pool exhausted. */
static struct page* alloc_pt_from_pool(void) {
    if (kernel_pt_count >= PT_POOL_COUNT) return 0;
    struct page *pt = &kernel_pt_pool[kernel_pt_count++][0];
    bzero_bytes(pt, sizeof(kernel_pt_pool[0]));
    return pt;
}

/* Ensure a PT exists for the given PDE index.
   If absent, allocate one from the pool and wire the PDE for 4KiB pages. */
static struct page* ensure_pt(struct page_directory_entry *pd, uint32_t pdi) {
    if (pd[pdi].present) {
        uint32_t pt_phys = pd[pdi].frame << 12;
        return (struct page*)pt_phys;  // prior to paging, identity assumption is fine
    }
    struct page *pt = alloc_pt_from_pool();
    if (!pt) return 0;

    pd[pdi].present       = 1;
    pd[pdi].rw            = 1;
    pd[pdi].user          = 0;
    pd[pdi].writethru     = 0;
    pd[pdi].cachedisabled = 0;
    pd[pdi].accessed      = 0;
    pd[pdi].pagesize      = 0;                 // 4 KiB pages
    pd[pdi].ignored       = 0;
    pd[pdi].os_specific   = 0;
    pd[pdi].frame         = ((uint32_t)(uintptr_t)pt) >> 12; // physical >> 12

    return pt;
}

/* Map a single 4 KiB page: VA -> PA (helper for map_pages) */
static void map_4k(struct page_directory_entry *pd, uint32_t va, uint32_t pa) {
    uint32_t pdi = vaddr_pdi(va);
    uint32_t pti = vaddr_pti(va);

    struct page *pt = ensure_pt(pd, pdi);
    if (!pt) return; // out of PTs, silently drop

    pt[pti].present  = 1;
    pt[pti].rw       = 1;
    pt[pti].user     = 0;
    pt[pti].accessed = 0;
    pt[pti].dirty    = 0;
    pt[pti].unused   = 0;
    pt[pti].frame    = (pa >> 12);
}

/* ===== Assignment function: map a linked list of physical pages at vaddr ===== */
void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd) {
    uint32_t va = align_down((uint32_t)(uintptr_t)vaddr, PAGE_SIZE);

    for (struct ppage *cur = pglist; cur; cur = cur->next) {
        uint32_t pa_base = (uint32_t)(uintptr_t)cur->physical_addr;
        for (uint32_t off = 0; off < PFA_PAGE_BYTES; off += PAGE_SIZE) {
            map_4k(pd, va, pa_base + off);
            va += PAGE_SIZE;
        }
    }
    return (void*)align_down((uint32_t)(uintptr_t)vaddr, PAGE_SIZE);
}

/* ===== Control registers ===== */
void loadPageDirectory(struct page_directory_entry *pd) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pd) : "memory");
}
void enablePaging(void) {
    __asm__ __volatile__(
        "mov %%cr0, %%eax\n"
        "or  $0x80000001, %%eax\n"  /* CR0.PE | CR0.PG */
        "mov %%eax, %%cr0\n"
        ::: "eax", "memory"
    );
}

/* ===== Recursive paging: set PDE[1023] to point to PD itself ===== */
void paging_init_recursive(struct page_directory_entry *pd) {
    pd[1023].present       = 1;
    pd[1023].rw            = 1;
    pd[1023].user          = 0;
    pd[1023].writethru     = 0;
    pd[1023].cachedisabled = 0;
    pd[1023].accessed      = 0;
    pd[1023].pagesize      = 0; // 4 KiB pages
    pd[1023].ignored       = 0;
    pd[1023].os_specific   = 0;
    pd[1023].frame         = ((uint32_t)(uintptr_t)pd) >> 12; // PD maps itself
}

/* ===== Functions that use the recursive mapping =====
   Requires: paging enabled AND paging_init_recursive() called before CR3 load. */

static inline int is_present(uint32_t entry) { return entry & 0x001; }

/* Translate VA -> PA; returns NULL if PDE/PTE not present. */
void *get_physaddr(void *virtualaddr) {
    unsigned long va = (unsigned long)virtualaddr;
    unsigned long pdindex = va >> 22;
    unsigned long ptindex = (va >> 12) & 0x03FF;

    volatile unsigned long *pd = (unsigned long *)0xFFFFF000; // PD via recursive map
    if (!is_present(pd[pdindex])) return (void*)0;

    volatile unsigned long *pt = (unsigned long *)0xFFC00000 + (0x400 * pdindex);
    if (!is_present(pt[ptindex])) return (void*)0;

    unsigned long frame = pt[ptindex] & ~0xFFFUL;
    unsigned long off   = va & 0xFFFUL;
    return (void *)(frame + off);
}

/* Map exactly one 4 KiB page: physaddr -> virtualaddr with low 12-bit flags.
   If the PT is missing, allocate from the static pool and wire the PDE. */
int map_page(void *physaddr, void *virtualaddr, unsigned int flags) {
    unsigned long pa = (unsigned long)physaddr;
    unsigned long va = (unsigned long)virtualaddr;

    if ((pa & 0xFFFUL) || (va & 0xFFFUL)) return -1; // require alignment

    unsigned long pdindex = va >> 22;
    unsigned long ptindex = (va >> 12) & 0x03FF;

    // Access PD via recursive map
    volatile unsigned long *pd = (unsigned long *)0xFFFFF000;

    // If PDE is absent, create a new PT from our pool and wire it
    if (!is_present(pd[pdindex])) {
        struct page *newpt = alloc_pt_from_pool();
        if (!newpt) return -2; // out of PTs
        // Zero already done in allocator; now wire PDE (present|rw, user=0, 4KiB)
        pd[pdindex] = (((unsigned long)(uintptr_t)newpt) & ~0xFFFUL) | 0x003UL;
        // After this write, that PT appears at 0xFFC00000 + pdindex*0x1000 immediately
    }

    volatile unsigned long *pt = (unsigned long *)0xFFC00000 + (0x400 * pdindex);

    // If an existing mapping is present, you can choose to overwrite or error
    // if (is_present(pt[ptindex])) return -3; // uncomment to disallow remap

    pt[ptindex] = (pa & ~0xFFFUL) | (flags & 0xFFFUL) | 0x001UL; // set Present
    invlpg((void*)va);
    return 0;
}
/* Identity map a range of physical addresses (phys addr = virt addr)
   Used during early boot before higher-half kernel */
void identity_map_range(uint32_t start, uint32_t end) {
    // Align start down to page boundary
    start = start & ~(PAGE_SIZE - 1);
    
    // Align end up to page boundary  
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Map each 4KB page in the range
    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t pdi = (addr >> 22) & 0x3FF;  // Page directory index
        uint32_t pti = (addr >> 12) & 0x3FF;  // Page table index
        
        // Ensure page table exists for this PDE
        if (!kernel_pd[pdi].present) {
            // Allocate a page table from the static pool
            if (kernel_pt_count >= PT_POOL_COUNT) {
                // Out of page tables - silently fail for now
                // In a real system, you'd panic or handle this
                continue;
            }
            
            struct page *pt = &kernel_pt_pool[kernel_pt_count++][0];
            
            // Zero out the page table
            for (int i = 0; i < PT_ENTRIES; i++) {
                pt[i].present = 0;
                pt[i].rw = 0;
                pt[i].user = 0;
                pt[i].accessed = 0;
                pt[i].dirty = 0;
                pt[i].unused = 0;
                pt[i].frame = 0;
            }
            
            // Setup the page directory entry
            kernel_pd[pdi].present = 1;
            kernel_pd[pdi].rw = 1;
            kernel_pd[pdi].user = 0;
            kernel_pd[pdi].writethru = 0;
            kernel_pd[pdi].cachedisabled = 0;
            kernel_pd[pdi].accessed = 0;
            kernel_pd[pdi].pagesize = 0;  // 4KB pages
            kernel_pd[pdi].ignored = 0;
            kernel_pd[pdi].os_specific = 0;
            kernel_pd[pdi].frame = ((uint32_t)pt) >> 12;
        }
        
        // Get the page table (before paging is enabled, physical = virtual)
        struct page *pt = (struct page*)(kernel_pd[pdi].frame << 12);
        
        // Setup the page table entry for identity mapping (phys = virt)
        pt[pti].present = 1;
        pt[pti].rw = 1;
        pt[pti].user = 0;
        pt[pti].accessed = 0;
        pt[pti].dirty = 0;
        pt[pti].unused = 0;
        pt[pti].frame = addr >> 12;  // Identity: virt addr = phys addr
    }
}
