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

#define _GNU_SOURCE

#include "gdpfs_file.h"
#include "gdpfs_stat.h"
#include "gdpfs.h"

#include <ep/ep_app.h>
#include <ep/ep_hash.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bitmap.h"
#include "bitmap_file.h"
#include "list.h"
#include "figtree/figtree.h"
#include "figtree/figtreenode.h"
#include <assert.h>
#include <errno.h>
#include <dirent.h>

#include <ep/ep_thr.h>


#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define MAGIC_NUMBER 0xb531479b64f64e0d

// Uncomment this if you want to use the bitmap for some reason
//#define USE_BITMAP

// TODO: file should store a copy of the meta data. This makes writes easier.
typedef struct
{
    gdpfs_log_t *log_handle;
    char *hash_key;
    uint32_t ref_count;
    int cache_fd;
#ifdef USE_BITMAP
    int cache_bitmap_fd;
#endif
    struct list_elem rc_elem; // for the recently closed cache
    bool new_file; // Used as an optimization: if this file is new, then the cache is fully up-to-date
    bool info_cache_valid;
    gdpfs_file_info_t info_cache;
    gdpfs_recno_t last_recno;

    int outstanding_reqs;
    int index_flush_reqs; // true if the index has been flushed to the log
    bool recently_closed; // true if this file is on the second chance list
    EP_THR_MUTEX index_flush_lock;
    EP_THR_COND index_flush_cond;

    figtree_t figtree;
    bool figtree_initialized;
    EP_THR_MUTEX ref_count_lock;
    EP_THR_MUTEX cache_lock;
    EP_THR_RWLOCK figtree_lock;
} gdpfs_file_t;

typedef struct
{
    off_t start;
    size_t len;
    int* numleft;
    EP_THR_MUTEX* lock;
    EP_THR_COND* condvar;
    char* writebuf;
} gdpfs_readstate_t;

#define MAX_FHS 1024
#define RC_CAP 256
static bitmap_t *fhs;
static gdpfs_file_t **files;
static EP_HASH *file_hash;
static bool use_cache;


static EP_THR_MUTEX rc_lock;
static EP_THR_MUTEX open_lock;
static struct list recently_closed;
static int recently_closed_size;

// Private Functions
static size_t do_write(uint64_t fh, const char *buf,
        size_t size, off_t offset, const gdpfs_file_info_t *info);
static gdpfs_file_t *lookup_fh(uint64_t fh);
static EP_STAT gdpfs_file_fill_cache(gdpfs_file_t *file, const void *buffer, size_t size,
        off_t offset, bool overwrite);
static bool gdpfs_file_get_cache(gdpfs_file_t *file, void *buffer, size_t size,
        off_t offset);
static EP_STAT _file_load_info_cache(gdpfs_file_t* file);
static EP_STAT _file_get_info_raw(gdpfs_file_info_t** info, gdpfs_file_t* file);
EP_STAT _file_dealloc(gdpfs_file_t* file);
EP_STAT _file_unref(gdpfs_file_t* file);
EP_STAT _file_ref(gdpfs_file_t* file);
EP_STAT _recently_closed_insert(gdpfs_file_t* file);
void _file_chkpt(gdpfs_file_t* file, bool do_callback);

EP_STAT
init_gdpfs_file(gdpfs_file_mode_t fs_mode, bool _use_cache, char *gdp_router_addr)
{
    EP_STAT estat;
    DIR *dirp;
    struct dirent *dp;

    use_cache = _use_cache;

    estat = GDPFS_STAT_OOMEM;

    /* Initialize cache of recently closed files. */
    list_init(&recently_closed);
    recently_closed_size = 0;
    if (ep_thr_mutex_init(&rc_lock, EP_THR_MUTEX_NORMAL) != 0)
        return GDPFS_STAT_SYNCH_FAIL;
    if (ep_thr_mutex_init(&open_lock, EP_THR_MUTEX_NORMAL) != 0)
        return GDPFS_STAT_SYNCH_FAIL;

    files = ep_mem_zalloc(sizeof(gdpfs_file_t *) * MAX_FHS);
    if (files == NULL)
        goto fail2;
    fhs = bitmap_create(MAX_FHS);
    if (fhs == NULL)
        goto fail1;
    file_hash = ep_hash_new("file_hash", NULL, MAX_FHS);
    if (file_hash == NULL)
        goto fail0;

    estat = init_gdpfs_log(fs_mode, gdp_router_addr);
    if (!EP_STAT_ISOK(estat))
        goto fail0;

    if (use_cache)
    {
        // set up caching directory
        estat = GDPFS_STAT_LOCAL_FS_FAIL;
        if (mkdir(CACHE_DIR, 0744) != 0 && errno != EEXIST)
            goto fail0;
        if ((dirp = opendir(CACHE_DIR)) == NULL)
            goto fail0;
        do {
            if ((dp = readdir(dirp)) != NULL)
            {
                char *unlink_path = ep_mem_zalloc(strlen(CACHE_DIR)
                                         + strlen("/") + strlen(dp->d_name) + 1);
                sprintf(unlink_path, "%s/%s", CACHE_DIR, dp->d_name);
                unlink(unlink_path);
                ep_mem_free(unlink_path);
            }
        } while (dp != NULL);
    }
    return GDPFS_STAT_OK;

fail0:
    ep_hash_free(file_hash);
fail1:
    bitmap_free(fhs);
fail2:
    ep_mem_free(files);
    return estat;
}

