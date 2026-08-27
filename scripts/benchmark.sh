#!/bin/sh
# Copyright (C) 2025 Huawei Technologies Co., Ltd.
# SPDX-License-Identifier: MIT
#
set -eu

MAKE=make
CUR_DIR="$(cd "$(dirname "$0")" && pwd)"
BENCH_DIR="$CUR_DIR/../bench"
RESULTS_DIR="$CUR_DIR/../results"
mkdir -p "$RESULTS_DIR"

OPTION_CLEAN="no"
OPTION_NO_HYPERFINE="no"
BUILD_TYPE="Release"

SELECTED_BENCHMARKS=""
SELECTED_VARIANTS=""
ALL_BENCHMARKS="leveldb:; raytracing:; scratchapixel:;"
KNOWN_BENCHMARKS="leveldb raytracing scratchapixel"
KNOWN_VARIANTS="baseline tsan tsano coldtrace nowrites"

# --- CLI ARGUMENT PARSING ---
usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -b, --benchmark <name>   Select benchmark(s): leveldb, raytracing, scratchapixel"
    echo "                           Repeatable and/or comma-separated (e.g. -b leveldb,scratchapixel)"
    echo "  -v, --variant <name>     Select variant(s): baseline, tsan, tsano, coldtrace, nowrites"
    echo "                           Repeatable and/or comma-separated (e.g. -v baseline,coldtrace)"
    echo "  --clean                  Remove the selected benchmarks' work/ (build) directories"
    echo "  --no-hyperfine           Run standalone without hyperfine"
    echo
    echo "Note: If no benchmark is selected, ALL benchmarks are run and summarized."
    echo "      If no variant is selected, ALL variants of each benchmark are run."
    exit 1
}

validate() {
    _kind="$1"; _value="$2"; _known="$3"
    case " $_known " in
        *" $_value "*) return 0 ;;
        *)
            echo "Error: unknown $_kind '$_value' (valid: $_known)"
            usage ;;
    esac
}

# Parse flags
while [ $# -gt 0 ]; do
    case "$1" in
        --clean)        OPTION_CLEAN="yes"; shift ;;
        --no-hyperfine) OPTION_NO_HYPERFINE="yes"; shift ;;
        -b|--benchmark|--benchmarks)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            for _b in $(echo "$2" | tr ',' ' '); do
                validate benchmark "$_b" "$KNOWN_BENCHMARKS"
                SELECTED_BENCHMARKS="$SELECTED_BENCHMARKS $_b "
            done
            shift 2 ;;
        -v|--variant|--variants)
            [ $# -ge 2 ] || { echo "Error: $1 requires an argument"; usage; }
            for _v in $(echo "$2" | tr ',' ' '); do
                validate variant "$_v" "$KNOWN_VARIANTS"
                SELECTED_VARIANTS="$SELECTED_VARIANTS $_v "
            done
            shift 2 ;;
        -h|--help)      usage ;;
        *)              echo "Unknown option: $1"; usage ;;
    esac
done

VARIANTS=$(echo $SELECTED_VARIANTS)
NO_HF_FLAG=""
[ "$OPTION_NO_HYPERFINE" = "yes" ] && NO_HF_FLAG="NO_HYPERFINE=1"

# Check if a benchmark is meant to be processed
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

bench_make() {
    _dir="$1"; _action="$2"; _target="$3"; shift 3
    if [ -n "$_target" ]; then
        $MAKE -sC "$_dir" "$_action" TARGET="$_target" "$@"
    else
        $MAKE -sC "$_dir" "$_action" "$@"
    fi
}

# --- CLEAN (benchmark work/ dirs) ---
if [ "$OPTION_CLEAN" = "yes" ]; then
    for bench in $ALL_BENCHMARKS; do
        name=$(echo "$bench" | cut -d':' -f1)
        if process_benchmark "$name"; then
            echo "--> Cleaning work directory: $name"
            $MAKE -sC "$BENCH_DIR/$name" clean
        fi
    done
fi

# --- RUN BENCHMARKS ---
for bench in $ALL_BENCHMARKS; do
    name=$(echo "$bench" | cut -d':' -f1)

    if process_benchmark "$name"; then

        _have=""
        [ -f "$BENCH_DIR/$name/work/.build-type" ] && \
            _have=$(cat "$BENCH_DIR/$name/work/.build-type" 2>/dev/null || echo "")
        if [ -n "$_have" ] && [ "$_have" != "$BUILD_TYPE" ]; then
            echo "--> Previous build was $_have; cleaning for $BUILD_TYPE: $name"
            $MAKE -sC "$BENCH_DIR/$name" clean
        fi

        echo "--> Building benchmark: $name"
        $MAKE -sC "$BENCH_DIR/$name" build $NO_HF_FLAG
        mkdir -p "$BENCH_DIR/$name/work"
        echo "$BUILD_TYPE" > "$BENCH_DIR/$name/work/.build-type"

        echo "--> Running benchmark: $name${VARIANTS:+ (variants: $VARIANTS)}"
        bench_make "$BENCH_DIR/$name" run "$VARIANTS" FORCE=1 $NO_HF_FLAG
    fi
done

# --- GENERATE SUMMARY ---
DATE=$(date "+%Y-%m-%d %H:%M:%S")
BRANCH=$(git branch --show-current 2>/dev/null || echo "")
FILE_DATE=$(echo "$DATE" | tr ' ' '_' | tr ':' '-')
SUMMARY="$RESULTS_DIR/bench-$FILE_DATE.md"

: > "$SUMMARY"

echo "--> Generating Environment Metadata"
{
    echo "## Environment"
    echo
    echo "- OS:  $(uname -srm)"
    echo "- Tag: $(git rev-parse --short HEAD 2>/dev/null || echo 'N/A') (${BRANCH:-detached})"
    [ "$OPTION_NO_HYPERFINE" = "yes" ] && echo "- Run: standalone (no hyperfine)"
} >> "$SUMMARY"

# --- PROCESS & APPEND BENCHMARK RESULTS ---
for bench in $ALL_BENCHMARKS; do
    name=$(echo "$bench" | cut -d':' -f1)
    sep=$(echo "$bench" | cut -d':' -f2)

    if process_benchmark "$name"; then
        echo "--> Processing summary for: $name"

        if [ -n "$VARIANTS" ]; then
            PROC_TARGET="header $VARIANTS"
        else
            PROC_TARGET=""
        fi
        bench_make "$BENCH_DIR/$name" process "$PROC_TARGET" FORCE=1 $NO_HF_FLAG

        {
            echo
            first_char=$(echo "$name" | cut -c1 | tr '[:lower:]' '[:upper:]')
            rest_chars=$(echo "$name" | cut -c2-)
            echo "## ${first_char}${rest_chars}"

            CSV_FILE="$BENCH_DIR/$name/work/results.csv"
            if [ -f "$CSV_FILE" ]; then
                mlr --icsv --ifs "$sep" --omd cat "$CSV_FILE"
            else
                echo "Warning: Results file missing for $name"
            fi
        } >> "$SUMMARY"
    fi
done

echo "Benchmark execution complete! Report written to: $SUMMARY"
