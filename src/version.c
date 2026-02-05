#include <coldtrace/version.h>
#include <stdbool.h>

const struct version_header current_version_header = {
    .git_hash = GIT_COMMIT_HASH,
    .padding  = VERSION_PADDING,
    .major    = VERSION_MAJOR,
    .minor    = VERSION_MINOR,
    .patch    = VERSION_PATCH};

bool
version_header_equal(const struct version_header file_header)
{
    return file_header.git_hash == GIT_COMMIT_HASH &&
           file_header.padding == VERSION_PADDING &&
           file_header.major == VERSION_MAJOR &&
           file_header.minor == VERSION_MINOR &&
           file_header.patch == VERSION_PATCH;
}
