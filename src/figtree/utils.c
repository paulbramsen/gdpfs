#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <ep/ep_app.h>

#include "utils.h"
#include "figtreenode.h"
#include "figtree.h"
#include "../gdpfs_log.h"
#include "../gdpfs_file.h"

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

/* Adds NODE's children to the buffer, and then adds NODE to the buffer. */
void get_dirty_helper(struct subtree_ptr* node, gdpfs_recno_t recno, struct ft_node** dirty, int* dirty_cap, int* dirty_len) {
    int i;
    struct ft_node* written;
    if (!node->inmemory || node->st == NULL || !node->st->dirty) {
        return;
    }
    
    /* Add NODE's children to the buffer. */
    for (i = 0; i < node->st->subtrees_len; i++) {
        get_dirty_helper(&node->st->subtrees[i], recno, dirty, dirty_cap, dirty_len);
    }
    
    /* Add NODE to the buffer. */
    if (*dirty_len == *dirty_cap) {
        (*dirty_cap) <<= 1;
        *dirty = ep_mem_realloc(*dirty, (*dirty_cap) * sizeof(struct ft_node));
    }
    
    written = &(*dirty)[*dirty_len];
    node->st->dirty = false;
    node->recno = recno;
    node->offset = (*dirty_len) * sizeof(struct ft_node);
    memcpy(written, node->st, sizeof(struct ft_node));
    
    /* Handle subtree pointers among node's children.
     * Pointers to NULL subtrees that occur at the leaves of the tree are
     * marked with the inmemory flag set to true and a null pointer.
     * All other pointers need to be adjusted so that they work as expected
     * when loaded from the GDP.
     */
    for (i = 0; i < written->subtrees_len; i++) {
        if (written->subtrees[i].st != NULL) {
            written->subtrees[i].inmemory = false;
            written->subtrees[i].st = NULL;
        } else {
            EP_ASSERT(written->subtrees[i].inmemory == true);
        }
    }
    
    
    (*dirty_len)++;
}

void get_dirty(struct ft_node** dirty, int* dirty_len, struct figtree* ft, gdpfs_recno_t chkpt_recno) {
    struct subtree_ptr root;
    int dirty_cap = 4;
    
    memset(&root, 0x00, sizeof(struct subtree_ptr));
    root.st = ft->root;
    root.inmemory = true;
    
    *dirty_len = 0;
    *dirty = ep_mem_zalloc(dirty_cap * sizeof(struct ft_node));
    
    get_dirty_helper(&root, chkpt_recno, dirty, &dirty_cap, dirty_len);
}

struct ft_node* subtree_get(struct subtree_ptr* sptr, gdpfs_log_t* log) {
    if (!sptr->inmemory) {
        // Need to go the GDP to get the subtree
        gdpfs_log_ent_t log_ent;
        gdpfs_fmeta_t entry;
        EP_STAT estat;
        size_t data_size;
        
        //printf("Going to the GDP to read subtree...\n");
        
        estat = gdpfs_log_ent_open(log, &log_ent, sptr->recno, false);
        if (!EP_STAT_ISOK(estat)) {
            printf("Recno is %ld\n", sptr->recno);
        }
        EP_ASSERT_INSIST(EP_STAT_ISOK(estat));
        
        data_size = gdpfs_log_ent_length(&log_ent);
        if (gdpfs_log_ent_read(&log_ent, &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
            || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
        {
            ep_app_fatal("Corrupt log entry in file.");
        }
        
        EP_ASSERT_REQUIRE(entry.logent_type == GDPFS_LOGENT_TYPE_CHKPT);
        
        gdpfs_log_ent_drain(&log_ent, sptr->offset);
        
        sptr->st = mem_alloc(sizeof(struct ft_node));
        
        gdpfs_log_ent_read(&log_ent, sptr->st, sizeof(struct ft_node));
    }
    return sptr->st;
}

void subtree_free(struct subtree_ptr* sptr) {
    if (sptr->inmemory && sptr->st != NULL) {
        ftn_free(sptr->st);
        sptr->st = NULL;
    }
}
