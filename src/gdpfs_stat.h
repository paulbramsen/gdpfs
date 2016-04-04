#ifndef _GDPFS_STAT_H_
#define _GDPFS_STAT_H_

#include <ep/ep_stat.h>

#define GDPFS_MODULE 2
#define GDPFS_STAT_NEW(sev, det)    EP_STAT_NEW(EP_STAT_SEV_ ## sev,    \
                                        EP_REGISTRY_UCB, GDPFS_MODULE, det)

#define GDPFS_STAT_OK               GDPFS_STAT_NEW(OK,     1)
#define GDPFS_STAT_EOF              GDPFS_STAT_NEW(OK,     2)
#define GDPFS_STAT_NOTFOUND         GDPFS_STAT_NEW(WARN,   3)
#define GDPFS_STAT_RW_FAILED        GDPFS_STAT_NEW(WARN,   4)
#define GDPFS_STAT_OOMEM            GDPFS_STAT_NEW(ERROR,  5)
#define GDPFS_STAT_BADFH            GDPFS_STAT_NEW(ERROR,  6)
#define GDPFS_STAT_INVLDPARAM       GDPFS_STAT_NEW(ERROR,  7)
#define GDPFS_STAT_INVLDMODE        GDPFS_STAT_NEW(ERROR,  8)
#define GDPFS_STAT_INVLDFTYPE       GDPFS_STAT_NEW(ERROR,  9)
#define GDPFS_STAT_CORRUPT          GDPFS_STAT_NEW(ERROR, 10)
#define GDPFS_STAT_BADPATH          GDPFS_STAT_NEW(ERROR, 11)
#define GDPFS_STAT_CREATE_FAILED    GDPFS_STAT_NEW(ERROR, 12)

#endif // _GDPFS_STAT_H_
