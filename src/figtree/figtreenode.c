#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "figtreenode.h"
#include "interval.h"
#include "utils.h"

/* Fig Tree Entry */

bool fte_overlaps(struct ft_ent* this, struct ft_ent* other) {
    return i_overlaps(&this->irange, &other->irange);
}


/* Fig Tree Node */

void _ftn_entries_add(struct ft_node* this, int index, struct ft_ent* new) {
    ASSERT(index >= 0 && index <= this->entries_len &&
           this->entries_len < FT_SPLITLIMIT, "Bad index in _ftn_entries_add");
    memmove(&this->entries[index + 1], &this->entries[index],
            (this->entries_len - index) * sizeof(struct ft_ent));
    memcpy(&this->entries[index], new, sizeof(struct ft_ent));
    this->entries_len++;
}   

void _ftn_subtrees_add(struct ft_node* this, int index, struct ft_node* new) {
    ASSERT(index >= 0 && index <= this->subtrees_len &&
           this->subtrees_len <= FT_SPLITLIMIT,
           "Bad index in _ftn_subtrees_add");
    memmove(&this->subtrees[index + 1], &this->subtrees[index],
            (this->subtrees_len - index) * sizeof(struct ft_node*));
    subtree_set(&this->subtrees[index], new);
    this->subtrees_len++;
}

struct ft_node* ftn_new(int height, bool make_height) {
    struct ft_node* this;
    ASSERT(height >= 0, "Negative height in ftn_new");
    this = mem_alloc(sizeof(struct ft_node));
    this->HEIGHT = height;

    ftn_clear(this, make_height);
    
    return this;
}

void ftn_clear(struct ft_node* this, bool make_height) {
    struct ft_node* firstchild;
    int i;
    for (i = 0; i < this->subtrees_len; i++) {
        subtree_free(&this->subtrees[i]);
    }
    
    if (this->HEIGHT == 0 || !make_height) {
        firstchild = NULL;
    } else {
        firstchild = ftn_new(this->HEIGHT - 1, true);
    }
    this->entries_len = 0;
    subtree_set(&this->subtrees[0], firstchild);
    this->subtrees_len = 1;
}

struct ft_node* ftn_insert(struct ft_node* this, struct ft_ent* newent,
                           int index, struct ft_node* leftChild,
                           struct ft_node* rightChild) {
    ASSERT(this->entries_len + 1 == this->subtrees_len, "entries-subtree invariant violated in ftn_insert");
    ASSERT(index >= 0 && index <= this->entries_len &&
           (index == 0 || !fte_overlaps(newent, &this->entries[index - 1])) &&
           (index == this->entries_len ||
            !fte_overlaps(newent, &this->entries[index])), "bad ftn_insert");
    _ftn_entries_add(this, index, newent);
    subtree_set(&this->subtrees[index], leftChild);
    _ftn_subtrees_add(this, index + 1, rightChild);

    if (this->entries_len == FT_SPLITLIMIT) {
        // Split the node and push the middle entry to parent
        struct ft_node* left = ftn_new(this->HEIGHT, false);
        struct ft_node* right = ftn_new(this->HEIGHT, false);
        int i;

        left->entries_len = FT_ORDER;
        left->subtrees_len = FT_ORDER + 1;
        right->entries_len = FT_ORDER;
        right->subtrees_len = FT_ORDER + 1;
        left->subtrees[0] = this->subtrees[0];
        right->subtrees[0] = this->subtrees[FT_ORDER + 1];
        for (i = 0; i < FT_ORDER; i++) {
            left->entries[i] = this->entries[i];
            left->subtrees[i + 1] = this->subtrees[i + 1];
            right->entries[i] = this->entries[FT_ORDER + i + 1];
            right->subtrees[i + 1] = this->subtrees[FT_ORDER + i + 2];
        }

        /* This node no longer exists... so we might as well reuse its memory
         * for the node that got pushed up the tree.
         */
        this->entries[0] = this->entries[FT_ORDER];
        this->entries_len = 1;
        subtree_set(&this->subtrees[0], left);
        subtree_set(&this->subtrees[1], right);
        this->subtrees_len = 2;
        this->HEIGHT++;

        return this;
    }

    return NULL;
}

/* Replaces the entries from start, inclusive, to end, exclusive, with the
 * single provided entry.
 */
