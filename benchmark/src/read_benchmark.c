#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


static void print_usage();
#define BLOCKSIZE 4096
#define BILLION 1000000000.0
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
    output = fopen(filename, "w+");
    fd = open("write_file", O_RDONLY);
    for (i = 0; i < num; i++)
    {
        clock_gettime(CLOCK_REALTIME, &first_ts);
        read(fd, block, BLOCKSIZE);
        clock_gettime(CLOCK_REALTIME, &second_ts);
        uint64_t first = ((uint64_t) first_ts.tv_sec * BILLION) + ((uint64_t) first_ts.tv_nsec);
        uint64_t second = ((uint64_t) second_ts.tv_sec * BILLION) + ((uint64_t) second_ts.tv_nsec);
        fprintf(output, "%lu\n", second - first);
    }
}

void print_usage()
{
    printf("Usage: ./read_benchmark <file_name> <number>\n");
}
