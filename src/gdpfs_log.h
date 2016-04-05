#ifndef _GDPFS_LOG_H_
#define _GDPFS_LOG_H_

#include <ep/ep.h>
#include <gdp/gdp.h>

typedef struct gdpfs_log gdpfs_log_t;
typedef struct gdpfs_log_ent gdpfs_log_ent_t;
typedef int64_t gdpfs_recno_t;
enum gdpfs_log_mode
{
    GDPFS_LOG_MODE_RO = 0,
    GDPFS_LOG_MODE_RA,
    GDPFS_LOG_MODE_AO,
};
typedef enum gdpfs_log_mode gdpfs_log_mode_t;
typedef gdp_name_t gdpfs_log_gname_t;

// global init
EP_STAT init_gdpfs_log();

// log management
EP_STAT gdpfs_log_create(gdp_name_t log_iname);
EP_STAT gdpfs_log_open(gdpfs_log_t **handle, gdp_name_t gcl_name,
        gdpfs_log_mode_t mode);
EP_STAT gdpfs_log_close(gdpfs_log_t *handle);
EP_STAT gdpfs_log_append(gdpfs_log_t *handle, gdpfs_log_ent_t *ent);
void gdpfs_log_gname(gdpfs_log_t *handle, gdpfs_log_gname_t gname);

// log ent management
gdpfs_log_ent_t *gdpfs_log_ent_new();
EP_STAT gdpfs_log_ent_open(gdpfs_log_t *handle, gdpfs_log_ent_t **ent,
    gdpfs_recno_t recno);
void gdpfs_log_ent_close(gdpfs_log_ent_t *ent);
size_t gdpfs_log_ent_length(gdpfs_log_ent_t *ent);
size_t gdpfs_log_ent_read(gdpfs_log_ent_t *ent, void *buf, size_t size);
int gdpfs_log_ent_write(gdpfs_log_ent_t *ent, const void *buf, size_t size);

#endif // _GDPFS_LOG_H_
