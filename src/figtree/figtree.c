#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "figtree.h"
#include "figtreenode.h"
#include "interval.h"
#include "utils.h"

/* Fig Tree */

void ft_init(struct figtree* this) {
    this->root = ftn_new(0, true);
}

struct insertargs {
    struct interval range;
    figtree_value_t value;
    struct ft_node* at;
    struct interval valid;
};

struct insertcont {
    bool hasleftc;
    struct insertargs leftc;
    bool hasrightc;
    struct insertargs rightc;
};

void _ft_insert(struct figtree* this, struct insertargs* args,
                struct ft_node** path, int* pathIndices, int* path_len,
                bool rightcontinuation, struct insertcont* ic) {
    struct interval* range = &args->range;
    figtree_value_t value = args->value;
    struct ft_node* currnode = args->at;
    struct interval* valid = &args->valid;

    int finalsharedindex = (*path_len) - 1;

    /* Record the residual groups [star1, range->left - 1] and
       [range->right + 1, star2]. */

    int numentries, i;

    memset(ic, 0x00, sizeof(struct insertcont));

    outerloop:
    while (currnode != NULL) {
        struct ft_ent* current;
        struct interval* previval;
        struct interval* currival;
        ftn_pruneTo(currnode, valid);
        numentries = currnode->entries_len;
        current = NULL;
        currival = NULL;

        for (i = 0; i < numentries; i++) {
            previval = currival;
            current = &currnode->entries[i];
            currival = &current->irange;
            if (i_overlaps(currival, range)) {
                int j;
                struct ft_ent* previous;

                // We increment *path_len later
                path[*path_len] = currnode;
                if (currival->left < range->left) {
                    // Create a continuation for the left subinterval
                    ic->hasleftc = true;
                    i_init(&ic->leftc.range, currival->left, range->left - 1);
                    ic->leftc.value = current->value;
                    ic->leftc.at = subtree_get(&currnode->subtrees[i]);
                    memcpy(&ic->leftc.valid, valid, sizeof(struct interval));
                    i_restrict_range(&ic->leftc.valid, i == 0 ? BYTE_INDEX_MIN :
                                     previval->right + 1,
                                     currival->left - 1, true);
                }
                
                /* The entry in this node immediately after current will either
                 * be disjoint from RANGE, or will left-overlap it. It can't
                 * right-overlap it (unless it also left-overlaps it).
                 */
                previous = current;
                for (j = i + 1; j < numentries &&
                         i_leftOverlaps(&(current =
                                          &currnode->entries[j])->irange,
                                        range); j++) {
                    previous = current;
                }

                /* Now, either CURRENT is the first entry in the node that is
                 * disjoint from RANGE, or, if there is no such entry, then
                 * j == numentries. In either case, PREVIOUS is the last entry
                 * in the node that overlaps with RANGE.
                 */

                if (previous->irange.right > range->right) {
                    // Create a continuation for the right subinterval
                    ic->hasrightc = true;
                    i_init(&ic->rightc.range, range->right + 1,
                           previous->irange.right);
                    ic->rightc.value = previous->value;
                    /* After we replace entries i ... j - 1 with the new
                     * entry, we need to continue with what is now subtree j.
                     */
                    ic->rightc.at = subtree_get(&currnode->subtrees[j]);
                    memcpy(&ic->rightc.valid, valid, sizeof(struct interval));
                    i_restrict_range(&ic->rightc.valid,
                                     previous->irange.right + 1,
                                     j == numentries ? BYTE_INDEX_MAX :
                                     current->irange.left - 1, true);
                    /* If there's a right continuation, then we set the
                     * path index to that of the right continuation. */
                    /* If there's also a left continuation, then we adjust the
                     * index for the left continuation. */
                    pathIndices[(*path_len)++] = i + 1;
                } else {
                    /* There's no right continuation that will adjust the final
                     * shared path index for the left continuation. So, we need
                     * to directly insert the index for the left continuation
                     * here, in case there is a left continuation.
                     */
                    pathIndices[(*path_len)++] = i;
                }

                /* Now that we have created the continuations, we can replace
                 * the appropriate entries in the node with the new one.
                 */
                ftn_replaceEntries(currnode, i, j, range, value);
                
                goto treeinsertion;
            } else if (i_rightOf_int(currival, range)) {
                path[*path_len] = currnode;
                pathIndices[(*path_len)++] = i;
                currnode = subtree_get(&currnode->subtrees[i]);
                /* What if previval and currival are adjacent intervals? Then
                 * the entire subtree can be pruned. This is represented by the
                 * special empty interval.
                 */
                i_restrict_range(valid, previval == NULL ? BYTE_INDEX_MIN :
                                 previval->right + 1, currival->left - 1, true);
                goto outerloop;
            }
        }
        path[*path_len] = currnode;
        pathIndices[(*path_len)++] = numentries;
        currnode = subtree_get(&currnode->subtrees[numentries]);
        i_restrict_range(valid, currival == NULL ? BYTE_INDEX_MIN :
                         currival->right + 1, BYTE_INDEX_MAX, true);
    }

    treeinsertion:
    if (currnode == NULL) {
        // In this case, we actually need to do an insertion
        struct ft_ent toinsert;
        struct ft_node* rv = NULL;
        struct ft_node* topushnode = NULL;
        struct ft_ent* topushent = &toinsert;
        struct ft_node* left = NULL;
        struct ft_node* right = NULL;
        struct ft_node* insertinto;
        int insertindex;
        int pathindex;

        memcpy(&toinsert.irange, range, sizeof(struct interval));
        toinsert.value = value;

        for (pathindex = (*path_len) - 1; pathindex >= 0; pathindex--) {
            insertinto = path[pathindex];
            insertindex = pathIndices[pathindex];
            rv = ftn_insert(insertinto, topushent, insertindex, left, right);
            mem_free(topushnode);
            topushnode = rv;
            
            if (rightcontinuation) {
                /*
                 * All indices in the pathIndices and path lists at or before
                 * FINALSHAREDINDEX are shared with the path in a left
                 * continuation that has not yet been executed. If any nodes get
                 * split along that path, we need to update the path
                 * accordingly.
                 * 
                 * Special case: we need to artificially decrement the stored
                 * insertindex at the FINALSHAREDINDEX because the left
                 * continuation takes the left subtree of the primary range
                 * (whereas we took the right branch).
                 */
                if (pathindex == finalsharedindex) {
                    pathIndices[pathindex] = --insertindex;
                } else if (pathindex < finalsharedindex) {
                    struct ft_node* nextpathmember = path[pathindex + 1];
                    if (nextpathmember == right) {
                        pathIndices[pathindex] = ++insertindex;
                    }
                }

                if (topushnode != NULL) {
                    /* If this node is being split, then there are some
                     * complications. We need to change the node itself along
                     * the path. We also need to adjust the index accordingly.
                     */
                    if (insertindex <= FT_ORDER) {
                        path[pathindex] = subtree_get(&topushnode->subtrees[0]);
                    } else {
                        path[pathindex] = subtree_get(&topushnode->subtrees[1]);
                        pathIndices[pathindex] =
                            (insertindex -= (FT_ORDER + 1));
                    }
                }
            }

            if (topushnode == NULL) {
                // Nothing to push up
                /* Edge case: If we didn't hit the case where
                 * pathindex == finalsharedindex, we need to make sure that that
                 * entry of pathIndices got decremented anyway (so that the left
                 * continuation works as expected).
                 */
                if (rightcontinuation && pathindex > finalsharedindex) {
                    pathIndices[finalsharedindex]--;
                }
                goto endtreeinsertion;
            }
            
            topushent = &topushnode->entries[0];
            left = subtree_get(&topushnode->subtrees[0]);
            right = subtree_get(&topushnode->subtrees[1]);
        }

        // No parent to push to
        this->root = topushnode;
        if (rightcontinuation) {
            struct ft_node* nextpathmember = path[0];
            memmove(&pathIndices[1], pathIndices, (*path_len) * sizeof(int));
            if (nextpathmember == subtree_get(&topushnode->subtrees[1])) {
                pathIndices[0] = 1;
            } else {
                ASSERT(nextpathmember == subtree_get(&topushnode->subtrees[0]),
                       "First element of path is not a child of the new root");
                pathIndices[0] = 0;
            }
            memmove(&path[1], path, (*path_len) * sizeof(struct ft_node*));
            (*path_len)++;
            path[0] = topushnode;
            finalsharedindex++;
        }
    }
    endtreeinsertion:
    if (rightcontinuation) {
        *path_len = finalsharedindex + 1;
    }
}

