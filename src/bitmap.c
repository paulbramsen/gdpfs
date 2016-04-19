#include "bitmap.h"


static inline size_t byte_size(size_t bit_size)
{
    return 1 + ((bit_size - 1) / 8);
}

bitmap_t *bitmap_create(size_t size)
{
    bitmap_t *bmp;

    bmp = ep_mem_zalloc(sizeof(bitmap_t) + byte_size(size));
    if (bmp)
        bmp->size = size;
    return bmp;
}

uint64_t bitmap_reserve(bitmap_t *bmp)
{
    uint8_t *data;
    off_t byte;
    off_t bit;
    off_t bit_lim;

    if (!bmp)
        return -1;
    data = bmp->data;
    byte = 0;
    for (byte = 0; data[byte] == 0xFF && byte < byte_size(bmp->size); byte++);
    // If last byte, may need to limit bits examined in case size isn't mulitple
    // of 8.
    if (byte == byte_size(bmp->size) - 1)
        bit_lim = ((bmp->size - 1) % 8) + 1;
    else if (byte == byte_size(bmp->size))
        bit_lim = 0;
    else
        bit_lim = 8;
    for (bit = 0; bit < bit_lim; bit++)
    {
        if (((data[byte] >> bit) & 1) == 0)
        {
            data[byte] |= (1 << bit);
            return byte * 8 + bit;
        }
    }
    return -1;
}

int bitmap_release(bitmap_t *bmp, uint64_t val)
{
    uint8_t field;
    off_t byte;
    off_t bit;

    if (!bmp)
        return -1;
    if (val > bmp->size)
        return -1;
    byte = val / 8;
    bit = val % 8;
    field = bmp->data[byte];
    if (((field >> bit) & 1) != 1)
        return -1;
    bmp->data[byte] &= ~(1 << bit);
    return 0;
}

int bitmap_set(bitmap_t *bmp, uint64_t val) {
    uint8_t field;
    off_t byte;
    off_t bit;
    if (!bmp)
        return -1;
    if (val > bmp->size)
        return -1;
    byte = val / 8;
    bit = val % 8;
    field = bmp->data[byte];
    if (((field >> bit) & 1) != 0)
        return -1;
    bmp->data[byte] |= (1 << bit);
}

int bitmap_is_set(bitmap_t *bmp, uint64_t val)
{
    off_t byte;
    off_t bit;

    if (!bmp)
        return -1;
    if (val > bmp->size)
        return -1;
    byte = val / 8;
    bit = val % 8;
    return (bmp->data[byte] >> bit) & 1;    
}

void bitmap_free(bitmap_t *bmp)
{
    ep_mem_free(bmp);
}
