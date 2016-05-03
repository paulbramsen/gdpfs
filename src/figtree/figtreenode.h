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

struct ft_node {
    int entries_len;
    struct ft_ent entries[FT_SPLITLIMIT];
    int subtrees_len;
    struct subtree_ptr subtrees[FT_SPLITLIMIT + 1];
    int HEIGHT;
};

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