void ft_write(struct figtree* this, byte_index_t start, byte_index_t end,
              figtree_value_t value) {
    // Plus one because the height of the tree may increase on insert
    // Plus one because the height of a leaf is 0, not 1
    int maxdepth = this->root->HEIGHT + 2;
    struct ft_node* path[maxdepth];
    int pathIndices[maxdepth];
    int path_len = 0; // length of both of the above arrays
    struct insertargs iargs;
    struct insertcont starinserts;
    struct insertcont newstarinserts;

    i_init(&iargs.range, start, end);
    iargs.value = value;
    iargs.at = this->root;
    i_init(&iargs.valid, BYTE_INDEX_MIN, BYTE_INDEX_MAX);
    
    _ft_insert(this, &iargs, path, pathIndices, &path_len, false, &starinserts);
    if (starinserts.hasrightc) {
        _ft_insert(this, &starinserts.rightc, path, pathIndices, &path_len,
                   true, &newstarinserts);
        ASSERT(!newstarinserts.hasleftc && !newstarinserts.hasrightc,
               "Recursive star insert on right continutation");
    }
    if (starinserts.hasleftc) {
        _ft_insert(this, &starinserts.leftc, path, pathIndices, &path_len,
                   false, &newstarinserts);
        ASSERT(!newstarinserts.hasleftc && !newstarinserts.hasrightc,
               "Recursive star insert on left continuation");
    }
}

