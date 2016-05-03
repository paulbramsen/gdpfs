#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <ep/ep_mem.h>

#include "utils.h"
#include "figtree.h"

/* Allows certain functionality to be customized. In particular, allows lazy
 * loading of Fig Tree Nodes.
 */

void* mem_alloc(size_t s) {
    return ep_mem_zalloc(s);
}

void mem_free(void* ptr) {
    ep_mem_free(ptr);
}

void subtree_set(struct subtree_ptr* sptr, struct ft_node* st) {
    sptr->st = st;
}

struct ft_node* subtree_get(struct subtree_ptr* sptr) {
    return sptr->st;
}

void subtree_free(struct subtree_ptr* sptr) {
    if (sptr->st != NULL) {
        ftn_free(sptr->st);
        sptr->st = NULL;
    }
}
