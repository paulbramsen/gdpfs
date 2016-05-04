#include "bitmap.h"

static inline size_t byte_size(size_t bit_size)
{
    return 1 + ((bit_size - 1) / 8);
}

bitmap_t *bitmap_create(size_t size)
{
    bitmap_t *bmp = NULL;
    bmp = ep_mem_zalloc(sizeof(bitmap_t) + byte_size(size));
    if (bmp)
    {
        bmp->size = size;
        int mutex_err = ep_thr_mutex_init(&bmp->mutex, EP_THR_MUTEX_DEFAULT);
        if (mutex_err != 0)
        {
            ep_mem_free(bmp);
            bmp = NULL;
        }
    }
    return bmp;
}

uint64_t bitmap_reserve(bitmap_t *bmp)
{
    uint8_t *data;
    off_t byte;
    off_t bit;
    off_t bit_lim;
    uint64_t return_value = -1;

    if (!bmp)
        return return_value;
    data = bmp->data;
    byte = 0;
    ep_thr_mutex_lock(&bmp->mutex);
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
            return_value = byte * 8 + bit;
            goto release_and_done;
        }
    }

release_and_done:
    ep_thr_mutex_unlock(&bmp->mutex);
    return return_value;
}

int bitmap_release(bitmap_t *bmp, uint64_t val)
{
    uint8_t field;
    off_t byte;
    off_t bit;
    int return_value = -1;

    if (!bmp)
        return -1;
    if (val > bmp->size)
        return -1;
    byte = val / 8;
    bit = val % 8;

    ep_thr_mutex_lock(&bmp->mutex);
    field = bmp->data[byte];
    if (((field >> bit) & 1) != 1)
        goto release_and_done;
    bmp->data[byte] &= ~(1 << bit);
    return_value = 0;
    goto release_and_done;

release_and_done:
    ep_thr_mutex_unlock(&bmp->mutex);
    return return_value;
}

int bitmap_set(bitmap_t *bmp, uint64_t val) {
    uint8_t field;
    off_t byte;
    off_t bit;
    int return_value = -1;

    if (!bmp)
        return -1;
    if (val > bmp->size)
        return -1;
    byte = val / 8;
    bit = val % 8;

    ep_thr_mutex_lock(&bmp->mutex);
    field = bmp->data[byte];
    if (((field >> bit) & 1) != 0)
        goto release_and_done;
    bmp->data[byte] |= (1 << bit);
    return_value = 0;

release_and_done:
    ep_thr_mutex_unlock(&bmp->mutex);
    return return_value;
}

int bitmap_is_set(bitmap_t *bmp, uint64_t val)
{
    off_t byte;
    off_t bit;
    int return_value;

    if (!bmp)
        return -1;
    if (val > bmp->size)
        return -1;

    byte = val / 8;
    bit = val % 8;
    return_value = (bmp->data[byte] >> bit) & 1;
    return return_value;
}

void bitmap_free(bitmap_t *bmp)
{
    ep_thr_mutex_destroy(&bmp->mutex);
    ep_mem_free(bmp);
}
