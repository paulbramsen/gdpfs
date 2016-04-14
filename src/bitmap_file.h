#ifndef _BITMAP_FILE_H_
#define _BITMAP_FILE_H_
#include "bitmap.h"

void bitmap_file_set_range(int fd, off_t left, off_t right);
bitmap_t *bitmap_file_get_range(int fd, off_t left, off_t right);

#endif //_BITMAP_FILE_H_
