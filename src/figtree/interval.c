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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interval.h"
#include "utils.h"

/* Interval */

void i_init(struct interval* newint, byte_index_t left, byte_index_t right) {
    newint->left = left;
    newint->right = right;
    newint->nonempty = true;
}

struct interval* i_new(byte_index_t left, byte_index_t right) {
    struct interval* newint;
    ASSERT(left <= right, "Invalid interval");
    newint = mem_alloc(sizeof(struct interval));
    i_init(newint, left, right);
    return newint;
}

struct interval* i_copy(struct interval* this) {
    struct interval* newint = mem_alloc(sizeof(struct interval));
    memcpy(newint, this, sizeof(struct interval));
    return newint;
}

bool i_contains_val(struct interval* this, byte_index_t x) {
    return x >= this->left && x <= this->right;
}

bool i_contains_int(struct interval* this, struct interval* other) {
    return this->left <= other->left && this->right >= other->right;
}

void i_restrict_range(struct interval* this, byte_index_t left,
                      byte_index_t right, bool allowempty) {
    byte_index_t newleft = MAX(this->left, left);
    byte_index_t newright = MIN(this->right, right);
    if (allowempty && newleft > newright) {
        this->left = BYTE_INDEX_MAX;
        this->right = BYTE_INDEX_MIN;
        this->nonempty = false;
    } else {
        ASSERT(newleft <= newright, "Restricting results in empty interval");
        this->left = newleft;
        this->right = newright;
    }
}

void i_restrict_int(struct interval* this, struct interval* to,
                    bool allowempty) {
    i_restrict_range(this, to->left, to->right, allowempty);
}

bool i_overlaps(struct interval* this, struct interval* other) {
    return this->right >= other->left && this->left <= other->right;
}

bool i_leftOverlaps(struct interval* this, struct interval* other) {
    return this->nonempty && i_contains_val(other, this->left);
}

bool i_rightOverlaps(struct interval* this, struct interval* other) {
    return this->nonempty && i_contains_val(other, this->right);
}

bool i_leftOf_val(struct interval* this, byte_index_t x) {
    return this->nonempty && this->right < x;
}

bool i_leftOf_int(struct interval* this, struct interval* other) {
    return i_leftOf_val(this, other->left);
}

bool i_rightOf_val(struct interval* this, byte_index_t x) {
    return this->nonempty && this->left > x;
}

bool i_rightOf_int(struct interval* this, struct interval* other) {
    return i_rightOf_val(this, other->right);
}

bool i_equals(struct interval* this, struct interval* other) {
    if (this->nonempty == other->nonempty) {
        return !this->nonempty ||
            (this->left == other->left && this->right == other->right);
    } else {
        return false;
    }
}