void
checkpoint_file_on_stop(size_t keylen, const void* key, void* val, va_list av)
{
    if (val != NULL)
        _file_chkpt((gdpfs_file_t*) val, false);
}

void
stop_gdpfs_file()
{
	ep_hash_forall(file_hash, checkpoint_file_on_stop);
	sleep(10);
    ep_hash_free(file_hash);
    bitmap_free(fhs);
}

EP_STAT
_recently_closed_insert(gdpfs_file_t* file)
{
    EP_STAT estat;
    EP_ASSERT(!file->recently_closed);

    EP_ASSERT(ep_thr_mutex_lock(&rc_lock) == 0);
    file->recently_closed = true;
    list_push_front(&recently_closed, &file->rc_elem);
    EP_ASSERT(recently_closed_size <= RC_CAP && recently_closed_size >= 0);
    if (recently_closed_size == RC_CAP)
    {
        gdpfs_file_t *oldfile = list_entry(list_pop_back(&recently_closed), gdpfs_file_t, rc_elem);
        oldfile->recently_closed = false; // not in list anymore!
        EP_ASSERT(ep_thr_mutex_unlock(&rc_lock) == 0);
        estat = _file_dealloc(oldfile);
    }
    else
    {
        recently_closed_size++;
        EP_ASSERT(ep_thr_mutex_unlock(&rc_lock) == 0);
        estat = GDPFS_STAT_OK;
    }

    return estat;
}

void
_recently_closed_revive(gdpfs_file_t* file)
{
    EP_ASSERT(file->recently_closed);
    file->recently_closed = false;
    EP_ASSERT(ep_thr_mutex_lock(&rc_lock) == 0);
    list_remove(&file->rc_elem);
    recently_closed_size--;
    EP_ASSERT(ep_thr_mutex_unlock(&rc_lock) == 0);
}

EP_STAT
gdpfs_file_create(uint64_t* fhp, gdpfs_file_gname_t log_iname,
        gdpfs_file_type_t type, gdpfs_file_perm_t perm)
{
    EP_STAT estat;
    uint64_t fh;

    estat = gdpfs_log_get_precreated(log_iname);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to create file");
        return estat;
    }

    fh = gdpfs_file_open_init(&estat, log_iname, type, perm, true);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to initialize file");
        return estat;
    }

    if (fhp)
        *fhp = fh;
    return GDPFS_STAT_OK;
}

