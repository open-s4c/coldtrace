# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: MIT

# ------------------------------------------------------------------------------
# Commands and variables for rendertri
# ------------------------------------------------------------------------------
PROJECT!=	readlink -f $(ROOTDIR)/../..
BUILD_DIR=	$(PROJECT)/build
TSANO_CMD=	$(PROJECT)/deps/dice/deps/tsano/tsano
TSANO_LIBDIR=	$(BUILD_DIR)/deps/dice/deps/tsano
COLDTRACE_CMD=	COLDTRACE_PATH=traces \
		COLDTRACE_MAX_FILES=3 \
		COLDTRACE_DISABLE_CLEANUP=true \
		COLDTRACE_DISABLE_COPY=true \
		$(PROJECT)/scripts/coldtracer

CXX_FLAGS+=	-I/usr/X11R7/include -L/usr/X11R7/lib
CXX_FLAGS+=	-fsanitize=thread
CXX_FLAGS+=	-ltsan

TESTER!=	if which hyperfine; \
		then echo "hyperfine"; \
		else echo "sh -c"; fi

RUNCMD= 	./ri1.0 bunny.obj 20
PARSE=		cat $(WORKDIR)/$*.run.log \
		| grep Time | tr -s ' ' \
		| cut -d' ' -f6,9 \
		| tr ' ' '\;' \
		| xargs -n1 echo "$*"';' \
		| tee -a $(WORKDIR)/results.csv

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
BLD.patch=	git apply $(ROOTDIR)/patches/0001-add-parameter.patch && \
		git apply $(ROOTDIR)/patches/0002-enable-sanitizer.patch

TARGET+=	rendertri
DEP.rendertri=	.patch.bld
DIR.rendertri=	rendertri
BLD.rendertri=	$(MAKE) CXX_FLAGS='$(CXX_FLAGS)'

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------

TARGET+=	tsan
DEP.tsan=	.rendertri.bld
DIR.tsan=	rendertri
RUN.tsan=	$(TESTER) 'env TSAN_OPTIONS=report_bugs=0 $(RUNCMD)'
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DEP.tsano=	.rendertri.bld
DIR.tsano=	rendertri
RUN.tsano=	$(TESTER) 'env TSANO_LIBDIR=$(TSANO_LIBDIR) \
			$(TSANO_CMD) $(RUNCMD)'
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DEP.coldtrace=	.rendertri.bld
DIR.coldtrace=	rendertri
RUN.coldtrace=	$(TESTER) 'env $(COLDTRACE_CMD) $(RUNCMD)'
PRO.coldtrace=	$(PARSE)

TARGET+=	nowrites
DEP.nowrites=	.rendertri.bld
DIR.nowrites=	rendertri
RUN.nowrites=	$(TESTER) 'env COLDTRACE_DISABLE_WRITES=true \
			$(COLDTRACE_CMD) $(RUNCMD)'
PRO.nowrites=	$(PARSE)

# file to build internal tool (freezer)
# assumes libfreeze.so to be in the current directory
TARGET+=	freezer
DEP.freezer=	.rendertri.bld
DIR.freezer=	rendertri
RUN.freezer=	FREEZER_TRACE_PATH=traces $(TESTER) \
		'env LD_PRELOAD=$(ROOTDIR)/libfreeze.so $(RUNCMD)'
PRO.freezer=	$(PARSE)

# ------------------------------------------------------------------------------
BENCHMK= ../bench.mk
include $(BENCHMK)
