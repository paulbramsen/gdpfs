#include "bitmap_file.h"
#include "bitmap.h"

/**
 * Set bits [left, right) to 1
 */
void bitmap_file_set_range(int fd, off_t left, off_t right) 
{
    off_t extended_left;
    off_t extended_right;
    ssize_t extended_size;
    off_t byte;
    bitmap_t *bitmap;

    extended_left = left - left % 8;
    extended_right = right + 8 - right % 8;
    extended_size = extended_right - extended_left;
    bitmap = bitmap_create(extended_size);
    for (byte = left - extended_left; byte < right - extended_left; byte++) 
    {
        bitmap_set(bitmap, byte);
    }
    lseek(fd, SEEK_SET, extended_left / 8);
    write(fd, bitmap->data, (extended_size) / 8);
    bitmap_free(bitmap);
}

/**
 * Gets a bitmap from [left, right). Must free returned bitmap.
 */
bitmap_t *bitmap_file_get_range(int fd, off_t left, off_t right) 
{
    off_t extended_left;
    off_t extended_right;
    ssize_t extended_size;
    ssize_t size;
    off_t byte;

    extended_left = left - left % 8;
    extended_right = right + 8 - right % 8;
    extended_size = extended_right - extended_left;
    uint8_t file_bitmap[extended_size / 8];
    lseek(fd, SEEK_SET, extended_left/8);
    size = read(fd, file_bitmap, extended_size / 8);
    bitmap_t *extended_bitmap = bitmap_create(extended_size);
    memcpy(extended_bitmap->data, file_bitmap, size);
    bitmap_t *bitmap = bitmap_create(right - left);
    for (byte = left; byte < right; byte++)
    {
        if (bitmap_is_set(extended_bitmap, byte - extended_left))
            bitmap_set(bitmap, byte - left);
    }
    bitmap_free(extended_bitmap);
    return bitmap;
}
