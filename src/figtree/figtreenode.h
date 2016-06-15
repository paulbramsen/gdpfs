/*
**  ----- BEGIN LICENSE BLOCK -----
**  GDPFS: Global Data Plane File System
**  From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**  Copyright (c) 2016, Regents of the University of California.
**  Copyright (c) 2016, Paul Bramsen, Sam Kumar, and Andrew Chen
**  All rights reserved.
**
**  Permission is hereby granted, without written agreement and without
**  license or royalty fees, to use, copy, modify, and distribute this
**  software and its documentation for any purpose, provided that the above
**  copyright notice and the following two paragraphs appear in all copies
**  of this software.
**
**  IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**  SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**  PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**  EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**  REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**  FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**  IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**  OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**  OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interval.h"
#include "utils.h"

/* Fig Tree Entry */

struct ft_ent {
    struct interval irange;
    figtree_value_t value;
};

bool fte_overlaps(struct ft_ent* this, struct ft_ent* other);


/* Fig Tree Node */

typedef struct ft_node {
    int entries_len;
    struct ft_ent entries[FT_SPLITLIMIT];
    int subtrees_len;
    struct subtree_ptr subtrees[FT_SPLITLIMIT + 1];
    int HEIGHT;
    bool dirty;
} figtree_node_t;

struct ft_node* ftn_new(int height, bool make_height);
void ftn_clear(struct ft_node* this, bool make_height);
struct ft_node* ftn_insert(struct ft_node* this, struct ft_ent* newent,
                           int index, struct ft_node* leftChild,
                           struct ft_node* rightChild);
void ftn_replaceEntries(struct ft_node* this, int start, int end,
                        struct interval* newent_interval,
                        figtree_value_t newent_value);
void ftn_pruneTo(struct ft_node* this, struct interval* valid);
void ftn_free(struct ft_node* this);
