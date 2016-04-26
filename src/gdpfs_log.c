#include "gdpfs_log.h"

#include "gdpfs_stat.h"
#include <ep/ep_app.h>
#include <string.h>

extern char* logd_xname;
static gdp_iomode_t gcl_mode;

struct gdpfs_log
{
    gdp_gcl_t *gcl_handle;
    gdpfs_log_gname_t gname;
};

EP_STAT init_gdpfs_log(gdpfs_log_mode_t log_mode)
{
    EP_STAT estat;

    estat = gdp_init(NULL);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("GDP initialization failed");
        return estat;
    }
    switch (log_mode)
    {
    case GDPFS_LOG_MODE_RO:
        gcl_mode = GDP_MODE_RO;
        break;
    case GDPFS_LOG_MODE_RA:
        gcl_mode = GDP_MODE_RA;
        break;
    case GDPFS_LOG_MODE_AO:
        gcl_mode = GDP_MODE_AO;
        break;
    default:
        ep_app_error("uknown log mode");
        return GDPFS_STAT_INVLDPARAM;
    }
    return GDPFS_STAT_OK;
}

EP_STAT gdpfs_log_create(gdp_name_t log_iname)
{
    EP_STAT estat;

    // The internal name of the log server we're going to use
    gdp_name_t logd_iname;

    // The log that we're going to create, and its internal name
    gdp_gcl_t* gcl;
    const gdp_name_t* gcl_iname;

    gdp_parse_name(logd_xname, logd_iname);

    // Metadata for the log (this allocation will succeed; the program exits otherwise)
    gdp_gclmd_t* gmd = gdp_gclmd_new(0);

    // Save creation time as metadata (to generate the log name)
    EP_TIME_SPEC tv;
    char timestring[40];

    ep_time_now(&tv);
    ep_time_format(&tv, timestring, sizeof timestring, EP_TIME_FMT_DEFAULT);
    gdp_gclmd_add(gmd, GDP_GCLMD_CTIME, strlen(timestring), timestring);

    // TODO create a keypair and use it for this log

    estat = gdp_gcl_create(NULL, logd_iname, gmd, &gcl);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to create log.");
        goto fail0;
    }

    // TODO:Error checking here?
    gdp_gclmd_free(gmd);
    gcl_iname = gdp_gcl_getname(gcl);
    memcpy(log_iname, *gcl_iname, sizeof(gdp_name_t));

    estat = gdp_gcl_close(gcl);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];

        ep_app_error("Cannot close GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
    }

    return GDPFS_STAT_OK;

fail0:
    gdp_gclmd_free(gmd);
    return GDPFS_STAT_CREATE_FAILED;
}

EP_STAT gdpfs_log_open(gdpfs_log_t **handle, gdp_name_t gcl_name)
{
    EP_STAT estat;

    // open the GCL
    *handle = ep_mem_zalloc(sizeof(gdpfs_log_t));
    if (*handle == NULL)
    {
        return GDPFS_STAT_OOMEM;
    }
    estat = gdp_gcl_open(gcl_name, gcl_mode, NULL, &(*handle)->gcl_handle);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];

        ep_app_error("Cannot open GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
        goto fail0;
    }

    // TODO: better size protection?
    memcpy((*handle)->gname, gcl_name, sizeof((*handle)->gname) * sizeof(*(*handle)->gname));

    return estat;

fail0:
    ep_mem_free(*handle);
    *handle = NULL;
    return estat;
}

EP_STAT gdpfs_log_close(gdpfs_log_t *handle)
{
    EP_STAT estat;

    estat = gdp_gcl_close(handle->gcl_handle);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];

        ep_app_error("Cannot close GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
    }
    ep_mem_free(handle);
    return estat;
}

EP_STAT gdpfs_log_append(gdpfs_log_t *handle, gdpfs_log_ent_t *ent, gdpfs_callback_t cb, void *udata)
{
    EP_STAT estat;

    if (gcl_mode == GDP_MODE_RO)
    {
        ep_app_error("Cannot append to log in RO mode");
        return GDPFS_STAT_BADLOGMODE;
    }
    estat = gdp_gcl_append_async(handle->gcl_handle, ent->datum, cb, udata);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];
        ep_app_error("Cannot write GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
    }
    return estat;
}

void gdpfs_log_gname(gdpfs_log_t *handle, gdpfs_log_gname_t gname)
{
     memcpy(gname, handle->gname, sizeof(gname) * sizeof(gname[0]));
}

EP_STAT
gdpfs_log_ent_init(gdpfs_log_ent_t *log_ent)
{
    log_ent->datum = gdp_datum_new();
    
    if (log_ent->datum == NULL)
        return GDPFS_STAT_OOMEM;
    else
        return GDPFS_STAT_OK;
}

/*
 * Attempt to open the ent for reco. If it fails, free resources and set ent to
 * NULL.
 */
EP_STAT
gdpfs_log_ent_open(gdpfs_log_t *handle, gdpfs_log_ent_t *ent, gdpfs_recno_t recno)
{
    EP_STAT estat;


    if (gcl_mode == GDP_MODE_AO)
    {
        ep_app_error("Cannot read log ent in AO mode");
        return GDPFS_STAT_BADLOGMODE;
    }
    estat = gdpfs_log_ent_init(ent);
    if (!EP_STAT_ISOK(estat))
        return estat;

    estat = gdp_gcl_read(handle->gcl_handle, recno, ent->datum);
    if (!EP_STAT_ISOK(estat))
    {
        if (EP_STAT_DETAIL(estat) == _GDP_CCODE_NOTFOUND)
        {
            estat = GDPFS_STAT_NOTFOUND;
        }
        else
        {
            char sbuf[100];
            ep_app_error("Cannot read GCL:\n    %s",
                ep_stat_tostr(estat, sbuf, sizeof sbuf));
        }
        goto fail0;
    }
    return estat;

fail0:
    gdp_datum_free(ent->datum);
    return estat;
}

/*
 * Attempt to close the ent. If it's NULL, do nothing.
 */
void gdpfs_log_ent_close(gdpfs_log_ent_t *ent)
{
    gdp_datum_free(ent->datum);
}

size_t gdpfs_log_ent_length(gdpfs_log_ent_t *ent)
{
    return gdp_buf_getlength(gdp_datum_getbuf(ent->datum));
}

/*
 *  Read size bytes from ent into buf. If buf is NULL, skips over data. Returns
 *  number of bytes actually read.
 */
size_t gdpfs_log_ent_read(gdpfs_log_ent_t *ent, void *buf, size_t size)
{
    gdp_buf_t *datum_buf = gdp_datum_getbuf(ent->datum);

    if (buf == NULL)
    {
        if (gdp_buf_drain(datum_buf, size) != 0)
        {
            ep_app_error("Error draining buf");
            return 0;
        }
        return size;
    }
    else
    {
        return gdp_buf_read(datum_buf, buf, size);
    }
}
/*
 *  Read size bytes from ent into buf. Difference between read is that peek does
 *  not advance pointer in buffer
 */
size_t gdpfs_log_ent_peek(gdpfs_log_ent_t *ent, void *buf, size_t size)
{
    gdp_buf_t *datum_buf = gdp_datum_getbuf(ent->datum);
    return gdp_buf_peek(datum_buf, buf, size);
}

/*
 * Write size bytes from buf to ent. Returns 0 on success and -1 on failure.
 */
int gdpfs_log_ent_write(gdpfs_log_ent_t *ent, const void *buf, size_t size)
{
    gdp_buf_t *datum_buf = gdp_datum_getbuf(ent->datum);

    return gdp_buf_write(datum_buf, buf, size);
}
