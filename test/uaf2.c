#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int *x = malloc(sizeof(int));
    free(x);
    *x = 42;
    return 0;
}