figtree_value_t* ft_lookup(struct figtree* this, byte_index_t location) {
    struct ft_node* currnode = this->root;

    outerloop:
    while (currnode != NULL) {
        int i;
        for (i = 0; i < currnode->entries_len; i++) {
            struct ft_ent* current = &currnode->entries[i];
            struct interval* currival = &current->irange;
            if (i_contains_val(currival, location)) {
                return &current->value;
            } else if (currival->left > location) {
                currnode = subtree_get(&currnode->subtrees[i]);
                goto outerloop;
            }
        }
        currnode = subtree_get(&currnode->subtrees[currnode->entries_len]);
    }

    return NULL;
}

/* Stores one node in a path of nodes to reach the current point in the
 * iteration.
 */
struct figtree_iterstate {
    struct ft_node* node;
    int pos; // index of the next subtree to look at
    struct interval valid;
};

void ft_iterstate_init(struct figtree_iterstate* this, struct ft_node* node,
                       struct interval* valid) {
    this->node = node;
    this->pos = -1; // index of the entry we just looked at
    memcpy(&this->valid, valid, sizeof(struct interval));
}

/* This is really just a header for an array of figtree_iterstates.
 * The current position is given by the current entry of the iterstate
 * at index DEPTH.
 */
struct figtree_iter {
    int depth; // Index into following array
};

/* Returns an iterator over the closed interval [START, END]. */
struct figtree_iter* ft_read(struct figtree* this,
                             byte_index_t start, byte_index_t end) {
    struct interval initvalid;
    struct ft_ent* entry;
    struct figtree_iter* iterator =
        mem_alloc(sizeof(struct figtree_iter) +
                  (sizeof(struct figtree_iterstate) *
                   (this->root->HEIGHT + 2)));
    struct figtree_iterstate* iterstates =
        (struct figtree_iterstate*) (iterator + 1);
    struct figtree_iterstate* rs = &iterstates[0];
    struct figtree_iterstate* ors; /* temp. pointer to change rs */
    iterator->depth = 0;

    i_init(&initvalid, start, end);
    ft_iterstate_init(rs, this->root, &initvalid);

    continueouterloop:
    while (rs->node != NULL) {
        struct interval* previval;
        struct interval* currival = NULL;
        while (++rs->pos < rs->node->entries_len) {
            entry = &rs->node->entries[rs->pos];
            previval = currival;
            currival = &entry->irange;
            if (i_contains_val(currival, start)) {
                goto breakouterloop;
            } else if (i_rightOf_val(currival, start)) {
                ors = rs;
                rs = &iterstates[++iterator->depth];
                ft_iterstate_init(rs,
                                  subtree_get(&ors->node->subtrees[ors->pos]),
                                  &ors->valid);
                i_restrict_range(&rs->valid, previval == NULL ?
                                 BYTE_INDEX_MIN : (previval->right + 1),
                                 currival->left - 1, false);
                goto continueouterloop;
            }
        }
        ors = rs;
        rs = &iterstates[++iterator->depth];
        ft_iterstate_init(rs, subtree_get(&ors->node->subtrees[ors->pos]),
                          &ors->valid);
        i_restrict_range(&rs->valid, currival->right + 1, BYTE_INDEX_MAX,
                         false);
    }
    breakouterloop:
    while (iterator->depth != -1 &&
           (rs->node == NULL || rs->pos == rs->node->entries_len ||
            i_leftOf_int(&rs->valid, &rs->node->entries[rs->pos].irange))) {
        rs = &iterstates[--iterator->depth];
    }
    
    return iterator;
}

void ft_dealloc(struct figtree* this) {
    ftn_free(this->root);
    this->root = NULL;
}

/* Populates NEXT with the next fig (i.e. the next range of bytes and the
 * value it corresponds to), or returns false if there is no next fig.
 */
