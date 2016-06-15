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

#ifndef _FIGTREE_H_
#define _FIGTREE_H_

#include "interval.h"
#include "utils.h"

#include "../gdpfs_log.h"

void ftn_free(struct ft_node* this);

typedef struct figtree {
    struct ft_node* root;
} figtree_t;

/* Initializes a Fig Tree in the specified space. */
void ft_init(struct figtree* this);

/* Initializes a Fig Tree in the specified space with the specified data to use for the root node. */
void ft_init_with_root(struct figtree* this, struct ft_node* root);

/* Sets the bytes in the range [START, END] to correspond to VALUE. */
void ft_write(struct figtree* this, byte_index_t start, byte_index_t end,
              figtree_value_t value, gdpfs_log_t* log);

/* Returns a pointer to the value at the specified byte LOCATION. */
figtree_value_t* ft_lookup(struct figtree* this, byte_index_t location, gdpfs_log_t* log);


/* Returns an iterator to read over the specified range of bytes. */
struct figtree_iter* ft_read(struct figtree* this,
                             byte_index_t start, byte_index_t end, gdpfs_log_t* log);

/* Deallocates the resources for the Fig Tree in the specified space. */
void ft_dealloc(struct figtree* this);

/* File-Indexed Group (FIG)
 * Identical in structure to a Fig Tree Entry, but this is used to return
 * ranges, not to store them. Remember that the irange field represents
 * a closed interval!
 */
typedef struct fig {
    struct interval irange;
    figtree_value_t value;
} fig_t;

/* Fig Tree Iterator
 * An iterator over a range of bytes in a Fig Tree. On each call to next(),
 * it returns a FIG describing a range of bytes and the corresponding value
 * for that range of bytes. The ranges are guaranteed to not overlap; however,
 * if the tree was not populated with a value for some bytes in the range, then
 * none of the yielded ranges will contain those bytes.
 */
typedef struct figtree_iter figiter_t;

/* Gets the next FIG from the Fig Tree Iterator and populates NEXT with that
 * result. Returns true if there are additional FIGs in the iterator; returns
 * false if there are no more. */
bool fti_next(struct figtree_iter* this, struct fig* next, gdpfs_log_t* log);

/* Deallocates the resources for the specified Fig Tree Iterator */
void fti_free(struct figtree_iter* this);

#endif
