#include <stdio.h>
#include "interval.h"

int main(int argc, char** argv) {
    struct interval i;
    i_init(&i, 1, 7);
    printf("%d\n", i_contains_val(&i, 7));
}
