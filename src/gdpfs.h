#ifndef _GDPFS_H_
#define _GDPFS_H_

#include <gdp/gdp.h>

int gdpfs_run(char *gclpname, bool ro_mode, int fuse_argc, char *fuse_argv[]);
void gdpfs_shutdown();

#endif // _GDPFS_H_