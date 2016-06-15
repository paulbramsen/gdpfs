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

#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>


static void print_usage();
#define BLOCKSIZE 131072
#define BILLION 1000000000
char buffer[100000];
int main(int argc, char** argv)
{
    char *filename;
    int num;
    char *block;
    FILE *output;
    int i;
    int fd;
    struct timespec first_ts;
    struct timespec second_ts;

    if (argc != 3)
        print_usage();
    filename = argv[1];
    num = atoi(argv[2]);
    block = malloc(BLOCKSIZE);
    memset(block, 'A', BLOCKSIZE);
    output = fopen(filename, "w+");
    for (i = 0; i < num; i++)
    {
        clock_gettime(CLOCK_REALTIME, &first_ts);
        fd = open("write_file", O_CREAT | O_RDWR | O_APPEND, 0744);
        write(fd, block, BLOCKSIZE);
        close(fd);
        clock_gettime(CLOCK_REALTIME, &second_ts);
        uint64_t first = ((uint64_t) first_ts.tv_sec * BILLION) + ((uint64_t) first_ts.tv_nsec);
        uint64_t second = ((uint64_t) second_ts.tv_sec * BILLION) + ((uint64_t) second_ts.tv_nsec);
        fprintf(output, "%lu\n", second - first);
    }
}

void print_usage()
{
    printf("Usage: ./write_benchmark <file_name> <number>\n");
}