bool fti_next(struct figtree_iter* this, struct fig* next) {
    struct figtree_iterstate* states = (struct figtree_iterstate*) (this + 1);
    struct figtree_iterstate* rs;
    struct interval* oldvalid;
    struct ft_ent* entry; // the current entry

    /* At the end of the iteration, we backtrack up the tree past the root since
     * all nodes appear "invalid" given the restricted valid interval.
     */
    if (this->depth == -1) {
        return false;
    }
    rs = &states[this->depth];
    ASSERT (rs->pos < rs->node->entries_len,
            "Iterator starting at end of interior node");
    entry = &rs->node->entries[rs->pos];

    // Populate next with what we're going to yield
    memcpy(&next->irange, &entry->irange, sizeof(struct interval));
    i_restrict_int(&next->irange, &rs->valid, false);
    next->value = entry->value;

    /* Now that we've populated NEXT, we need to do the hard part, which is
     * figuring out whether there's something else that comes after this, while
     * simultaneously figuring out what the next fig is, if it exists.
     *
     * The algorithm for this consists of two phases. First, we drill down the
     * tree until we reach a leaf. In fact we'll go between two entries in a
     * leaf so we'll be at a NULL subtree. Then, we backtrack up the tree until
     * we find the next entry.
     */

    /* First, descend the subtree after the rv until we reach a leaf. */
    if (rs->valid.right <= entry->irange.right) {
        /* If we've moved past the right of the valid interval, skip the
         * remaining entries.
         */
        rs->pos = rs->node->entries_len;
    } else {
        byte_index_t leftlimit, rightlimit;
        struct ft_node* subtree;

        leftlimit = entry->irange.right + 1;
        if (++rs->pos == rs->node->entries_len) {
            rightlimit = BYTE_INDEX_MAX;
        } else {
            rightlimit = rs->node->entries[rs->pos].irange.left - 1;
        }

        /* If the next entry in this node is adjacent to the one we just
         * yielded, then there's no point in traversing this subtree. That's
         * why we have this if statement.
         */
        if (leftlimit <= rightlimit) {
            subtree = subtree_get(&rs->node->subtrees[rs->pos]);

            /* This is the loop where we drill down the subtree. */
            while (subtree != NULL) {
                oldvalid = &rs->valid;
                rs = &states[++this->depth];
                ft_iterstate_init(rs, subtree, oldvalid);
                i_restrict_range(&rs->valid, leftlimit, rightlimit, false);
                /* The above operation will never result in an empty valid
                 * interval because the entry that we just yielded was valid,
                 * and we already verified that the valid interval extends
                 * past the right of that entry into this gap whose subtree
                 * we are now traversing.
                 */

                /* Skip entries to the left of the valid interval. */
                while (++rs->pos != rs->node->entries_len &&
                       i_leftOf_int(&rs->node->entries[rs->pos].irange,
                                    &rs->valid)) {
                    /* Do nothing; the loop condition does all the work. */
                }

                if (rs->pos == 0) {
                    leftlimit = BYTE_INDEX_MIN;
                } else {
                    leftlimit = rs->node->entries[rs->pos - 1].irange.right + 1;
                }

                if (rs->pos == rs->node->entries_len) {
                    rightlimit = BYTE_INDEX_MAX;
                } else {
                    /* If the entry overlaps partially with the interval,
                     * then we can skip the left subtree.
                     */
                    if (i_leftOverlaps(&rs->valid,
                                       &rs->node->entries[rs->pos].irange)) {
                        /* rs->node->entries[rs->pos] is the next entry to
                         * yield.
                         */
                        break;
                    }
                    rightlimit = rs->node->entries[rs->pos].irange.left - 1;
                }
                subtree = subtree_get(&rs->node->subtrees[rs->pos]);
            }
        }
    }

    /* It's a rule that in the second phase, that the entry at which to resume
     * in a parent after iterating over its subtree is
     * rs->node->entries[rs->pos], UNLESS
     * (1) rs->pos == rs->node->entries_len, in which case we need to backtrack
     * further, or
     * (2) rs->node->entries[rs->pos] moves past the rs->valid interval, in
     * which case the rest of the node is "dead" and needs to be ignored. Again,
     * we just keep backtracking in this case, as if we reached the end of the
     * node.
     */
    while (rs->pos == rs->node->entries_len ||
           i_leftOf_int(&rs->valid, &rs->node->entries[rs->pos].irange)) {
        /* If we backtrack up beyond the root, then we've walked past what's in
         * the tree, and there's nothing more to yield.
         */
        if (--this->depth == -1) {
            return true;
        }
        rs = &states[this->depth];
    }

    /* At this point, rs->node->entries[rs->pos] is the entry to yield next. */
    return true;
}

void fti_free(struct figtree_iter* this) {
    mem_free(this);
}
