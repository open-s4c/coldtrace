#ifndef COLDTRACE_VERSION_H
#define COLDTRACE_VERSION_H

#include <stdint.h>

struct version_header {
    uint32_t git_hash; // first 4 bytes of commit hash
    uint8_t padding;
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
};

#endif // COLDTRACE_VERSION_H
