#ifndef _GDPFS_LOG_H_
#define _GDPFS_LOG_H_

#include <ep/ep.h>
#include <gdp/gdp.h>

typedef gdp_name_t gdpfs_log_gname_t;
typedef gdp_event_cbfunc_t gdpfs_callback_t;

typedef int64_t gdpfs_recno_t;
struct gdpfs_log_ent
{
    int cached_fd;
    gdpfs_recno_t cached_recno;
    bool is_cached;
    gdp_datum_t *datum;
};
typedef struct gdpfs_log_ent gdpfs_log_ent_t;
struct gdpfs_log
{
    gdp_gcl_t *gcl_handle;
    gdpfs_log_gname_t gname;
};
typedef struct gdpfs_log gdpfs_log_t;
enum gdpfs_log_mode
{
    GDPFS_LOG_MODE_RO = 0,
    GDPFS_LOG_MODE_RA,
    GDPFS_LOG_MODE_AO,
};
typedef enum gdpfs_log_mode gdpfs_log_mode_t;

/*
 * global init of log subsystem
 */
EP_STAT
init_gdpfs_log(gdpfs_log_mode_t log_mode, char *gdp_router_addr);

/*
 * log management
 */
EP_STAT
gdpfs_log_get_precreated(gdp_name_t log_iname);

EP_STAT
gdpfs_log_open(gdpfs_log_t **handle, gdp_name_t gcl_name);

EP_STAT
gdpfs_log_close(gdpfs_log_t *handle);

EP_STAT
gdpfs_log_append(gdpfs_log_t *handle, gdpfs_log_ent_t *ent, gdpfs_callback_t cb, void *data);

void
gdpfs_log_gname(gdpfs_log_t *handle, gdpfs_log_gname_t gname);

/*
 * log ent management
 */
EP_STAT
gdpfs_log_ent_init(gdpfs_log_ent_t* log_ent);

EP_STAT
gdpfs_log_ent_open(gdpfs_log_t *handle, gdpfs_log_ent_t *ent,
		gdpfs_recno_t recno, bool bypass_cache);

void
gdpfs_log_ent_close(gdpfs_log_ent_t *ent);

size_t
gdpfs_log_ent_length(gdpfs_log_ent_t *ent);

size_t
gdpfs_log_ent_read(gdpfs_log_ent_t *ent, void *buf, size_t size);

// same as read, but doesn't advance pointer
size_t
gdpfs_log_ent_peek(gdpfs_log_ent_t *ent, void *buf, size_t size);

gdpfs_recno_t
gdpfs_log_ent_recno(gdpfs_log_ent_t *ent);

int
gdpfs_log_ent_write(gdpfs_log_ent_t *ent, const void *buf, size_t size);

int
gdpfs_log_ent_drain(gdpfs_log_ent_t *ent, size_t size);

#endif // _GDPFS_LOG_H_