static EP_STAT
open_file(uint64_t *fhp, gdpfs_file_gname_t log_name, gdpfs_file_type_t type,
        gdpfs_file_mode_t perm, bool init, bool strict_init)
{
    EP_STAT estat;
    uint64_t fh;
    gdpfs_file_t *file = NULL;
    char *cache_name = NULL;
    char *cache_bitmap_name = NULL;
    gdp_pname_t printable;

    *fhp = -1;

    fh = bitmap_reserve(fhs);
    if (fh == -1)
    {
        estat = GDPFS_STAT_OOMEM;
        goto fail1;
    }

    ep_thr_mutex_lock(&open_lock);
    // check if the file is already open and in the hash
    file = ep_hash_search(file_hash, sizeof(gdpfs_file_gname_t), log_name);

    // if there file isn't currently open, create and open it
    if (!file)
    {
        file = ep_mem_zalloc(sizeof(gdpfs_file_t));
        if (!file)
        {
            estat = GDPFS_STAT_OOMEM;
            goto fail0;
        }
        estat = gdpfs_log_open(&file->log_handle, log_name);
        if (!EP_STAT_ISOK(estat))
            goto fail0;
        file->hash_key = ep_mem_zalloc(sizeof(gdpfs_file_gname_t));
        if (!file->hash_key)
        {
            estat = GDPFS_STAT_OOMEM;
            goto fail0;
        }
        memcpy(file->hash_key, log_name, sizeof(gdpfs_file_gname_t));

        if (use_cache)
        {
            // Initialize cache_name and cache_bitmap names
            gdp_printable_name(log_name, printable);
            if ((cache_name = ep_mem_zalloc(strlen(CACHE_DIR) + strlen("/")
                    + strlen(printable) + 1)) == 0)
            {
                estat = GDPFS_STAT_OOMEM;
                goto fail0;
            }
            sprintf(cache_name, "%s%s%s", CACHE_DIR, "/", printable);
            if ((cache_bitmap_name = ep_mem_zalloc(strlen(CACHE_DIR) + strlen("/")
                    + strlen(printable) + strlen(BITMAP_EXTENSION) + 1)) == 0)
            {
                estat = GDPFS_STAT_OOMEM;
                goto fail0;
            }
            sprintf(cache_bitmap_name, "%s%s%s%s", CACHE_DIR, "/",
                    printable, BITMAP_EXTENSION);

            // Open the cache files and put them in the file struct
            if ((file->cache_fd = open(cache_name, O_RDWR | O_CREAT, 0744)) == -1)
            {
                estat = GDPFS_STAT_LOCAL_FS_FAIL;
                goto fail0;
            }
#ifdef USE_BITMAP
            if ((file->cache_bitmap_fd = open(cache_bitmap_name, O_RDWR | O_CREAT, 0744)) == -1)
            {
                estat = GDPFS_STAT_LOCAL_FS_FAIL;
                close(file->cache_fd);
                goto fail0;
            }
#endif
            ep_mem_free(cache_name);
            ep_mem_free(cache_bitmap_name);
        }

        if (ep_thr_mutex_init(&file->ref_count_lock, EP_THR_MUTEX_NORMAL) != 0 ||
            ep_thr_mutex_init(&file->cache_lock, EP_THR_MUTEX_NORMAL) != 0 ||
            ep_thr_rwlock_init(&file->figtree_lock) != 0 ||
            ep_thr_mutex_init(&file->index_flush_lock, EP_THR_MUTEX_NORMAL) != 0 ||
            ep_thr_cond_init(&file->index_flush_cond) != 0)
        {
            estat = GDPFS_STAT_SYNCH_FAIL;
            goto fail0;
        }

        file->outstanding_reqs = 0;

        // add to hash table at very end to make handling failure cases easier
        ep_hash_insert(file_hash, sizeof(gdpfs_file_gname_t), log_name, file);
    }
    else if (file->recently_closed)
    {
        // Revive it from the recently closed cache if necessary
        _recently_closed_revive(file);
    }
    estat = _file_ref(file);
    files[fh] = file;
    ep_thr_mutex_unlock(&open_lock);
    if (!EP_STAT_ISOK(estat))
        goto fail2;

    if (strict_init)
    {
        // This is guaranteed to be a new file
        file->new_file = true;
    }

    // check type and initialize if necessary
    if (type != GDPFS_FILE_TYPE_UNKNOWN)
    {
        gdpfs_file_info_t* current_info;
        estat = _file_get_info_raw(&current_info, file);
        if (!EP_STAT_ISOK(estat))
        {
            ep_app_error("Failed to load file info");
            goto fail2;
        }
        if (init)
        {
            if (current_info->file_type == GDPFS_FILE_TYPE_NEW)
            {
                current_info->file_size = 0;
                current_info->file_type = type;
                current_info->file_perm = perm;
                do_write(fh, NULL, 0, 0, current_info);
            }
            else if (current_info->file_type == GDPFS_FILE_TYPE_UNKNOWN || strict_init)
            {
                estat = GDPFS_STAT_INVLDFTYPE;
                goto fail2;
            }
        }
        else if (current_info->file_type != type)
        {
            estat = GDPFS_STAT_INVLDFTYPE;
            goto fail2;
        }
    }

    //gdp_pname_t pn;
    //gdp_printable_name(file->log_handle->gname, pn);
    //printf("Log is %s\n", pn);
    ep_thr_rwlock_wrlock(&file->figtree_lock);
    if (strict_init) {
        // Fast path for a common case
        //printf("Optimizing\n");
        ft_init(&file->figtree);
        file->last_recno = 1;
        file->figtree_initialized = true;
    }
    else if (!file->figtree_initialized)
    {
        gdpfs_log_ent_t* ents;
        gdpfs_recno_t recno;
        gdpfs_fmeta_t entry;
        size_t data_size;
        int entslen = 16;
        int enti = 0;
        char bounce[1024];
        size_t read;
        size_t toread;

        ents = ep_mem_zalloc(entslen * sizeof(gdpfs_log_ent_t));
        estat = gdpfs_log_ent_open(file->log_handle, &ents[0], -1, true);
        recno = gdpfs_log_ent_recno(&ents[0]);
        file->last_recno = recno;
        gdpfs_log_ent_close(&ents[0]);
        if (EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
        {
            // Empty log, just initialize the fig tree
            recno = 0;
        }
        for (; recno > 0; recno--)
        {
            if (enti >= entslen) {
                entslen <<= 1;
                ents = ep_mem_realloc(ents, entslen * sizeof(gdpfs_log_ent_t));
            }

            // Check estat
            estat = gdpfs_log_ent_open(file->log_handle, &ents[enti], recno, true);
            EP_ASSERT_INSIST(EP_STAT_ISOK(estat));
            data_size = gdpfs_log_ent_length(&ents[enti]);
            if (gdpfs_log_ent_peek(&ents[enti], &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
                || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
            {
                ep_app_fatal("Corrupt log entry in file (#1).");
            }

            /* Check if this is the index. */
            if (entry.logent_type == GDPFS_LOGENT_TYPE_CHKPT)
            {
                EP_ASSERT_REQUIRE((entry.ent_size % sizeof(figtree_node_t)) == 0);

                //printf("Found the checkpoint!\n");

                EP_ASSERT(entry.ent_size > 0);

                /* Get the last node in the log; that is the root. */
                figtree_node_t* root = ep_mem_zalloc(sizeof(figtree_node_t));
                gdpfs_log_ent_drain(&ents[enti], data_size - sizeof(figtree_node_t));
                gdpfs_log_ent_read(&ents[enti], root, sizeof(figtree_node_t));
                ft_init_with_root(&file->figtree, root);
                break;
            }

            enti++;
        }

        if (recno == 0) {
            /* No index for this file... */
            //printf("No index for this file\n");
            ft_init(&file->figtree);
        }

        enti--;

        ep_thr_mutex_lock(&file->cache_lock);
        for (; enti >= 0; enti--) {
            data_size = gdpfs_log_ent_length(&ents[enti]);
            if (gdpfs_log_ent_read(&ents[enti], &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
                || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
            {
                ep_app_fatal("Corrupt log entry in file (#2).");
            }
            if (entry.ent_size > 0) {
                //printf("Writing [%lu, %lu]: %lu\n", entry.ent_offset, entry.ent_offset + entry.ent_size - 1, gdpfs_log_ent_recno(&ents[enti]));
                ft_write(&file->figtree, entry.ent_offset, entry.ent_offset + entry.ent_size - 1, gdpfs_log_ent_recno(&ents[enti]), file->log_handle);
                read = 0;
                // while we're at it, populate the cache
                while (read < entry.ent_size) {
                    toread = entry.ent_size - read;
                    if (toread > 1024) {
                        toread = 1024;
                    }
                    EP_ASSERT_INSIST(toread == gdpfs_log_ent_read(&ents[enti], bounce, toread));
                    estat = gdpfs_file_fill_cache(file, bounce, toread, entry.ent_offset + read, true);
                    EP_ASSERT_INSIST(EP_STAT_ISOK(estat));
                    read += toread;
                }
            }
            gdpfs_log_ent_close(&ents[enti]);
        }
        ep_thr_mutex_unlock(&file->cache_lock);

        ep_mem_free(ents);
        file->figtree_initialized = true;
    }
    ep_thr_rwlock_unlock(&file->figtree_lock);

    // success

    *fhp = fh;
    return GDPFS_STAT_OK;

fail0:
    ep_thr_mutex_unlock(&open_lock);
    bitmap_release(fhs, fh);
    if (use_cache)
    {
        ep_mem_free(cache_name);
        ep_mem_free(cache_bitmap_name);
    }
    if (file)
    {
        ep_mem_free(file->hash_key);
        ep_mem_free(file);
    }
fail1:
    return estat;
fail2:
    gdpfs_file_close(fh);
    return estat;
}

uint64_t
gdpfs_file_open(EP_STAT *ret_stat, gdpfs_file_gname_t name)
{
    uint64_t fh;
    *ret_stat = open_file(&fh, name, GDPFS_FILE_TYPE_UNKNOWN, 0, false, false);
    return fh;
}

uint64_t
gdpfs_file_open_type(EP_STAT *ret_stat, gdpfs_file_gname_t name,
        gdpfs_file_type_t type)
{
    uint64_t fh;
    *ret_stat = open_file(&fh, name, type, 0, false, false);
    return fh;
}

uint64_t
gdpfs_file_open_init(EP_STAT *ret_stat, gdpfs_file_gname_t name,
        gdpfs_file_type_t type, gdpfs_file_perm_t perm, bool strict_init)
{
    uint64_t fh;
    *ret_stat = open_file(&fh, name, type, perm, true, strict_init);
    return fh;
}

EP_STAT
gdpfs_file_close(uint64_t fh)
{
    gdpfs_file_t *file;
    EP_STAT estat;

    file = lookup_fh(fh);
    if (file == NULL)
        return GDPFS_STAT_BADFH;
    bitmap_release(fhs, fh);

    estat = _file_unref(file);
    return estat;
}

static void
_file_chkpt_finish(gdp_event_t* ev)
{
    bool dontfree = false;

    gdpfs_file_t* file = gdp_event_getudata(ev);
    if (file == NULL)
        return;

    ep_thr_mutex_lock(&file->index_flush_lock);
    if ((--file->index_flush_reqs) != 0)
    {
        dontfree = true;
    }
    ep_thr_mutex_unlock(&file->index_flush_lock);

    if (dontfree)
        return;

    EP_ASSERT(ep_thr_mutex_lock(&open_lock) == 0);
    EP_ASSERT(ep_thr_mutex_lock(&rc_lock) == 0);
    EP_ASSERT(ep_thr_mutex_lock(&file->ref_count_lock) == 0);
    if (file->ref_count != 0 || file->recently_closed)
    {
        // Drop locks and don't deallocate!
        dontfree = true;
    }
    EP_ASSERT(ep_thr_mutex_unlock(&file->ref_count_lock) == 0);
    EP_ASSERT(ep_thr_mutex_unlock(&rc_lock) == 0);
    EP_ASSERT(ep_thr_mutex_unlock(&open_lock) == 0);

    if (dontfree)
        return;

    ep_hash_delete(file_hash, sizeof(gdpfs_file_gname_t), file->hash_key);
    if (use_cache)
    {
        close(file->cache_fd);
#ifdef USE_BITMAP
        close(file->cache_bitmap_fd);
#endif
    }
    ep_mem_free(file->hash_key);

    ft_dealloc(&file->figtree);

    EP_ASSERT (ep_thr_mutex_destroy(&file->ref_count_lock) == 0);
    EP_ASSERT (ep_thr_mutex_destroy(&file->cache_lock) == 0);
    EP_ASSERT (ep_thr_rwlock_destroy(&file->figtree_lock) == 0);
    EP_ASSERT (ep_thr_mutex_destroy(&file->index_flush_lock) == 0);
    EP_ASSERT (ep_thr_cond_destroy(&file->index_flush_cond) == 0);
    ep_mem_free(file);
}



/* The index_flush_lock must be held when entering this function. */
void
_file_chkpt(gdpfs_file_t* file, bool do_callback)
{
    EP_STAT estat;
    figtree_node_t* chkpt;
    int len;
    gdpfs_log_ent_t ent;
    gdpfs_file_info_t* info;

    EP_ASSERT_REQUIRE (file != NULL);

    estat = _file_get_info_raw(&info, file);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_fatal("Failed to read file size.");
    }

    gdpfs_fmeta_t entry = {
        .file_size   = info->file_size,
        .file_type   = info->file_type,
        .file_perm   = info->file_perm,
        .logent_type = GDPFS_LOGENT_TYPE_CHKPT,
        .ent_offset  = -1,
        .ent_size    = 0,
        .magic       = MAGIC_NUMBER,
    };

    /* Now checkpoint the log. */
    //gdp_pname_t pn;
    //gdp_printable_name(file->_log_handle->gname, pn);
    //printf("%s: Checkpoint at record %ld\n", pn, file->last_recno + 1);
    ep_thr_rwlock_wrlock(&file->figtree_lock);
    get_dirty(&chkpt, &len, &file->figtree, ++file->last_recno);
    ep_thr_rwlock_unlock(&file->figtree_lock);
    if (len > 0)
    {
        file->index_flush_reqs++;
        entry.ent_size = len * sizeof(figtree_node_t);
        gdpfs_log_ent_init(&ent);
        gdpfs_log_ent_write(&ent, &entry, sizeof(gdpfs_fmeta_t));
        gdpfs_log_ent_write(&ent, chkpt, entry.ent_size);
        ep_mem_free(chkpt);

        estat = gdpfs_log_append(file->log_handle, &ent, _file_chkpt_finish, do_callback ? file : NULL);
        EP_ASSERT (EP_STAT_ISOK(estat));
        gdpfs_log_ent_close(&ent);
    }
    else
    {
        ep_mem_free(chkpt);
    }
}

EP_STAT
_file_dealloc(gdpfs_file_t* file)
{
    EP_STAT estat = GDPFS_STAT_OK;

    ep_thr_mutex_lock(&file->index_flush_lock);
    while (file->outstanding_reqs != 0) {
        ep_thr_cond_wait(&file->index_flush_cond, &file->index_flush_lock, NULL);
    }
    _file_chkpt(file, true);
    ep_thr_mutex_unlock(&file->index_flush_lock);

    return estat;
}

EP_STAT
_file_unref(gdpfs_file_t* file)
{
    EP_STAT estat;
    uint32_t new_ref_count;

    if (ep_thr_mutex_lock(&file->ref_count_lock) != 0)
        return GDPFS_STAT_SYNCH_FAIL;
    new_ref_count = --file->ref_count;
    if (ep_thr_mutex_unlock(&file->ref_count_lock) != 0)
        return GDPFS_STAT_SYNCH_FAIL;

    if (new_ref_count == 0)
    {
        estat = _recently_closed_insert(file);
    }
    else
        estat = GDPFS_STAT_OK;
    return estat;
}

EP_STAT
_file_ref(gdpfs_file_t* file)
{
    if (ep_thr_mutex_lock(&file->ref_count_lock) != 0)
        return GDPFS_STAT_SYNCH_FAIL;
    ++file->ref_count;
    if (ep_thr_mutex_unlock(&file->ref_count_lock) != 0)
        return GDPFS_STAT_SYNCH_FAIL;
    return EP_STAT_OK;
}

/*
static size_t do_read_starting_at_rec_no(gdpfs_file_t *file, char *buf, size_t size,
        off_t offset, gdpfs_recno_t rec_no)
{
    EP_STAT estat;
    size_t data_size;
    off_t read_start;
    size_t read_size;
    size_t left_read_size;
    size_t read_so_far;
    gdpfs_fmeta_t entry;
    gdpfs_log_ent_t log_ent;

    if (size == 0)
    {
        return 0;
    }

    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, rec_no);
    if (!EP_STAT_ISOK(estat))
    {
        if (!EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
        {
            char sbuf[100];
            ep_app_error("Issue while reading GCL:\n    %s",
                ep_stat_tostr(estat, sbuf, sizeof sbuf));
        }
        goto fail0;
    }

    data_size = gdpfs_log_ent_length(&log_ent);
    if (gdpfs_log_ent_read(&log_ent, &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
        || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
    {
        ep_app_error("Corrupt log entry in file.");
        goto fail0;
    }

    // limit read size to file size. Even though we may encounter a larger file
    // size deeper down, we don't want to read its data since that means that
    // the log was truncated at some point and everything beyond the truncation
    // point should be 0s.
    size = max(min((int64_t)entry.file_size - (int64_t)offset, (int64_t)size), 0);
    read_start = max(offset, entry.ent_offset);
    read_size = max(min((int64_t)offset + (int64_t)size - (int64_t)read_start,
                        (int64_t)entry.ent_offset + (int64_t)entry.ent_size - (int64_t)read_start), 0);
    if (read_size > 0)
    {

        left_read_size = read_start - offset;

        // this log entry has data we want
        // TODO: error checking
        gdpfs_log_ent_read(&log_ent, NULL, read_start - entry.ent_offset);
        gdpfs_log_ent_read(&log_ent, buf + left_read_size, read_size);
        gdpfs_log_ent_close(&log_ent);

        // do left read
        do_read_starting_at_rec_no(file, buf, left_read_size, offset, rec_no - 1);
        // assume read of size size succeeds (if not then they're just filled
        // with zeros) and do right read.
        read_so_far = left_read_size + read_size;
        do_read_starting_at_rec_no(file, buf + read_so_far, size - read_so_far,
                                   offset + read_so_far, rec_no - 1);
    }
    else
    {
        gdpfs_log_ent_close(&log_ent);
        do_read_starting_at_rec_no(file, buf, size, offset, rec_no - 1);
    }
    return size;

fail0:
    gdpfs_log_ent_close(&log_ent);
    return 0;
}
*/

void handle_read(gdp_event_t* ev)
{
    gdpfs_fmeta_t entry;
    gdpfs_log_ent_t log_ent;
    size_t data_size;
    gdpfs_readstate_t* rs = gdp_event_getudata(ev);
    log_ent.datum = gdp_event_getdatum(ev);

    data_size = gdpfs_log_ent_length(&log_ent);
    if (gdpfs_log_ent_read(&log_ent, &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
        || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
    {
        ep_app_fatal("Corrupt log entry in file (#3).");
    }

    gdpfs_log_ent_drain(&log_ent, rs->start - entry.ent_offset);
    gdpfs_log_ent_read(&log_ent, rs->writebuf, rs->len);

    ep_thr_mutex_lock(rs->lock);
    if (--*rs->numleft == 0) {
        ep_thr_cond_signal(rs->condvar);
    }
    ep_thr_mutex_unlock(rs->lock);
    ep_mem_free(rs);
}

static size_t
do_read(uint64_t fh, char *buf, size_t size, off_t offset)
{
    gdpfs_file_t *file;
    gdpfs_file_info_t *info;
    EP_STAT estat;
    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    estat = _file_get_info_raw(&info, file);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Could not get file info during read: %d", EP_STAT_DETAIL(estat));
        return 0;
    }

    if (size + offset > info->file_size)
    {
        if (offset > info->file_size)
            return 0;
        size = info->file_size - offset;
    }

    // check cache
    if (use_cache) {
        bool hit;
        ep_thr_mutex_lock(&file->cache_lock);
        hit = gdpfs_file_get_cache(file, buf, size, offset);
        ep_thr_mutex_unlock(&file->cache_lock);
        if (hit)
            return size;
    }

    memset(buf, 0, size);

    /* On a miss, traverse the fig tree. */
    if (size > 0)
    {
        fig_t indexgroup;
        figiter_t* figterator;
        int numleft = 0;
        EP_THR_MUTEX lock;
        EP_THR_COND condvar;
        gdpfs_readstate_t* rs;
        ep_thr_mutex_init(&lock, EP_THR_MUTEX_NORMAL);
        ep_thr_cond_init(&condvar);
        ep_thr_rwlock_wrlock(&file->figtree_lock);
        figterator = ft_read(&file->figtree, offset, offset + size - 1, file->log_handle);
        while (fti_next(figterator, &indexgroup, file->log_handle)) {
            if (indexgroup.value == 0) {
                // We have to read from the cache here
                EP_ASSERT(gdpfs_file_get_cache(file, buf + indexgroup.irange.left - offset, indexgroup.irange.right - indexgroup.irange.left + 1,
                                               indexgroup.irange.left));
                continue;
            }
            /* This if statement is really just a workaround for until we can get multiread working... */
            if (indexgroup.value > 0) {
                gdpfs_log_ent_t log_ent;
                size_t data_size, read;
                gdpfs_fmeta_t entry;
                gdpfs_log_ent_open(file->log_handle, &log_ent, indexgroup.value, true);
                data_size = gdpfs_log_ent_length(&log_ent);
                read = gdpfs_log_ent_read(&log_ent, &entry, sizeof(gdpfs_fmeta_t));
                if (read != sizeof(gdpfs_fmeta_t)
                    || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
                {
                    ep_app_fatal("Corrupt log entry in file (#4).");
                }

                gdpfs_log_ent_drain(&log_ent, indexgroup.irange.left - entry.ent_offset);
                EP_ASSERT_REQUIRE(gdpfs_log_ent_read(&log_ent, buf + (indexgroup.irange.left - offset), indexgroup.irange.right - indexgroup.irange.left + 1) ==
                    indexgroup.irange.right - indexgroup.irange.left + 1);
                gdpfs_log_ent_close(&log_ent);
                continue;
            }
            rs = ep_mem_zalloc(sizeof(gdpfs_readstate_t)); // freed by the callback
            rs->lock = &lock;
            rs->condvar = &condvar;
            rs->numleft = &numleft;
            rs->start = indexgroup.irange.left;
            rs->len = indexgroup.irange.right - indexgroup.irange.left + 1;
            rs->writebuf = buf + (indexgroup.irange.left - offset);
            ep_thr_mutex_lock(&lock);
            numleft++;
            ep_thr_mutex_unlock(&lock);
            estat = gdp_gcl_multiread(file->log_handle->gcl_handle, indexgroup.value, 1, NULL, rs);
            EP_ASSERT(EP_STAT_ISOK(estat));
        }
        ep_thr_rwlock_unlock(&file->figtree_lock);
        fti_free(figterator);

        ep_thr_mutex_lock(&lock);
        // Technically we could make this an "if", but "while" is more idiomatic
        while (numleft > 0) {
            ep_thr_cond_wait(&condvar, &lock, NULL);
        }
        ep_thr_mutex_unlock(&lock);

        ep_thr_mutex_destroy(&lock);
        ep_thr_cond_destroy(&condvar);

        ep_thr_mutex_lock(&file->cache_lock);
        estat = gdpfs_file_fill_cache(files[fh], buf, size, offset, true);
        ep_thr_mutex_unlock(&file->cache_lock);

        return size;
    }

    return 0;
}

size_t
gdpfs_file_read(uint64_t fh, void *buf, size_t size, off_t offset)
{
    return do_read(fh, buf, size, offset);
}

static void
free_fileref(gdp_event_t* ev)
{
    EP_STAT estat;
    gdpfs_file_t* file = gdp_event_getudata(ev);
    ep_thr_mutex_lock(&file->index_flush_lock);
    if ((--file->outstanding_reqs) == 0) {
        ep_thr_cond_signal(&file->index_flush_cond);
    }
    ep_thr_mutex_unlock(&file->index_flush_lock);
    estat = gdp_event_getstat(ev);
    if (!EP_STAT_ISOK(estat))
        ep_app_error("Could not properly append: %d", EP_STAT_DETAIL(estat));
    estat = _file_unref(file);
    if (!EP_STAT_ISOK(estat))
        ep_app_error("Could not unreference file at %p", file);
}

// TODO: do_write should probably return an EP_STAT so we can error check
static size_t
do_write(uint64_t fh, const char *buf, size_t size, off_t offset,
    const gdpfs_file_info_t *info)
{
    EP_STAT estat;
    gdpfs_file_t *file;
    size_t written = 0;
    gdpfs_log_ent_t log_ent;
    gdpfs_recno_t rc;
    gdpfs_fmeta_t entry = {
        .file_size   = info->file_size,
        .file_type   = info->file_type,
        .file_perm   = info->file_perm,
        .logent_type = GDPFS_LOGENT_TYPE_DATA,
        .ent_offset  = offset,
        .ent_size    = size,
        .magic       = MAGIC_NUMBER,
    };

    // TODO: where are perms checked?

    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    estat = gdpfs_log_ent_init(&log_ent);
    if (!EP_STAT_ISOK(estat))
        goto fail1;
    if (gdpfs_log_ent_write(&log_ent, &entry, sizeof(gdpfs_fmeta_t)) != 0)
    {
        ep_app_error("Failed on metadata write to log entry");
        goto fail0;
    }
    if (gdpfs_log_ent_write(&log_ent, buf, size) != 0)
    {
        ep_app_error("Failed on data write to log entry");
        goto fail0;
    }

    estat = _file_ref(file);
    if (!EP_STAT_ISOK(estat))
        goto fail0;

    ep_thr_mutex_lock(&file->index_flush_lock);
    file->outstanding_reqs++;
    ep_thr_mutex_unlock(&file->index_flush_lock);

    // Write to cache. true is for overwriting cache
    if (use_cache)
    {
        ep_thr_mutex_lock(&file->cache_lock);
        gdpfs_file_fill_cache(file, buf, size, offset, true);
        ep_thr_rwlock_wrlock(&file->figtree_lock);
        ep_thr_mutex_unlock(&file->cache_lock);
    }
    else
        ep_thr_rwlock_wrlock(&file->figtree_lock);

    rc = ++file->last_recno;

    if (size > 0)
        ft_write(&file->figtree, offset, offset + size - 1, rc, file->log_handle);

    estat = gdpfs_log_append(file->log_handle, &log_ent, free_fileref, file);

    ep_thr_rwlock_unlock(&file->figtree_lock);

    if (EP_STAT_ISOK(estat))
    {
        written = size;
    }

fail0:
    // remember to free our resources
    gdpfs_log_ent_close(&log_ent);
fail1:
    return written;
}

size_t
gdpfs_file_write(uint64_t fh, const void *buf, size_t size, off_t offset)
{
    EP_STAT estat;
    size_t potential_size;
    gdpfs_file_info_t* info;

    estat = gdpfs_file_get_info(&info, fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to read file size.");
        return 0;
    }
    potential_size = offset + size;
    info->file_size = max(info->file_size, potential_size);

    return do_write(fh, buf, size, offset, info);
}

// TODO return an EP_STAT
int
gdpfs_file_ftruncate(uint64_t fh, size_t file_size)
{
    EP_STAT estat;
    gdpfs_file_info_t* info;

    estat = gdpfs_file_get_info(&info, fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("failed to get file info");
        return 0;
    }
    info->file_size = 0;
    return do_write(fh, NULL, 0, 0, info);
}

EP_STAT
gdpfs_file_set_perm(uint64_t fh, gdpfs_file_perm_t perm)
{
    EP_STAT estat;
    gdpfs_file_info_t* info;

    estat = gdpfs_file_get_info(&info, fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("failed to get file info");
        return estat;
    }
    info->file_perm = perm;
    do_write(fh, NULL, 0, 0, info);
    return GDPFS_STAT_OK;
}

EP_STAT
gdpfs_file_set_info(uint64_t fh, gdpfs_file_info_t* info)
{
    do_write(fh, NULL, 0, 0, info);
    return GDPFS_STAT_OK;
}

EP_STAT
gdpfs_file_get_info(gdpfs_file_info_t** info, uint64_t fh)
{
    gdpfs_file_t* file = lookup_fh(fh);
    if (file == NULL)
        return GDPFS_STAT_BADFH;

    return _file_get_info_raw(info, file);
}

static EP_STAT
_file_get_info_raw(gdpfs_file_info_t** info, gdpfs_file_t* file)
{
    if (!file->info_cache_valid)
    {
        EP_STAT estat = _file_load_info_cache(file);
        if (!EP_STAT_ISOK(estat))
        {
            ep_app_error("Failed to load file info cache.");
            return estat;
        }
        file->info_cache_valid = true;
    }

    *info = &file->info_cache;
    return GDPFS_STAT_OK;
}


static EP_STAT
_file_load_info_cache(gdpfs_file_t* file)
{
    EP_STAT estat;
    gdpfs_fmeta_t curr_entry;
    gdpfs_log_ent_t log_ent;
    gdpfs_file_info_t *info = &file->info_cache;
    size_t read;

    memset(info, 0, sizeof(gdpfs_file_info_t));
    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, -1, true);
    if (EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
    {
        // no entries yet so file type is new
        info->file_size = 0;
        info->file_type = GDPFS_FILE_TYPE_NEW;
        info->file_perm = 0;
    }
    else if (!EP_STAT_ISOK(estat))
    {
        goto fail0;
    }
    else
    {
        read = gdpfs_log_ent_read(&log_ent, &curr_entry, sizeof(gdpfs_fmeta_t));
        if (read != sizeof(gdpfs_fmeta_t))
        {
            estat = GDPFS_STAT_CORRUPT;
            goto fail0;
        }
        info->file_size = curr_entry.file_size;
        info->file_type = curr_entry.file_type;
        info->file_perm = curr_entry.file_perm;
        // remember to free our resources
        gdpfs_log_ent_close(&log_ent);

    }
    return GDPFS_STAT_OK;

fail0:
    info->file_size = 0;
    info->file_type = GDPFS_FILE_TYPE_UNKNOWN;
    info->file_perm = 0;

    return estat;
}

static gdpfs_file_t *
lookup_fh(uint64_t fh)
{
    int set;

    set = bitmap_is_set(fhs, fh);
    if (set < 0)
    {
        ep_app_error("recieved bad file descriptor:%Lu", fh);
        return NULL;
    }
    return files[fh];
}

void
gdpfs_file_gname(uint64_t fh, gdpfs_file_gname_t gname)
{
    memcpy(gname, files[fh]->hash_key, sizeof(gdpfs_file_gname_t));
}

/**
 * Fill cache with size bytes from the buffer starting at offset
 * If overwrite is true, then fill in bytes even if they're already in the cache.
 * If overwrite is false, then do not fill in bytes when they are already in the cache.
 */
static EP_STAT gdpfs_file_fill_cache(gdpfs_file_t *file, const void *buffer, size_t size,
        off_t offset, bool overwrite)
{
    EP_STAT estat;

    if (!use_cache)
    {
        ep_app_error("Illegal call to gdpfs_file_fill_cache with cache disabled.");
        return GDPFS_STAT_INVLDMODE;
    }

    if (overwrite)
    {
        if (lseek(file->cache_fd, offset, SEEK_SET) < 0)
            goto fail0;
        if (write(file->cache_fd, buffer, size) != size)
            goto fail0;
#ifdef USE_BITMAP
        if (size != 0)
            bitmap_file_set_range(file->cache_bitmap_fd, offset, offset + size);
#endif
    }
    else
    {
        ep_app_fatal("We haven't handled this case yet!");
        /*
        bitmap = bitmap_file_get_range(file->cache_bitmap_fd, offset, offset+size);
        for (byte = 0; byte < size; byte++)
        {
            if (bitmap_is_set(bitmap, byte))
            {
                if ((write(file->cache_fd, buffer + byte, 1) != 1))
                {
                    bitmap_free(bitmap);
                    goto fail0;
                }
            }
        }
        bitmap_free(bitmap);
        */
    }
    estat = GDPFS_STAT_OK;
    return estat;

fail0:
    estat = GDPFS_STAT_OOMEM;
    return estat;
}

static bool gdpfs_file_get_cache(gdpfs_file_t *file, void *buffer, size_t size,
        off_t offset)
{
    ssize_t rv;
    bool hit;
#ifndef USE_BITMAP
    off_t lrv;
#endif

    if (!use_cache)
    {
        ep_app_error("Illegal call to gdpfs_file_get_cache with cache disabled.");
        return false;
    }

    /* Optimization: if the file was opened, then its cache is by definition up-to-date.
     * Without this optimization, the client may query the GDP for things it doesn't have
     * to. For example, suppose that the user writes at bytes 1000 to 1004 without writing
     * any other bytes. 0 to 999 is a "hole" in the cache. Normally, the client will assume
     * that this isn't cached, and will go to the log daemon to fetch this data. But if
     * the client created the file, then they would have been informed via any subscriptions
     * of updates to the file. So the cache will be up-to-date and the client can safely
     * return zeros without querying the log daemon.
     * Somehow, compilers actually perform this pattern of writes.
     */
    if (file->new_file)
    {
        hit = true;
        goto check;
    }

    if (size == 0)
        return true;

#ifndef USE_BITMAP
    lrv = lseek(file->cache_fd, offset, SEEK_HOLE);
    if (lrv == (off_t) -1)
        hit = false;
    else
        hit = (lrv >= offset + size);
#else

    hit = bitmap_file_isset(file->cache_bitmap_fd, offset, offset + size);

#endif

check:
    if (hit)
    {
        lseek(file->cache_fd, offset, SEEK_SET);
        rv = read(file->cache_fd, buffer, size);
        if (rv != size)
        {
            ep_app_error("Cache is corrupt!\n");
            return false;
        }
    }
    else
    {
        //printf("Cache miss\n");
    }
    return hit;

    /*bitmap = bitmap_file_get_range(file->cache_bitmap_fd, offset, offset+size);
    for (byte = 0; byte < size; byte++)
    {
        if (!bitmap_is_set(bitmap, byte))
        {
            bitmap_free(bitmap);
            goto fail0;
        }
    }
    lseek(file->cache_fd, SEEK_SET, offset);
    assert(read(file->cache_fd, buffer, size) == size);
    return true;

fail0:
    return false;
    */
}

