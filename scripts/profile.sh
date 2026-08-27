#!/bin/sh
# Copyright (C) 2026 Huawei Technologies Co., Ltd.
# SPDX-License-Identifier: MIT
#
# Profile benchmark variants on Linux with `perf record`, producing one
# perf.data per (benchmark, variant).
#
set -eu

MAKE=make
CUR_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$CUR_DIR/.." && pwd)"
BENCH_DIR="$PROJECT_ROOT/bench"
RESULTS_DIR="$PROJECT_ROOT/results"
BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$RESULTS_DIR"

# Debug-symbol build so perf can symbolise and unwind call graphs.
BUILD_TYPE="RelWithDebInfo"
PROFILE_FLAGS="-g -fno-omit-frame-pointer"

# perf attributes
FREQ="999"
CALL_GRAPH="dwarf"
PERF_EVENT=""

OPTION_CLEAN="no"

SELECTED_BENCHMARKS=""
SELECTED_VARIANTS=""
ALL_BENCHMARKS="leveldb:; raytracing:; scratchapixel:;"
ALL_VARIANTS="baseline tsan tsano coldtrace nowrites"

# --- CLI ARGUMENT PARSING ---
usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -b, --benchmark <name>   Select benchmark(s): leveldb, raytracing, scratchapixel"
    echo "                           Repeatable and/or comma-separated"
    echo "  -v, --variant <name>     Select variant(s): baseline, tsan, tsano, coldtrace, nowrites"
    echo "                           Repeatable and/or comma-separated"
    echo "  --freq <hz>              perf sampling frequency (default $FREQ)"
    echo "  --call-graph <mode>      Call-graph unwind: dwarf or fp (default $CALL_GRAPH)"
    echo "  -e, --event <event>      perf event to sample (default: perf's own default)"
    echo "  --clean                  Force a clean rebuild of project and benchmarks"
    echo
    echo "Builds the coldtrace project with $BUILD_TYPE, then the selected"
    echo "benchmarks with debug info, then records perf.data per variant into"
    echo "results/profile-<timestamp>/."
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --clean)        OPTION_CLEAN="yes"; shift ;;
        --freq)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            FREQ="$2"; shift 2 ;;
        --call-graph)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            CALL_GRAPH="$2"; shift 2 ;;
        -e|--event)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            PERF_EVENT="-e $2"; shift 2 ;;
        -b|--benchmark|--benchmarks)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            SELECTED_BENCHMARKS="$SELECTED_BENCHMARKS $(echo "$2" | tr ',' ' ') "
            shift 2 ;;
        -v|--variant|--variants)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            SELECTED_VARIANTS="$SELECTED_VARIANTS $(echo "$2" | tr ',' ' ') "
            shift 2 ;;
        -h|--help)      usage ;;
        *)              echo "Unknown option: $1"; usage ;;
    esac
done

VARIANTS=$(echo $SELECTED_VARIANTS)
COLLECT_VARIANTS=${VARIANTS:-$ALL_VARIANTS}

# Check whether a benchmark should be processed
process_benchmark() {
    target_name="$1"
    if [ -z "$(echo $SELECTED_BENCHMARKS)" ]; then
        return 0
    fi
    case " $SELECTED_BENCHMARKS " in
        *" $target_name "*) return 0 ;;
        *) return 1 ;;
    esac
}

# --- perf sanity checks ---
if ! command -v perf >/dev/null 2>&1; then
    echo "Error: 'perf' not found. Install it (e.g. linux-perf / linux-tools-\$(uname -r))."
    exit 1
fi
if [ -r /proc/sys/kernel/perf_event_paranoid ]; then
    paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)
    if [ "$paranoid" -gt 1 ]; then
        echo "Warning: kernel.perf_event_paranoid=$paranoid may block 'perf record'."
        echo "         If recording fails: sudo sysctl kernel.perf_event_paranoid=1"
    fi
fi

# --- Build the coldtrace project with debug info ---
echo "=== Building project ($BUILD_TYPE) ==="
[ "$OPTION_CLEAN" = "yes" ] && rm -rf "$BUILD_DIR"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --parallel

# --- Build benchmarks with debug info ---
ensure_profile_build() {
    _name="$1"
    _dir="$BENCH_DIR/$_name"
    _marker="$_dir/work/.build-type"
    _have=""
    [ -f "$_marker" ] && _have=$(cat "$_marker" 2>/dev/null || echo "")

    if [ "$OPTION_CLEAN" = "yes" ] || [ "$_have" != "$BUILD_TYPE" ]; then
        echo "--> Cleaning for $BUILD_TYPE rebuild: $_name (was: ${_have:-none})"
        $MAKE -sC "$_dir" clean
    fi

    echo "--> Building benchmark ($BUILD_TYPE): $_name"
    $MAKE -sC "$_dir" build BUILD_TYPE="$BUILD_TYPE" PROFILE_FLAGS="$PROFILE_FLAGS"
    mkdir -p "$_dir/work"
    echo "$BUILD_TYPE" > "$_dir/work/.build-type"
}

# --- Profile ---
DATE=$(date "+%Y-%m-%d %H:%M:%S")
FILE_DATE=$(echo "$DATE" | tr ' ' '_' | tr ':' '-')
OUTDIR="$RESULTS_DIR/profile-$FILE_DATE"

for bench in $ALL_BENCHMARKS; do
    name=$(echo "$bench" | cut -d':' -f1)
    process_benchmark "$name" || continue

    ensure_profile_build "$name"

    for variant in $COLLECT_VARIANTS; do
        rm -f "$BENCH_DIR/$name/work/.$variant.perf.data"
    done

    echo "--> Profiling: $name${VARIANTS:+ (variants: $VARIANTS)}"
    if [ -n "$VARIANTS" ]; then
        $MAKE -sC "$BENCH_DIR/$name" run TARGET="$VARIANTS" PROFILE=1 \
            FREQ="$FREQ" CALL_GRAPH="$CALL_GRAPH" PERF_EVENT="$PERF_EVENT" FORCE=1
    else
        $MAKE -sC "$BENCH_DIR/$name" run PROFILE=1 \
            FREQ="$FREQ" CALL_GRAPH="$CALL_GRAPH" PERF_EVENT="$PERF_EVENT" FORCE=1
    fi
done

# --- Collect perf.data ---
mkdir -p "$OUTDIR"
for bench in $ALL_BENCHMARKS; do
    name=$(echo "$bench" | cut -d':' -f1)
    process_benchmark "$name" || continue

    work="$BENCH_DIR/$name/work"
    [ -d "$work" ] || continue

    for variant in $COLLECT_VARIANTS; do
        f="$work/.$variant.perf.data"
        [ -f "$f" ] || continue
        cp "$f" "$OUTDIR/${name}_${variant}.perf.data"
        echo "  collected ${name}_${variant}.perf.data"
    done
done

echo "Profiling complete! perf.data file(s) saved to: $OUTDIR"
