#include "trace_checker.h"

void
register_expected_trace(uint64_t tid, struct expected_entry *trace)
{
    abort();
}

void
register_entry_callback(void (*callback)(const void *entry))
{
    abort();
}

void
register_close_callback(void (*callback)(const void *page, size_t size))
{
    abort();
}

void
register_final_callback(void (*callback)(void))
{
    abort();
}
