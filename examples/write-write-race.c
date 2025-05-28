#include <pthread.h>
#include <stdio.h>

volatile int x = 0;

void *
run()
{
    x = 1;
    return 0;
}

int
main()
{
    pthread_t t;
    pthread_create(&t, 0, run, 0);
    x = 2;
    pthread_join(t, 0);
    printf("result: %d\n", x);
    return 0;
}
