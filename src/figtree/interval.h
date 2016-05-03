#ifndef _INTERVAL_H_
#define _INTERVAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

struct interval {
    byte_index_t left;
    byte_index_t right;
    bool nonempty;
};

void i_init(struct interval* newint, byte_index_t left, byte_index_t right);
struct interval* i_new(byte_index_t left, byte_index_t right);
struct interval* i_copy(struct interval* this);
bool i_contains_val(struct interval* this, byte_index_t x);
bool i_contains_int(struct interval* this, struct interval* other);
void i_restrict_range(struct interval* this, byte_index_t left,
                      byte_index_t right, bool allowempty);
void i_restrict_int(struct interval* this, struct interval* to,
                    bool allowempty);
bool i_overlaps(struct interval* this, struct interval* other);
bool i_leftOverlaps(struct interval* this, struct interval* other);
bool i_rightOverlaps(struct interval* this, struct interval* other);
bool i_leftOf_val(struct interval* this, byte_index_t x);
bool i_leftOf_int(struct interval* this, struct interval* other);
bool i_rightOf_val(struct interval* this, byte_index_t x);
bool i_rightOf_int(struct interval* this, struct interval* other);
bool i_equals(struct interval* this, struct interval* other);

#endif
