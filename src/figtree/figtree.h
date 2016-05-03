#ifndef _FIGTREE_H_
#define _FIGTREE_H_

#include "interval.h"
#include "utils.h"

void ftn_free(struct ft_node* this);

typedef struct figtree {
    struct ft_node* root;
} figtree_t;

/* Initializes a Fig Tree in the specified space. */
void ft_init(struct figtree* this);

/* Sets the bytes in the range [START, END] to correspond to VALUE. */
void ft_write(struct figtree* this, byte_index_t start, byte_index_t end,
              figtree_value_t value);

/* Returns a pointer to the value at the specified byte LOCATION. */
figtree_value_t* ft_lookup(struct figtree* this, byte_index_t location);


/* Returns an iterator to read over the specified range of bytes. */
struct figtree_iter* ft_read(struct figtree* this,
                             byte_index_t start, byte_index_t end);

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
bool fti_next(struct figtree_iter* this, struct fig* next);

/* Deallocates the resources for the specified Fig Tree Iterator */
void fti_free(struct figtree_iter* this);

#endif
