#include <dice/self.h>
#include <dice/thread_id.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vsync/spinlock/caslock.h>

#define MAX_LINE_LENGTH 256
#define MAX_PATH_LENGTH 1024

const char *coldtrace_path(void);
bool has_ext_(const char *fname, const char *ext);
static int _mkdir(const char *path);
static int _cp(const char *src, const char *dest);

static int
_copy_proc_maps(const char *path)
{
    pid_t pid = getpid();
    char src[MAX_PATH_LENGTH];
    char dst[MAX_PATH_LENGTH];

    snprintf(src, sizeof(src), "/proc/%d/maps", pid);
    snprintf(dst, sizeof(dst), "%s/maps", path);

    if (_mkdir(path) != 0)
        return -1;
    if (_cp(src, dst) != 0)
        return -1;

    return 0;
}

static int
_copy_mapped_files(const char *path)
{
    char proc_maps[MAX_PATH_LENGTH];
    char line[MAX_LINE_LENGTH];
    char dst[MAX_PATH_LENGTH];
    char *last_slash;
    char unique[MAX_PATH_LENGTH * 1000] = {0};

    // Open proc maps file in path.
    snprintf(proc_maps, sizeof(proc_maps), "%s/maps", path);
    FILE *file = fopen(proc_maps, "r");
    if (!file) {
        perror("Failed to open proc map file");
        return -1;
    }

    // For each line in proc maps, copy mapped file if exists.
    while (fgets(line, sizeof(line), file)) {
        // Find file path at the end of the line.
        char *src = strchr(line, '/');
        if (!src)
            continue;

        // Remove newline if present.
        src[strcspn(src, "\n")] = 0;

        // Skip duplicates.
        if (strstr(unique, src))
            continue;

        // skip files with .bin extension
        if (!has_ext_(src, ".bin"))
            continue;

        // Create the destination path.
        snprintf(dst, sizeof(dst), "%s%s", path, src);
        last_slash = strrchr(dst, '/');
        if (last_slash) {
            // Directory is up to the last slash. Remove it, create directory,
            // and then restore slash.
            *last_slash = '\0';
            if (_mkdir(dst) != 0)
                return -1;
            *last_slash = '/';
        }

        if (_cp(src, dst) != 0)
            return -1;

        // Mark this file as copied.
        strcat(unique, ":");
        strcat(unique, src);
    }

    fclose(file);
    return 0;
}

/* -----------------------------------------------------------------------------
 * subscriber callback
 * -------------------------------------------------------------------------- */

static bool _maps_copied = false;
static caslock_t _lock;
PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_THREAD_FINI, {
    caslock_acquire(&_lock);
    if (_maps_copied)
        goto out;

    // Only copy the proc maps when the main thread finishes
    // Technicallly _lock is not necessary in this case, but I am leaving it
    // there because I am not sure whether we should wait until the main threads
    // finishes or another thread instead. In the latter case, we'd need lock.
    if (self_id(md) != MAIN_THREAD)
        goto out;

    _maps_copied = true;

    if (_copy_proc_maps(coldtrace_path()) != 0)
        abort();
    if (_copy_mapped_files(coldtrace_path()) != 0)
        abort();

out:
    caslock_release(&_lock);
    return PS_CB_OK;
})


/* -----------------------------------------------------------------------------
 * helper functions
 * -------------------------------------------------------------------------- */

static int
_mkdir(const char *path)
{
    char temp[MAX_PATH_LENGTH];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (temp[len - 1] == '/') {
        temp[len - 1] = 0;
    }

    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(temp, S_IRWXU) && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(temp, S_IRWXU) && errno != EEXIST ? -1 : 0;
}

static int
_cp(const char *src, const char *dst)
{
    FILE *fsrc = fopen(src, "rb");
    if (!fsrc) {
        perror("fopen");
        return -1;
    }

    FILE *fdst = fopen(dst, "wb");
    if (!fdst) {
        fclose(fsrc);
        perror("fopen");
        return -1;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fsrc)) > 0) {
        fwrite(buffer, 1, bytes, fdst);
    }

    fclose(fsrc);
    fclose(fdst);
    return 0;
}
