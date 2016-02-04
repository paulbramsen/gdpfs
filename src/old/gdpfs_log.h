#ifndef _GDPFS_LOG_H_
#define _GDPFS_LOG_H_

#include <ep/ep.h>

typedef struct gdpfs_log gdpfs_log_t;
typedef struct gdpfs_log_ent gdpfs_log_ent_t;
typedef int64_t gdpfs_recno_t;

// global init
EP_STAT init_gdpfs_log();

// log management
EP_STAT gdpfs_log_open(gdpfs_log_t **handle, char *log_name, bool ro_mode);
EP_STAT gdpfs_log_close(gdpfs_log_t *handle);
EP_STAT gdpfs_log_append(gdpfs_log_t *handle, gdpfs_log_ent_t *ent);

// log ent management
gdpfs_log_ent_t *gdpfs_log_ent_new();
EP_STAT gdpfs_log_ent_open(gdpfs_log_t *handle, gdpfs_log_ent_t **ent,
    gdpfs_recno_t recno);
void gdpfs_log_ent_close(gdpfs_log_ent_t *ent);
size_t gdpfs_log_ent_length(gdpfs_log_ent_t *ent);
size_t gdpfs_log_ent_read(gdpfs_log_ent_t *ent, void *buf, size_t size);
int gdpfs_log_ent_write(gdpfs_log_ent_t *ent, const void *buf, size_t size);

#endif // _GDPFS_LOG_H_
