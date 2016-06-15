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
