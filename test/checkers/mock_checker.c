#include "trace_checker.h"

void
register_expected_trace(uint64_t tid, struct expected_entry *trace)
{
    abort();
}

void
register_entry_callback(void (*callback)(const void *entry, metadata_t *md))
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

thread_id
self_id(struct metadata *self)
{
    abort();
}

void *
self_tls(struct metadata *self, const void *global, size_t size)
{
    abort();
}

struct metadata *
self_md(void)
{
    abort();
};
