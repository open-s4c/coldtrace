# Getting started fast

Clone and build coldtrace:

    git clone ssh://git@github.com/open-s4c/coldtrace.git
    cd coldtrace
    git submodule update --init --recursive
    cmake -S . -Bbuild -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Build silly example of a write-write race:

    gcc examples/write-write-race.c -fsanitize=thread

Try that with tsan:

    ./a.out

Run that with coldtrace:

    COLDTRACE_PATH=traces scripts/coldtrace ./a.out

See the log output (optional):

    scripts/trace_dump.py traces/freezer_log_1_0.bin

`trace_dump.py` accepts a file named `freezer_log_<tid>_<fragment>.bin` and
dumps every fragment for that thread in numeric fragment order. The selected
fragment does not have to be the first one. Stack state is reconstructed across
fragment boundaries.

Use `-d` or `--debug` to print file, version-header, and decoder diagnostics to
standard error while keeping the decoded events on standard output:

    scripts/trace_dump.py --debug traces/freezer_log_1_0.bin

The trace format contains an 8-byte version header followed by variable-sized
entries using the current little-endian, 64-bit Coldtrace layouts. Trace files
are preallocated, and an all-zero tail marks the end of their entries. Invalid
filenames, truncated entries, unknown entry types, and inconsistent stack diffs
are reported with a nonzero exit status and file offset.

Fragment numbers are normally chronological. If `COLDTRACE_MAX_FILES` causes
the writer to wrap and overwrite fragments, the retained files do not contain
enough generation metadata to recover their original chronology or missing
stack state.

Run freezer analysis (optional):

    freezer -f traces

## Experimental generated Dice runtime

The `deps/dice` submodule uses [db7/dice](https://github.com/db7/dice), with
`dice-roll-improvements` configured as its update branch. A normal submodule
update checks out the exact commit recorded by Coldtrace. To advance it to the
latest commit on that branch, run `git submodule update --remote deps/dice`
and commit the updated submodule reference.

Builds use `deps/dice` by default. It supplies `dice-roll` namespace imports,
`add_dice_core`, `add_dice_mods`, and the definitions in `src/mod/*.dice`.
Set `DICE_SOURCE` to use another checkout; for an existing build directory,
set `-DDICE_SOURCE="$PWD/deps/dice"` to replace a cached sibling path.

`coldtrace.dice` is the application configuration, maintained at the repository
root. It imports Dice's reusable interceptor definitions, declares every
Coldtrace slot inside `(module coldtrace ...)`, and selects dispatch priorities.
The settings `(callbacks false)` and `(mempool-size 200Mi)` are top-level forms.
The binary size suffix means 200 * 1024 * 1024 bytes.

CMake generates only `platform.dice` in the build directory, imported as
`(import (coldtrace platform))`. On non-Apple platforms it imports
`pthread_spinlock` and adds `(after (dice pthread_spinlock) (coldtrace marker))`.
These standalone ordering constraints preserve initialization order without
redeclaring the slots. The generated directory is mounted under `coldtrace`;
no source-directory mapping or individual subscriber `.dice` files are needed.

Each Coldtrace subscriber has a named slot and explicit consumption routes.
For example, the `procmaps` slot in the `coldtrace` module follows `(dice self)`.
The generator assigns its number and emits `SLOT_COLDTRACE_PROCMAPS`.
CMake compiles `procmaps.c` with
`DICE_MODULE_SLOT=SLOT_COLDTRACE_PROCMAPS` and force-includes the generated
header. Its `PS_SUBSCRIBE` and `DICE_MODULE_INIT` declarations therefore use
that slot without knowing the numeric value. Other subscriber sources use the
same convention. Dice's own modules retain names such as `SLOT_DICE_SELF`.

Ordering constraints replace `COLDTRACE_BASE_SLOT` and numeric slot arithmetic.
`EVENT_TEST_MARKER` is allocated by the generator. Importing a definition does
not add its C implementation: CMake still selects and compiles module sources.

The `prioritize` form in `coldtrace.dice` selects chain-first dispatch for
`CHAIN_INTERCEPT_EVENT` and `CHAIN_CAPTURE_EVENT`, then `EVENT_MA_READ` and `EVENT_MA_WRITE`.
This is the policy that performed best in the Release/LTO LevelDB comparison.
Other routes use Dice's shared fallback, with inlining left to the compiler.
Tune this form for another workload; priorities do not change handler order.

`coldtrace-runtime` is an object target containing the generated dispatcher,
Dice core sources, and the selected interceptor sources. Both `coldtrace` and
`trace_checker` reuse it. Subscriber object targets consume `coldtrace-dice`,
which propagates the generated-header dependency and forces that header before
ordinary Dice headers. Utility objects shared with instrumented applications
keep ordinary header settings. The generator embeds no core or module sources.
The top-level `(callbacks false)` setting disables extension callbacks and is
inherited by every declared slot. All consumers are linked, matching the legacy
`dice-box` build. The `self-box` and `mempool-box` wrappers keep Dice helper
calls private so LTO can optimize them without dynamic symbol lookup.

To check declared routes as well as build and run tests:

    cmake -S . -B build-roll -DCMAKE_BUILD_TYPE=Release \
        -DCOLDTRACE_CHECK_ROUTES=ON
    cmake --build build-roll -j8
    ctest --test-dir build-roll --output-on-failure

The generated files are in `build-roll/src/coldtrace-runtime-dice/`.
CTest supplies the selected build paths to the preload scripts. For manual use:

    COLDTRACE_BUILD="$PWD/build-roll" \
        COLDTRACE_PATH=/tmp/coldtrace-traces scripts/coldtrace ./program

On macOS, capture starts after Coldtrace's configuration constructor, because
interposition can begin during libSystem's allocator initialization. The Linux
`/proc` collector is disabled there. Instrumented test applications use flat
symbol lookup so sanitizer calls reach the preloaded runtime. Tests retain
unoptimized calls because their expectations describe individual operations.

Validated on macOS arm64 with AppleClang 21: normal and route-checked
Release/LTO builds succeed. The latest full runs passed 45 of 51 tests, with
these six failures also present before the namespace migration: `trace_atomic`,
`trace_hb_check`, `trace_hb_check_2`, `trace_memcpy`, `trace_pthread_cond_wait`,
and `trace_read_write_size`. No undeclared-route errors were reported.
The trace expectations and timing-sensitive atomic checks still need
investigation; the complete suite is not green.

After consolidating the configuration, allocation/free, thread create/join,
and main-thread-close smoke tests pass. Generated C and headers are identical
to the previous configuration, with and without the optional spinlock slot.
The spinlock comparison validates generation only; Linux and other platforms
have not been built or tested for this integration.
