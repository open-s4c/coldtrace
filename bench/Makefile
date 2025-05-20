# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: MIT

# ------------------------------------------------------------------------------
# Commands and variables for all LevelDB benchmark variants
# ------------------------------------------------------------------------------
ROOT!=		pwd
RUNCMD= 	time ./ri1.0 bunny.obj 20 > /dev/null
PARSE=		cat $(ROOT)/$*.run.log \
		| xargs -n1 echo "$*"';' \
		| tee -a $(ROOT)/results.csv

# ------------------------------------------------------------------------------
# clone level, apply patches, build bingo
# ------------------------------------------------------------------------------

URL=		https://github.com/felipetavares/rendertri.git

TARGET+=	clone
DIR.clone=	rendertri
CFG.clone=	git clone $(URL) .
#BLD.clone=	git checkout $(SHA) && git submodule update --init --recursive

TARGET+=	patch
DEP.patch=	.clone.bld
DIR.patch=	rendertri
CFG.patch=	git checkout .
BLD.patch=	git apply $(ROOT)/patches/0001-add-parameter.patch && \
		git apply $(ROOT)/patches/0002-enable-sanitizer.patch

TARGET+=	rendertri
DEP.rendertri=	.patch.bld
BLD.rendertri=	$(MAKE)

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------

#TARGET+=	baseline
DEP.baseline=	.lib.bld .patch.bld
DIR.baseline=	build/leveldb
CFG.baseline= 	$(CONFIGURE)
BLD.baseline=	$(BUILD)
PRE.baseline= 	$(PREPARE)
RUN.baseline= 	$(RUNCMD)
PRO.baseline=	$(PARSE)

TARGET+=	tsan
DEP.tsan=	.rendertri.bld
DIR.tsan=	rendertri
RUN.tsan=	env TSAN_OPTIONS=report_bugs=0 $(RUNCMD)
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DEP.tsano=	.rendertri.bld
DIR.tsano=	rendertri
RUN.tsano=	env BINGO_PREFIX=$(ROOT)/../build/deps/bingo \
		$(ROOT)/../deps/bingo/scripts/tsano $(RUNCMD)
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DEP.coldtrace=	.rendertri.bld
DIR.coldtrace=	rendertri
RUN.coldtrace=	COLDTRACE_PATH=/tmp $(ROOT)/../scripts/coldtracer $(RUNCMD)
PRO.coldtrace=	$(PARSE)

# ------------------------------------------------------------------------------
include bench.mk
