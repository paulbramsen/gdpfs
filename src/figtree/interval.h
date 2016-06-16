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