void ftn_replaceEntries(struct ft_node* this, int start, int end,
                        struct interval* newent_interval,
                        figtree_value_t newent_value) {
    int i;
    ASSERT(this->entries_len + 1 == this->subtrees_len, "entry-subtree invariant violated in ftn_replaceEntries");
    ASSERT(start >= 0 && start < this->entries_len
           && end >= 0 && end <= this->entries_len, "bad ftn_replaceEntries");
    ASSERT(end > start, "replaceEntries called on empty range");

    // Free the subtrees that are going to be dropped
    for (i = start + 1; i < end; i++) {
        subtree_free(&this->subtrees[i]);
    }
    
    memcpy(&this->entries[start].irange, newent_interval,
           sizeof(struct interval));
    this->entries[start].value = newent_value;
    
    memmove(&this->entries[start + 1], &this->entries[end],
            (this->entries_len - end) * sizeof(struct ft_ent));
    memmove(&this->subtrees[start + 1], &this->subtrees[end],
            (this->subtrees_len - end) * sizeof(struct ft_node*));

    this->entries_len -= (end - start - 1);
    this->subtrees_len -= (end - start - 1);
}

void ftn_pruneTo(struct ft_node* this, struct interval* valid) {
    struct ft_node* true_subtree;
    struct subtree_ptr* subtree;
    struct interval* entryint;
    
    bool entrydel[this->entries_len];
    bool subtreedel[this->subtrees_len];
    int i = 0;
    int j;
    
    if (!valid->nonempty) {
        ftn_clear(this, true);
        return;
    }

    if (this->entries_len == 0) {
        return;
    }

    memset(entrydel, 0x00, this->entries_len);
    memset(subtreedel, 0x00, this->subtrees_len);

    entryint = &this->entries[i].irange;
    subtree = &this->subtrees[i];

    while (i_leftOf_int(entryint, valid)) {
        entrydel[i] = true;
        subtreedel[i] = true;

        ASSERT(i < this->entries_len, "iterated past end of entries (#1)");
        if (++i == this->entries_len) {
            goto performdeletes;
        }
        entryint = &this->entries[i].irange;
        subtree = &this->subtrees[i];
    }

    if (i_leftOverlaps(valid, entryint)) {
        if ((true_subtree = subtree_get(subtree)) != NULL) {
            ftn_clear(true_subtree, true);
        }

        // In case the valid boundary is in the middle of this interval
        i_restrict_int(entryint, valid, false);

        ASSERT(i < this->entries_len, "iterated past end of entries (#2)");
        if (++i == this->entries_len) {
            goto performdeletes;
        }
        entryint = &this->entries[i].irange;
        subtree = &this->subtrees[i];
    }

    while (i_contains_int(valid, entryint)) {
        ASSERT(i < this->entries_len, "iterated past end of entries (#3)");
        if (++i == this->entries_len) {
            goto performdeletes;
        }
        entryint = &this->entries[i].irange;
        subtree = &this->subtrees[i];
    }

    /* At this point, entryint is either overlapping with the right edge of
     * VALID, or it is beyond VALID. The left subtree, in either case, cannot
     * be dropped, since part of it must be in the valid interval. So, just
     * skip over it. From now on, we need to index subtrees with i + 1 (so
     * that SUBTREE is the right subtree of ENTRY).
     */
    subtree = &this->subtrees[i + 1];

    if (i_rightOverlaps(valid, entryint)) {
        if ((true_subtree = subtree_get(subtree)) != NULL) {
            ftn_clear(true_subtree, true);
        }

        // In case the valid boundary is in the middle of this interval
        i_restrict_int(entryint, valid, false);

        ASSERT(i < this->entries_len, "iterated past end of entries (#4)");
        if (++i == this->entries_len) {
            goto performdeletes;
        }
        entryint = &this->entries[i].irange;
        subtree = &this->subtrees[i + 1];
    }

    while (i_rightOf_int(entryint, valid)) {
        entrydel[i] = true;
        subtreedel[i + 1] = true;

        ASSERT(i < this->entries_len, "iterated past end of entries (#5)");
        if (++i == this->entries_len) {
            goto performdeletes;
        }
        entryint = &this->entries[i].irange;
        // don't assign to subtree, since henceforth we only need to remove
    }   

    performdeletes:
    for (i = 0, j = 0; i < this->entries_len; i++) {
        if (!entrydel[i]) {
            this->entries[j] = this->entries[i];
            j++;
        }
    }
    this->entries_len = j;
    for (i = 0, j = 0; i < this->subtrees_len; i++) {
        if (subtreedel[i]) {
            subtree_free(&this->subtrees[i]);
        } else {
            this->subtrees[j] = this->subtrees[i];
            j++;
        }
    }
    this->subtrees_len = j;

    ASSERT(this->entries_len + 1 == this->subtrees_len,
           "entries-subtree invariant violated after pruning");
}

void ftn_free(struct ft_node* this) {
    /* First, free all subtrees. */
    int i;
    for (i = 0; i < this->subtrees_len; i++) {
        subtree_free(&this->subtrees[i]);
    }

    /* Then, free this node. */
    mem_free(this);
}
