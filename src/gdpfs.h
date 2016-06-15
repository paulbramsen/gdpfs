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
