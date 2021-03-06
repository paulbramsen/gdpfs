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

#include "bitmap.h"
#include "bitmap_file.h"
#include <errno.h>
#include <ep/ep_app.h>
#include <string.h>

/**
 * Set bits [left, right) to 1
 */
/*void bitmap_file_set_range(int fd, off_t left, off_t right)
{
    off_t extended_left;
    off_t extended_right;
    ssize_t extended_size;
    off_t byte;
    ssize_t size;
    bitmap_t *bitmap;

    extended_left = left - left % 8;
    extended_right = right + 8 - right % 8;
    extended_size = extended_right - extended_left;
    bitmap = bitmap_create(extended_size);
    // TODO: error check
    for (byte = left - extended_left; byte < right - extended_left; byte++)
    {
        bitmap_set(bitmap, byte);
    }
    lseek(fd, SEEK_SET, extended_left / 8);
    size = write(fd, bitmap->data, (extended_size) / 8);
    (void)size;
    // TODO: error check
    bitmap_free(bitmap);
}
*/

#define BMP_CHUNKSIZE 4096

/* Set bits in the bitmap from [left, right). */
bool bitmap_file_set_range(int fd, off_t left, off_t right)
{
    if (right <= left) {
        ep_app_error("Bad arguments to bitmap_file_set_range: [%lu, %lu)", left, right);
        return false;
    }

    /* It's slightly more efficient this way; we treat the range as inclusive at this point,
       and have to consider one fewer byte if it's actually byte-aligned. */
    right -= 1;
    /* Check the first byte. */
    off_t leftbyte = left >> 3;
    off_t rightbyte = right >> 3;
    off_t leftbits = left & 0x7;
    off_t rightbits = right & 0x7;

    off_t lrv = lseek(fd, leftbyte, SEEK_SET);
    if (lrv == -1)
    {
        ep_app_error("lseek failed: %d", errno);
        return false;
    }

    uint8_t chunk[BMP_CHUNKSIZE];
    uint8_t mask;
    ssize_t rv;

    uint8_t leftmask = ((uint8_t) 0xFF) >> leftbits;
    uint8_t rightmask = ((uint8_t) 0xFF) << (7 - rightbits);

    off_t bytesleft;

    /* Read the first byte. */
    rv = read(fd, chunk, 1);
    if (rv == 0)
        chunk[0] = 0x00;
    else
        lseek(fd, -1, SEEK_CUR);

    if (leftbyte == rightbyte)
    {
        mask = leftmask & rightmask;
        chunk[0] = chunk[0] | mask;
        rv = write(fd, chunk, 1);
        return rv == 1;
    }
    else
    {
        chunk[0] = chunk[0] | leftmask;
        rv = write(fd, chunk, 1);
        if (rv != 1)
            return false;
        bytesleft = rightbyte - leftbyte;
        memset(chunk, 0xFF, bytesleft < BMP_CHUNKSIZE ? bytesleft : BMP_CHUNKSIZE);

        while (bytesleft > BMP_CHUNKSIZE) // all but the last chunk
        {
            rv = write(fd, chunk, BMP_CHUNKSIZE);
            if (rv != BMP_CHUNKSIZE)
                return false;
            bytesleft -= BMP_CHUNKSIZE;
        }
        // the final chunk
        rv = write(fd, chunk, bytesleft - 1);
        if (rv != bytesleft - 1)
            return false;
        rv = read(fd, chunk, 1);
        if (rv == 0)
            chunk[0] = 0x00;
        else
            lseek(fd, -1, SEEK_CUR);
        chunk[0] = chunk[0] | rightmask;
        rv = write(fd, chunk, 1);
        return rv == 1;
    }
}

/* Check if the bitmap file has NUMBITS bits set in [left, right). */
bool bitmap_file_isset(int fd, off_t left, off_t right)
{
    if (right <= left) {
        ep_app_error("Bad arguments to bitmap_file_isset: [%lu, %lu)", left, right);
        return false;
    }

    /* It's slightly more efficient this way; we treat the range as inclusive at this point,
       and have to consider one fewer byte if it's actually byte-aligned. */
    right -= 1;
    /* Check the first byte. */
    off_t leftbyte = left >> 3;
    off_t rightbyte = right >> 3;
    off_t leftbits = left & 0x7;
    off_t rightbits = right & 0x7;

    off_t lrv = lseek(fd, leftbyte, SEEK_SET);
    if (lrv == -1)
    {
        ep_app_error("lseek failed: %d", errno);
        return false;
    }

    uint8_t chunk[BMP_CHUNKSIZE];
    uint8_t mask;
    ssize_t rv;

    uint8_t leftmask = ((uint8_t) 0xFF) >> leftbits;
    uint8_t rightmask = ((uint8_t) 0xFF) << (7 - rightbits);

    off_t bytesleft;
    int i;

    /* Read the first byte. */
    rv = read(fd, chunk, 1);
    if (rv != 1)
        return false;

    if (leftbyte == rightbyte)
    {
        mask = leftmask & rightmask;
        return (chunk[0] & mask) == mask;
    }
    else
    {
        if ((chunk[0] & leftmask) != leftmask)
            return false;
        bytesleft = rightbyte - leftbyte;
        while (bytesleft > BMP_CHUNKSIZE) // all but the last chunk
        {
            rv = read(fd, chunk, BMP_CHUNKSIZE);
            if (rv != BMP_CHUNKSIZE)
                return false;
            for (i = 0; i < BMP_CHUNKSIZE; i++)
            {
                if (chunk[i] != 0xFF)
                    return false;
            }
            bytesleft -= BMP_CHUNKSIZE;
        }
        // the final chunk
        rv = read(fd, chunk, bytesleft);
        if (rv != bytesleft)
            return false;
        for (i = 0; i < bytesleft - 1; i++)
        {
            if (chunk[i] != 0xFF)
                return false;
        }
        return (chunk[bytesleft - 1] & rightmask) == rightmask;
    }
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

    // TODO: this should error check
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
