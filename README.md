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

Run freezer analysis (optional):

    freezer -f traces

