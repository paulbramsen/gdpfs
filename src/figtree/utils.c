#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "utils.h"
#include "figtreenode.h"
#include "../gdpfs_log.h"

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
    sptr->inmemory = true;
    sptr->st = st;
}

void subtree_clear(struct subtree_ptr* sptr, int height) {
    if (height < 0) {
        return;
    }
    
    if (sptr->inmemory) {
        if (sptr->st != NULL) {
            ASSERT(height == sptr->st->HEIGHT, "Heights don't match");
            ftn_clear(sptr->st, true);
        }
    } else {
        sptr->inmemory = true;
        sptr->recno = 0;
        sptr->offset = 0;
        sptr->st = ftn_new(height, true);
    }
}

struct ft_node* subtree_get(struct subtree_ptr* sptr, gdpfs_log_t* log) {
    if (!sptr->inmemory) {
        // Need to go the GDP to get the subtree
        
    }
    return sptr->st;
}

void subtree_free(struct subtree_ptr* sptr) {
    if (sptr->inmemory && sptr->st != NULL) {
        ftn_free(sptr->st);
        sptr->st = NULL;
    }
}
