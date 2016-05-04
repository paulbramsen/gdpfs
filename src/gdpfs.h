#ifndef _GDPFS_PRIV_H_
#define _GDPFS_PRIV_H_

#include <stdbool.h>

#define CACHE_DIR "/tmp/gdpfs-cache"
#define BITMAP_EXTENSION "-bitmap"

int
gdpfs_run(char *root_log, char *gdp_router_addr, bool ro_mode, bool use_cache, int fuse_argc, char *fuse_argv[]);

void
gdpfs_stop();

#endif // _GDPFS_PRIV_H_
