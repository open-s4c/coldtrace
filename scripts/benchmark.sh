#!/bin/sh
# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: MIT
#
set -e

MAKE=make

OPTION_CLEAN=no
OPTION_CONFIG=no
OPTION_BUILD=no

OPTION_FORCE_RUN=no
OPTION_FORCE_SUM=no

OPTION_RUN_LEVELDB=yes
OPTION_SUM_LEVELDB=yes

OPTION_RUN_RENDERTRI=no
OPTION_SUM_RENDERTRI=no

OPTION_RUN_RAYTRACING=yes
OPTION_SUM_RAYTRACING=yes

OPTION_RUN_SCRATCHAPIXEL=yes
OPTION_SUM_SCRATCHAPIXEL=yes

OPTION_PULL_WIKI=no
OPTION_MVTO_WIKI=no
WIKI_URL="ssh://git@github.com/open-s4c/coldtrace.wiki.git"
WIKI_DIR="coldtrace.wiki"

enabled() {
    if [ -z "$1" ] || [ "$1" != "yes" ]
    then return 1
    else return 0
    fi
}

# CONFIG AND BUILD COLDTRACE

if enabled $OPTION_CLEAN; then
    echo rm -rf build
fi

if enabled $OPTION_CONFIG; then
    cmake -S . -Bbuild -DCMAKE_BUILD_TYPE=Release
fi

if enabled $OPTION_BUILD; then
    cmake --build build
fi

# RUN BENCHMARKS
F=
if enabled $OPTION_FORCE_RUN; then
    F="FORCE=1"
fi

if enabled $OPTION_RUN_RENDERTRI; then
    echo Running rendertri
    $MAKE -sC bench/rendertri run $F
fi

if enabled $OPTION_RUN_LEVELDB; then
    echo Running leveldb
    $MAKE -sC bench/leveldb run $F
fi

if enabled $OPTION_RUN_RAYTRACING; then
    echo Running raytracing
    $MAKE -sC bench/raytracing run $F
fi

if enabled $OPTION_RUN_SCRATCHAPIXEL; then
    echo Running scratchapixel
    $MAKE -sC bench/scratchapixel run $F
fi

# SUMMARY

DATE=$(date "+%Y-%m-%d")
HOST=$(hostname -s)
SUMMARY=bench-$HOST-$DATE.md
F=
if enabled $OPTION_FORCE_SUM; then
    F="FORCE=1"
fi

rm -f $SUMMARY

output() {
    echo "$@" >> $SUMMARY
}

output "## Environment"
output
output "- Host: $HOST"
output "- Date: $DATE"
output "- Tag:  $(git rev-parse --short HEAD) ($(git branch --show-current))"
output
output '```'
output "$(uname -a | fmt -w 70)"
output '```'

if enabled $OPTION_SUM_RENDERTRI; then
    $MAKE -sC bench/rendertri process $F

    output
    output "## Rendertri"
    cat bench/rendertri/work/results.csv | mlr --icsv --omd cat >> $SUMMARY
fi

if enabled $OPTION_SUM_LEVELDB; then
    $MAKE -sC bench/leveldb process $F

    output
    output "## LevelDB"
    cat bench/leveldb/work/results.csv \
    | mlr --icsv --ifs=\; --omd cat >> $SUMMARY
fi

if enabled $OPTION_SUM_RAYTRACING; then
    $MAKE -sC bench/raytracing process $F

    output
    output "## Raytracing"
    cat bench/raytracing/work/results.csv \
    | mlr --icsv --ifs=\; --omd cat >> $SUMMARY
fi

if enabled $OPTION_SUM_SCRATCHAPIXEL; then
    $MAKE -sC bench/scratchapixel process $F

    output
    output "## Stratchapixel"
    cat bench/scratchapixel/work/results.csv \
    | mlr --icsv --ifs=\; --omd cat >> $SUMMARY
fi

# PULL WIKI

if enabled $OPTION_PULL_WIKI; then
    if [ ! -d $WIKI_DIR ]; then
        git clone $WIKI_URL $WIKI_DIR
    fi

    if [ -d $WIKI_DIR ]; then
        (cd $WIKI_DIR && git pull)
    fi
fi

# MOVE REPORT TO WIKI

if enabled $OPTION_MVTO_WIKI; then
    mv $SUMMARY $WIKI_DIR/bench
fi

