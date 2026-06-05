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
OPTION_CONFIG="no"
OPTION_BUILD="no"
OPTION_FORCE_RUN="no"
OPTION_FORCE_SUM="no"

SELECTED_BENCHMARKS=""
ALL_BENCHMARKS="leveldb:; raytracing:; scratchapixel:;"

# --- CLI ARGUMENT PARSING ---
usage() {
    echo "Usage: $0 [options] [benchmark_names...]"
    echo "Options:"
    echo "  --clean      Remove build directory"
    echo "  --config     Run CMake configuration"
    echo "  --build      Build the project"
    echo "  --force-run  Force benchmark execution"
    echo "  --force-sum  Force summary generation"
    echo "Available benchmarks: leveldb, raytracing, scratchapixel"
    echo "Note: If no benchmarks are specified, ALL will be run and summarized."
    exit 1
}

# Parse flags
while [ $# -gt 0 ]; do
    case "$1" in
        --clean)     OPTION_CLEAN="yes"; shift ;;
        --config)    OPTION_CONFIG="yes"; shift ;;
        --build)     OPTION_BUILD="yes"; shift ;;
        --force-run) OPTION_FORCE_RUN="yes"; shift ;;
        --force-sum) OPTION_FORCE_SUM="yes"; shift ;;
        -h|--help)   usage ;;
        -*)          echo "Unknown option: $1"; usage ;;
        *)           SELECTED_BENCHMARKS="$SELECTED_BENCHMARKS $1 "; shift ;;
    esac
done

# Check if a benchmark is meant to be processed
process_benchmark() {
    local target_name="$1"
    
    if [ -z "$SELECTED_BENCHMARKS" ]; then
        return 0
    fi
    
    case " $SELECTED_BENCHMARKS " in
        *" $target_name "*) return 0 ;;
        *) return 1 ;;
    esac
}

# --- PREPARATION & BUILD ---
if [ "$OPTION_CLEAN" = "yes" ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

if [ "$OPTION_CONFIG" = "yes" ]; then
    cmake -S . -Bbuild -DCMAKE_BUILD_TYPE=Release
fi

if [ "$OPTION_BUILD" = "yes" ]; then
    cmake --build build
fi

# --- RUN BENCHMARKS ---
RUN_FORCE_FLAG=""
[ "$OPTION_FORCE_RUN" = "yes" ] && RUN_FORCE_FLAG="FORCE=1"

for bench in $ALL_BENCHMARKS; do
    name=$(echo "$bench" | cut -d':' -f1)
    
    if process_benchmark "$name"; then
        echo "--> Running benchmark: $name"
        $MAKE -sC "$BENCH_DIR/$name" run $RUN_FORCE_FLAG
    fi
done

# --- GENERATE SUMMARY ---
DATE=$(date "+%Y-%m-%d %H:%M:%S")
HOST=$(hostname -s)
FILE_DATE=$(echo "$DATE" | tr ' ' '_' | tr ':' '-') 
SUMMARY="$RESULTS_DIR/bench-$FILE_DATE.md"

SUM_FORCE_FLAG=""
[ "$OPTION_FORCE_SUM" = "yes" ] && SUM_FORCE_FLAG="FORCE=1"

: > "$SUMMARY"

echo "--> Generating Environment Metadata"
{
    echo "## Environment"
    echo
    echo "- Host: $HOST"
    echo "- Date: $DATE"
    echo "- Tag:  $(git rev-parse --short HEAD 2>/dev/null || echo 'N/A') ($(git branch --show-current 2>/dev/null || echo 'detached'))"
    echo
    echo '```'
    uname -a | fmt -w 70
    echo '```'
} >> "$SUMMARY"

# --- PROCESS & APPEND BENCHMARK RESULTS ---
for bench in $ALL_BENCHMARKS; do
    name=$(echo "$bench" | cut -d':' -f1)
    sep=$(echo "$bench" | cut -d':' -f2)
    
    if process_benchmark "$name"; then
        echo "--> Processing summary for: $name"
        $MAKE -sC "$BENCH_DIR/$name" process $SUM_FORCE_FLAG

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
