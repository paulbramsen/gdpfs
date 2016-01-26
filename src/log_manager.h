#ifndef _LOG_MANAGER_H_
#define _LOG_MANAGER_H_

#include <gdp/gdp.h>

#define BLOCK_SIZE 4096

// setup log manager and return recno of root inode
gdp_recno_t lm_init(bool ro, char *gclpname);
int lm_free_resources();
size_t lm_read(gdp_recno_t rec, void *buf, off_t offset, size_t size);
size_t lm_write(gdp_recno_t rec, void *buf, off_t offset, size_t size);

#endif // _LOG_MANAGER_H_