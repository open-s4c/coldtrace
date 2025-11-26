#include <stdio.h>

// Define the list of memory order suffixes.
#define MEMORY_ORDERS(V)                                                       \
    V(RELAXED)                                                                 \
    V(CONSUME)                                                                 \
    V(ACQUIRE)                                                                 \
    V(RELEASE)                                                                 \
    V(ACQ_REL)                                                                 \
    V(SEQ_CST)

// Define the list of atomic operations.
#define ATOMIC_OPERATIONS(V)                                                   \
    V(LOAD)                                                                    \
    V(STORE)                                                                   \
    V(EXCHANGE)                                                                \
    V(COMPARE_EXCHANGE_STRONG)                                                 \
    V(COMPARE_EXCHANGE_WEAK)                                                   \
    V(FETCH_ADD)                                                               \
    V(FETCH_SUB)                                                               \
    V(FETCH_AND)                                                               \
    V(FETCH_OR)                                                                \
    V(FETCH_XOR)

// Helper macros to create combined enum names.
#define EVENT_TYPE(op, order) EVENT_##op##_##order

// For each operation, generate an entry for every memory ordering.
#define EVENT_ENTRY(op) MEMORY_ORDERS(EVENT_ENTRY_HELPER, op)

#define EVENT_ENTRY_HELPER(order, op) EVENT_TYPE(op, order),

// Since the C preprocessor does not support nested macro iteration directly,
// we redefine MEMORY_ORDERS for use in EVENT_ENTRY as follows:
#undef MEMORY_ORDERS
#define MEMORY_ORDERS(V, op)                                                   \
    V(RELAXED, op)                                                             \
    V(CONSUME, op)                                                             \
    V(ACQUIRE, op)                                                             \
    V(RELEASE, op)                                                             \
    V(ACQ_REL, op)                                                             \
    V(SEQ_CST, op)

// Now we can define our enum by iterating over operations and, for each one,
// iterating over the memory orders.
typedef enum {
#define MAKE_EVENT(op) MEMORY_ORDERS(EVENT_ENTRY_HELPER, op)
    ATOMIC_OPERATIONS(MAKE_EVENT)
#undef MAKE_EVENT
        EVENT_COUNT // Optionally, add a count or invalid event marker.
} event_type;

// Optional helper to convert an event_type to a string.
static inline const char *
event_type_to_string(event_type evt)
{
    switch (evt) {
#define EVENT_CASE(order, op)                                                  \
    case EVENT_TYPE(op, order):                                                \
        return #op "_" #order;
        // Again, iterate over each operation and each memory ordering.
#define MAKE_EVENT_CASE(op) MEMORY_ORDERS(EVENT_CASE, op)
        ATOMIC_OPERATIONS(MAKE_EVENT_CASE)
#undef MAKE_EVENT_CASE
#undef EVENT_CASE
        default:
            return "UNKNOWN_EVENT";
    }
}

// Example usage:
int
main(void)
{
    event_type evt = EVENT_COMPARE_EXCHANGE_WEAK_SEQ_CST;
    printf("Event: %s\n", event_type_to_string(evt));
    return 0;
}
