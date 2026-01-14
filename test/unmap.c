#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

// NOLINTBEGIN
int
main()
{
    char *path   = "mmap_test.lol";
    int fd       = open(path, O_RDWR | O_CREAT, 0666);
    int pagesize = getpagesize();
    char *map =
        mmap(NULL, 10 * pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);


    if (lseek(fd, 10 * pagesize, SEEK_SET) < 0) {
        return -1;
    }
    if (write(fd, &fd, 1) != 1) {
        return -1;
    }
    printf("%lx\n", (uint64_t)map + 5 * pagesize);
    map[0] = 'a';
    sleep(1);
    printf("munmap(map, pagesize): %d\n", munmap(map, pagesize));
    map[5 * pagesize] = 'b';
    sleep(1);
    printf("munmap(map + 2 * pagesize, pagesize): %d\n",
           munmap(map + 2 * pagesize, 2 * pagesize));
    map[5 * pagesize] = 'c';
    sleep(1);
    return 0;
}
// NOLINTEND
