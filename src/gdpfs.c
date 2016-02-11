#include "gdpfs.h"

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <sysexits.h>

#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include "gdpfs_file.h"
#include "gdpfs_stat.h"

#define MAGIC_NUMBER 0xb531479b64f64e0d

struct gdpfs_entry
{
    size_t file_size;
    off_t ent_offset;
    size_t ent_size;
    bool is_dir;
    uint64_t magic;
};

typedef struct gdpfs_entry gdpfs_entry_t;

static char *log_path;
static bool ro_mode = false;

static gdpfs_file_t *file_handle;

static int
gdpfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, log_path) == 0) {
        stbuf->st_mode = S_IFREG | (ro_mode ? 0444 : 0644);
        stbuf->st_nlink = 1;
        stbuf->st_size = gdpfs_file_size(file_handle);
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
    } else
        res = -ENOENT;
    return res;
}

static int
gdpfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
              off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, log_path + 1, NULL, 0);
    return 0;
}

static int
gdpfs_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, log_path) != 0)
        return -ENOENT;
    return 0;
}

static int
gdpfs_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi)
{
    (void) fi;
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_read(file_handle, buf, size, offset);
}

static int
gdpfs_write(const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_write(file_handle, buf, size, offset);
}

static int
gdpfs_truncate(const char *path, off_t file_size)
{
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_ftruncate(file_handle, file_size);
}

static int
gdpfs_ftruncate(const char *path, off_t file_size, struct fuse_file_info *fi)
{
    (void)fi;
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_ftruncate(file_handle, file_size);
}

static int
gdpfs_create(const char *file, mode_t mode, struct fuse_file_info *info)
{
    printf("Create not implemented. File:\"%s\"\n", file);
    return 0;
}

static int
gdpfs_mkdir(const char * file, mode_t mode)
{
    printf("Mkdir not implemented. File:\"%s\"\n", file);
    return 0;
}

static int
gdpfs_chmod (const char *file, mode_t mode)
{
    printf("Chmod not implemented. File:\"%s\"\n", file);
    return 0;
}

static int
gdpfs_chown (const char *file, uid_t uid, gid_t gid)
{
    printf("Chown not implemented. File:\"%s\"\n", file);
    return 0;
}

static struct fuse_operations gdpfs_oper = {
    .getattr        = gdpfs_getattr,
    .readdir        = gdpfs_readdir,
    .open           = gdpfs_open,
    .read           = gdpfs_read,
    .write          = gdpfs_write,
    .truncate       = gdpfs_truncate,
    .ftruncate      = gdpfs_ftruncate,
    .create         = gdpfs_create,
    .mkdir          = gdpfs_mkdir,
    .chmod          = gdpfs_chmod,
    .chown          = gdpfs_chown,
};

int gdpfs_run(char *gclpname, bool ro, int fuse_argc, char *fuse_argv[])
{
    int ret;
    EP_STAT estat;

    ro_mode = ro;
    
    // TODO: trash this
    log_path = malloc(sizeof(char) * (2 + strlen(gclpname)));
    log_path[0] = '/';
    strcpy(log_path + 1, gclpname);

    estat = init_gdpfs_file();
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);
    estat = gdpfs_file_open(&file_handle, gclpname, ro);
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);

    // TODO: properly close resources after fuse runs.
    ret = fuse_main(fuse_argc, fuse_argv, &gdpfs_oper, NULL);
    gdpfs_stop();
    return ret;
}

void gdpfs_stop()
{
    gdpfs_file_close(file_handle);
    free(log_path);
}