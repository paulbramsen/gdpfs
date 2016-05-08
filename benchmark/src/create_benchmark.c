#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>

static void print_usage();
#define BLOCKSIZE 4096
#define BILLION 1000000000
char buffer[100000];
int main(int argc, char** argv)
{
    char *filename;
    int num;
    FILE *output; 
    int i;
    struct timespec first_ts;
    struct timespec second_ts;

    if (argc != 3)
        print_usage();
    filename = argv[1];
    num = atoi(argv[2]);
    output = fopen(filename, "w+");
    for (i = 0; i < num; i++)
    {
        sprintf(buffer, "bench%d", i);
        clock_gettime(CLOCK_REALTIME, &first_ts);
        uint64_t first = ((uint64_t) first_ts.tv_sec * BILLION) + ((uint64_t) first_ts.tv_nsec);
        int fd = open(buffer, O_CREAT | O_TRUNC | O_RDWR, 0744);
        close(fd);
        clock_gettime(CLOCK_REALTIME, &second_ts);
        uint64_t second = ((uint64_t) second_ts.tv_sec * BILLION) + ((uint64_t) second_ts.tv_nsec);
        fprintf(output, "%lu\n", second - first);
    }
}

void print_usage()
{
    printf("Usage: ./create_benchmark <file_name> <number>\n");
}
