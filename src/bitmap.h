#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <ep/ep.h>

typedef struct {
    size_t size;
    uint8_t data[0];
} bitmap_t;

// returns NULL if alloc fails
bitmap_t *bitmap_create(size_t size);
// returns -1 if bitmap is full
uint64_t bitmap_reserve(bitmap_t *bmp);
// 0 on success, negative if error
int bitmap_release(bitmap_t *bmp, uint64_t val);
int bitmap_set(bitmap_t *bmp, uint64_t val);
int bitmap_is_set(bitmap_t *bmp, uint64_t val);
void bitmap_free(bitmap_t *bmp);

#endif // _BITMAP_H_
