#ifndef _GDPFS_FILE_H_
#define _GDPFS_FILE_H_

#include <ep/ep.h>
#include <stdbool.h>

typedef struct gdpfs_file gdpfs_file_t;

EP_STAT init_gdpfs_file();
void stop_gdpfs_file();
EP_STAT gdpfs_file_open(gdpfs_file_t **file, char *name, bool ro_mode);
EP_STAT gdpfs_file_close(gdpfs_file_t *file);
size_t gdpfs_file_read(gdpfs_file_t *file, char *buf, size_t size,
        off_t offset);
size_t gdpfs_file_write(gdpfs_file_t *file, const char *buf, size_t size,
        off_t offset);
int gdpfs_file_ftruncate(gdpfs_file_t *file, size_t file_size);
size_t gdpfs_file_size(gdpfs_file_t *file);

#endif // _GDPFS_FILE_H_
