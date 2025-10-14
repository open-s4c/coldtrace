#include "trace_checker.h"

DICE_WEAK
void
register_expected_trace(uint64_t tid, struct expected_entry *trace)
{
    abort();
}
DICE_WEAK
void
register_entry_callback(void (*callback)(const void *entry))
{
}
DICE_WEAK
void
register_close_callback(void (*callback)(const void *page, size_t size))
{
}
DICE_WEAK
void
register_final_callback(void (*callback)(void))
{
}
