#include "gdpfs_log.h"

#include "gdpfs_stat.h"
#include <ep/ep_app.h>
#include <string.h>

struct gdpfs_log
{
    gdp_gcl_t *gcl_handle;
    gdpfs_log_gname_t gname;
};

struct gdpfs_log_ent
{
    gdp_datum_t *datum;
};

EP_STAT init_gdpfs_log()
{
    EP_STAT estat;

    estat = gdp_init(NULL);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("GDP initialization failed");
        return estat;
    }
    return estat;
}

EP_STAT gdpfs_log_open(gdpfs_log_t **handle, char *log_name, gdpfs_log_mode_t mode)
{
    EP_STAT estat;
    gdp_name_t gcl_name;
    gdp_iomode_t gcl_mode;

    // convert the name to internal form
    estat = gdp_parse_name(log_name, gcl_name);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Illegal GCL name syntax:\n\t%s", gcl_name);
        return estat;
    }

    // open the GCL
    switch (mode)
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
        return GDPFS_STAT_INVLDPARAM;
    }
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

EP_STAT gdpfs_log_append(gdpfs_log_t *handle, gdpfs_log_ent_t *ent)
{
    EP_STAT estat;

    estat = gdp_gcl_append(handle->gcl_handle, ent->datum);
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

gdpfs_log_ent_t *gdpfs_log_ent_new()
{
    gdpfs_log_ent_t *log_ent;

    log_ent = ep_mem_zalloc(sizeof(gdpfs_log_ent_t));
    if (log_ent == NULL)
    {
        goto fail0;
    }
    log_ent->datum = gdp_datum_new();
    return log_ent;

fail0:
    return log_ent;
}

/*
 * Attempt to open the ent for reco. If it fails, free resources and set ent to
 * NULL.
 */
EP_STAT gdpfs_log_ent_open(gdpfs_log_t *handle, gdpfs_log_ent_t **ent, gdpfs_recno_t recno)
{
    EP_STAT estat;
    gdpfs_log_ent_t *log_ent;

    log_ent = gdpfs_log_ent_new();
    *ent = log_ent;
    if (log_ent == NULL)
    {
        estat = GDPFS_STAT_OOMEM;
        goto fail0;
    }

    estat = gdp_gcl_read(handle->gcl_handle, recno, log_ent->datum);
    if (!EP_STAT_ISOK(estat))
    {
        if (EP_STAT_DETAIL(estat) == GDP_COAP_NOTFOUND)
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
    if (log_ent)
    {
        gdp_datum_free(log_ent->datum);
        ep_mem_free(log_ent);
        *ent = NULL;
    }
    return estat;
}

/*
 * Attempt to close the ent. If it's NULL, do nothing.
 */
void gdpfs_log_ent_close(gdpfs_log_ent_t *ent)
{
    if (ent)
    {
        gdp_datum_free(ent->datum);
        ep_mem_free(ent);
    }

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
 * Write size bytes from buf to ent. Returns 0 on success and -1 on failure.
 */
int gdpfs_log_ent_write(gdpfs_log_ent_t *ent, const void *buf, size_t size)
{
    gdp_buf_t *datum_buf = gdp_datum_getbuf(ent->datum);

    return gdp_buf_write(datum_buf, buf, size);
}