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
#define GDPFS_STAT_BADLOGMODE       GDPFS_STAT_NEW(ERROR, 10)
#define GDPFS_STAT_CORRUPT          GDPFS_STAT_NEW(ERROR, 11)
#define GDPFS_STAT_BADPATH          GDPFS_STAT_NEW(ERROR, 12)
#define GDPFS_STAT_CREATE_FAILED    GDPFS_STAT_NEW(ERROR, 13)
#define GDPFS_STAT_FILE_EXISTS      GDPFS_STAT_NEW(ERROR, 14)
#define GDPFS_STAT_DIR_EXISTS       GDPFS_STAT_NEW(ERROR, 15)
#define GDPFS_STAT_DIR_NOT_EMPTY    GDPFS_STAT_NEW(ERROR, 16)
#define GDPFS_STAT_LOCAL_FS_FAIL    GDPFS_STAT_NEW(ERROR, 17)
#define GDPFS_STAT_SYNCH_FAIL       GDPFS_STAT_NEW(ERROR, 18)

#endif // _GDPFS_STAT_H_
