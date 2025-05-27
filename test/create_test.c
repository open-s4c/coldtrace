#include <pthread.h>
#include <stdio.h>


void *
run(void *ptr)
{
    printf("Hello\n");
    return 0;
}

int
main()
{
    pthread_t t;
    pthread_create(&t, 0, run, 0);
    pthread_join(t, 0);
    return 0;
}
