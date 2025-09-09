#include "checkers/trace_checker.h"

#include <assert.h>
#include <dice/log.h>
#include <stdbool.h>

static bool closed;
CHECK_FUNC void
when_closing(const void *page, size_t size)
{
    log_printf("here\n");
    closed = true;
}

CHECK_FUNC void
when_final(void)
{
    assert(closed);
}

int
main()
{
    register_close_callback(when_closing);
    register_final_callback(when_final);
    return 0;
}
