#include "log_manager.h"

#include "uthash/include/uthash.h"

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <sysexits.h>

#define BUF_BLOCKS 20

static bool ro_mode;
static gdp_gcl_t *gcl;

typedef struct {
    bool is_root_inode;                // true if this is the root inode
    size_t size;                       // records actual size of block
    char block[BLOCK_SIZE];            // stores user data
} log_ent_t;

typedef struct {
    int ref_count;
    gdp_recno_t recno;
    UT_hash_handle hh;
    gdpfs_log_ent_t data;
} log_ent_mem_t;
static log_ent_mem_t *log_ents = NULL;

void
lm_init(bool ro, char *gclpname)
{
    EP_STAT estat;
    gdp_name_t gclname;
    gdp_iomode_t mode;

    ro_mode = ro;

    // initialize the GDP library
    estat = gdp_init(NULL);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("GDP Initialization failed");
        exit(EX_UNAVAILABLE);
    }

    // convert the name to internal form
    estat = gdp_parse_name(gclpname, gclname);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Illegal GCL name syntax:\n\t%s",
            gclpname);
        exit(EX_NOINPUT);
    }

    // open the GCL
    mode = ro_mode ? GDP_MODE_RO : GDP_MODE_RA;
    estat = gdp_gcl_open(gclname, mode, NULL, &gcl);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];

        ep_app_error("Cannot open GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
        exit(EX_NOINPUT);
    }

    // asdfdsa
}

static void
release_log_ent_mem(log_ent_mem_t *lem)
{
    if (--lem->ref_count == 0)
    {
        free(lem);
    }

}

static log_ent_mem_t *
read_log_ent(gdp_recno_t block)
{
    EP_STAT estat;
    gdp_buf_t *datum_buf;
    gdp_datum_t *datum;
    log_ent_mem_t *lem;

    ////////////////////////////////////////////////////////////
    // TODO: rather than directly reading, do a cache lookup. //
    ////////////////////////////////////////////////////////////

    datum = gdp_datum_new();
    estat = gdp_gcl_read(gcl, block, datum);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];
        ep_app_error("Cannot read GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
        goto fail0;
    }

    lem = calloc(sizeof(log_ent_mem_t), 1);
    lem->ref_count = 1;

fail0:
    gdp_datum_free(datum);
    return 0;
}

int
lm_free_resources()
{
    EP_STAT estat;
    int status = 0;
    estat = gdp_gcl_close(gcl);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];

        ep_app_error("Cannot close GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
        status = -EP_STAT_DETAIL(estat);
    }
    return status;
}

size_t
lm_read(gdp_recno_t rec, void *buf, off_t offset, size_t size)
{
    size_t size_read;

    log_ent_mem_t *lem = read_log_ent(block);

    release_log_ent_mem(lem);
    return size_read;
}

size_t
lm_write(gdp_recno_t rec, void *buf, off_t offset, size_t size)
{
    log_ent_mem_t *lem = read_log_ent(block);
    return 0;
}
