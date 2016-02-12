#ifndef _GDPFS_STAT_H_
#define _GDPFS_STAT_H_

#include <ep/ep_stat.h>

#define GDPFS_MODULE 2
#define GDPFS_STAT_NEW(sev, det)    EP_STAT_NEW(EP_STAT_SEV_ ## sev,    \
                                        EP_REGISTRY_UCB, GDPFS_MODULE, det)

#define GDPFS_STAT_OK               GDPFS_STAT_NEW(OK,    1)
#define GDPFS_STAT_OOMEM            GDPFS_STAT_NEW(ERROR, 2)
#define GDPFS_STAT_NOTFOUND         GDPFS_STAT_NEW(WARN,  3)
#define GDPFS_STAT_BADFD            GDPFS_STAT_NEW(ERROR, 4)

#endif // _GDPFS_STAT_H_
