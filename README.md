# Getting started fast

Clone and build coldtrace:

    git clone ssh://git@github.com/open-s4c/coldtrace.git
    cd coldtrace
    git submodule update --init
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
