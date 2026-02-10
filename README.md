# Getting started fast

Clone and build coldtrace:

    git clone ssh://git@github.com/open-s4c/coldtrace.git
    cd coldtrace
    git submodule update --init
    cmake -S . -Bbuild -DCMAKE_BUILD_TYPE=Release
    cmake --build build

GN/Ninja build (core libs + Dice modules):

    gn gen out/gn
    ninja -C out/gn

GN tests (uses the same scripts as CTest):

    gn gen out/gn --args='coldtrace_tests=true'
    ninja -C out/gn run_tests

Re-run all GN tests (ctest-like behavior):

    ./scripts/gn-ctest

You can override the GN output directory via `GN_OUT_DIR`:

    GN_OUT_DIR=out/gn-debug ./scripts/gn-ctest

Optional GN args (edit with `gn args out/gn`):

    enable_lto = true
    dice_use_clang = false
    dice_link_override = false
    dice_dispatch_chains = "1,4,2,3,5,6,0"
    dice_max_types = 128
    dice_max_chains = 16
    dice_max_builtin_slots = 16
    dice_mempool_size = "1024*1024*200"
    dice_log_level = ""
    dice_sanitizer = ""
    dice_coverage = false
    coldtrace_base_slot = 3
    coldtrace_tests = false
    coldtrace_use_tsan = true
    coldtrace_use_gcc = true
    coldtrace_have_pthread_clocklock_clockwait = true

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
