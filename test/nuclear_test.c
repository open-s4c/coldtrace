#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#define SCALING 10

int important_calculation(int i, int n, int m) {
    int temp = n + m;
    i--;
    if (i < 1) {
        return temp;
    }
    return important_calculation(i, m, temp);

}

void* compute_something(void* ptr) {
    void* petra;
    petra = malloc(0);
    for (size_t i = 0; i < SCALING; i++) {
        free(petra);
        petra = malloc(i);
    }
    free(petra);

    int* iptr = (int *) ptr;
    int sum = 0;
    for (size_t i = 0; i < SCALING - 1; i++)
    {
        sum += iptr[i];
    }
    printf("sum = %d\n", sum);
    iptr[5] = sum;
    return NULL;
}



int main() {
    printf("Hello, World! %d\n", important_calculation(3, 0, 1));
    int* ptr = malloc(sizeof(int) * SCALING);
    ptr[3] = 5;
    printf("ptr[3]=%d\n", ptr[3]);

    for (size_t i = 0; i < SCALING; i++)
    {
        printf("ptr[%ld]=%d\n", i, ptr[i]);
    }

    volatile atomic_bool atom_b;
    atomic_init(&atom_b, false);
    pthread_t thread_id[5];
    pthread_create(thread_id, NULL, compute_something, ptr);
    pthread_create(thread_id + 1, NULL, compute_something, ptr);
    ptr[3] = 31;
    atomic_store(&atom_b, true);
    printf("Updated: %d\n", atomic_load(&atom_b));
    pthread_join(thread_id[0], NULL);
    pthread_join(thread_id[1], NULL);
    free(ptr);

    return 0;
}
