# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: MIT

# ------------------------------------------------------------------------------
# Commands and variables for rendertri
# ------------------------------------------------------------------------------
ROOT!=		pwd
RUNCMD= 	./ri1.0 bunny.obj 20
PARSE=		cat $(ROOT)/$*.run.log \
		| grep Time | tr -s ' ' \
		| cut -d' ' -f6,9 \
		| tr ' ' '\;' \
		| xargs -n1 echo "$*"';' \
		| tee -a $(ROOT)/results.csv

# ------------------------------------------------------------------------------
# clone level, apply patches, build rendertri
# ------------------------------------------------------------------------------

URL=		https://github.com/felipetavares/rendertri.git

TARGET+=	clone
DIR.clone=	rendertri
CFG.clone=	git clone $(URL) .

TARGET+=	patch
DEP.patch=	.clone.bld
DIR.patch=	rendertri
CFG.patch=	git checkout .
BLD.patch=	git apply $(ROOT)/patches/0001-add-parameter.patch && \
		git apply $(ROOT)/patches/0002-enable-sanitizer.patch

TARGET+=	rendertri
DEP.rendertri=	.patch.bld
DIR.rendertri=	rendertri
BLD.rendertri=	$(MAKE)

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------

TARGET+=	tsan
DEP.tsan=	.rendertri.bld
DIR.tsan=	rendertri
RUN.tsan=	hyperfine 'env TSAN_OPTIONS=report_bugs=0 $(RUNCMD)'
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DEP.tsano=	.rendertri.bld
DIR.tsano=	rendertri
RUN.tsano=	hyperfine 'env BINGO_PREFIX=$(ROOT)/../build/deps/bingo \
		$(ROOT)/../deps/bingo/scripts/tsano $(RUNCMD)'
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DEP.coldtrace=	.rendertri.bld
DIR.coldtrace=	rendertri
RUN.coldtrace=	hyperfine 'env COLDTRACE_PATH=/tmp \
		$(ROOT)/../scripts/coldtracer $(RUNCMD)'
PRO.coldtrace=	$(PARSE)

# file to build internal tool (freezer)
# assumes libfreeze.so to be in the current directory
TARGET+=	freezer
DEP.freezer=	.rendertri.bld
DIR.freezer=	rendertri
RUN.freezer=	FREEZER_TRACE_PATH=/tmp hyperfine \
		'env LD_PRELOAD=$(ROOT)/libfreeze.so $(RUNCMD)'
PRO.freezer=	$(PARSE)

# ------------------------------------------------------------------------------
include bench.mk
