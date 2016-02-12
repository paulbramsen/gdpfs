#ifndef _GDPFS_FILE_H_
#define _GDPFS_FILE_H_

#include <ep/ep.h>
#include <stdbool.h>

EP_STAT init_gdpfs_file();
void stop_gdpfs_file();
uint64_t gdpfs_file_open(EP_STAT *ret_stat, char *name, bool ro_mode);
EP_STAT gdpfs_file_close(uint64_t fd);
size_t gdpfs_file_read(uint64_t fd, char *buf, size_t size, off_t offset);
size_t gdpfs_file_write(uint64_t fd, const char *buf, size_t size, off_t offset);
int gdpfs_file_ftruncate(uint64_t fd, size_t file_size);
size_t gdpfs_file_size(uint64_t fd);

#endif // _GDPFS_FILE_H_
