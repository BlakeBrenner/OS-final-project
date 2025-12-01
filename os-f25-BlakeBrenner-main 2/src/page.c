#include "page.h"

// Static descriptor array (128 * 2 MiB = 256 MiB of pages)
static struct ppage physical_page_array[128];

// Global free list head
struct ppage *free_list_head = NULL;

/* ---------- Internal helpers ---------- */

static void list_push_front(struct ppage **head, struct ppage *node) {
    node->prev = NULL;
    node->next = *head;
    if (*head)
        (*head)->prev = node;
    *head = node;
}

static struct ppage *list_pop_front(struct ppage **head) {
    if (!*head)
        return NULL;
    struct ppage *n = *head;
    *head = n->next;
    if (*head)
        (*head)->prev = NULL;
    n->next = n->prev = NULL;
    return n;
}

static struct ppage *list_tail(struct ppage *head) {
    while (head && head->next)
        head = head->next;
    return head;
}

static unsigned int list_length(struct ppage *head) {
    unsigned int count = 0;
    while (head) {
        count++;
        head = head->next;
    }
    return count;
}

/* ---------- Public API ---------- */

void init_pfa_list(void) {
    free_list_head = NULL;
    for (unsigned int i = 0; i < (sizeof(physical_page_array) / sizeof(physical_page_array[0])); ++i) {
        struct ppage *pp = &physical_page_array[i];
        pp->next = pp->prev = NULL;
        pp->physical_addr = (void *)(uintptr_t)(i * (uintptr_t)PFA_PAGE_BYTES);
        list_push_front(&free_list_head, pp);
    }
}

struct ppage *allocate_physical_pages(unsigned int npages) {
    if (npages == 0)
        return NULL;

    struct ppage *alloc_head = NULL;
    struct ppage *alloc_tail = NULL;

    for (unsigned int i = 0; i < npages; ++i) {
        struct ppage *page = list_pop_front(&free_list_head);
        if (!page) {
            // Roll back already allocated pages
            if (alloc_head)
                free_physical_pages(alloc_head);
            return NULL;
        }

        if (!alloc_head)
            alloc_head = alloc_tail = page;
        else {
            alloc_tail->next = page;
            page->prev = alloc_tail;
            alloc_tail = page;
        }
    }

    return alloc_head;
}

void free_physical_pages(struct ppage *ppage_list) {
    if (!ppage_list)
        return;

    struct ppage *tail = list_tail(ppage_list);
    tail->next = free_list_head;
    if (free_list_head)
        free_list_head->prev = tail;

    ppage_list->prev = NULL;
    free_list_head = ppage_list;
}

unsigned int pfa_free_count(void) {
    return list_length(free_list_head);
}
