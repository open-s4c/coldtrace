
#include <dice/self.h>
#include <dice/thread_id.h>
#include <stdbool.h>

const char *coldtrace_path(void);

static int
_copy_proc_maps(const char *path)
{
    pid_t pid = getpid();
    char src[128];
    char dst[4096];

    snprintf(src, sizeof(src), "/proc/%d/maps", pid);
    snprintf(dst, sizeof(dst), "%s/maps", path);

    // Open source file
    FILE *fpi = fopen(src, "r");
    if (!fpi) {
        perror("fopen source");
        return -1;
    }

    // Ensure target directory exists
    // Open destination file
    FILE *fpo = fopen(dst, "w+");
    if (!fpo) {
        perror("fopen destination");
        fclose(fpi);
        return -1;
    }

    // Copy contents
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fpi)) > 0) {
        if (fwrite(buffer, 1, bytes, fpo) != bytes) {
            perror("fwrite");
            fclose(fpo);
            fclose(fpi);
            return -1;
        }
    }

    fclose(fpo);
    fclose(fpi);
    return 0;
}

static bool _maps_copied = false;
PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_THREAD_INIT, {
    if (_maps_copied)
        return PS_CB_OK;

    /* only copy the proc maps when the second thread gets initialized */
    if (self_id(md) == MAIN_THREAD)
        return PS_CB_OK;

    _maps_copied = true;
    if (_copy_proc_maps(coldtrace_path()) != 0)
        abort();
})
