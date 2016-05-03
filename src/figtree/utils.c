#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <ep/ep_app.h>

#include "utils.h"
#include "figtreenode.h"
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

struct ft_node* subtree_get(struct subtree_ptr* sptr, gdpfs_log_t* log) {
    if (!sptr->inmemory) {
        // Need to go the GDP to get the subtree
        gdpfs_log_ent_t log_ent;
        gdpfs_fmeta_t entry;
        EP_STAT estat;
        size_t data_size;
        
        printf("Going to the GDP to read subtree...\n");
        
        estat = gdpfs_log_ent_open(log, &log_ent, sptr->recno);
        EP_ASSERT_INSIST(EP_STAT_ISOK(estat));
        
        data_size = gdpfs_log_ent_length(&log_ent);
        if (gdpfs_log_ent_read(&log_ent, &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
            || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
        {
            ep_app_fatal("Corrupt log entry in file.");
        }
        
        EP_ASSERT_REQUIRE(entry.logent_type == GDPFS_LOGENT_TYPE_CHKPT);
        
        gdpfs_log_ent_drain(&log_ent, sptr->offset);
        
        sptr->st = ftn_new(0, false);
        
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
