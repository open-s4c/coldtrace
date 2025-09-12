# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: 0BSD

PROJECT!=	readlink -f $(ROOTDIR)/../..
CXX_FLAGS+=	-I/usr/X11R7/include -L/usr/X11R7/lib
RUNCMD= 	./ri1.0 bunny.obj 20

# ------------------------------------------------------------------------------
# clone level, apply patches, build rendertri
# ------------------------------------------------------------------------------

URL=		https://github.com/felipetavares/rendertri.git

TARGET+=	clone
DIR.clone=	rendertri
CFG.clone=	git clone $(URL) . && \
		git checkout . && \
		git apply $(ROOTDIR)/patches/0001-add-parameter.patch && \
		git apply $(ROOTDIR)/patches/0002-enable-sanitizer.patch

TARGET+=	rendertri
DIR.rendertri=	rendertri
BLD.rendertri=	$(MAKE) CXX_FLAGS='$(CXX_FLAGS) $(CXXFLAGS_EXTRA) $(LDFLAGS)'

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------
TARGET+=	header

TARGET+=	tsan
DIR.tsan=	rendertri
RUN.tsan=	$(TESTER) 'env TSAN_OPTIONS=report_bugs=0 $(RUNCMD)'
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DIR.tsano=	rendertri
RUN.tsano=	$(TESTER) 'env TSANO_LIBDIR=$(TSANO_LIBDIR) \
			$(TSANO_CMD) $(RUNCMD)'
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DIR.coldtrace=	rendertri
RUN.coldtrace=	$(TESTER) 'env $(COLDTRACE_CMD) $(RUNCMD)'
PRO.coldtrace=	$(PARSE)

TARGET+=	nowrites
DIR.nowrites=	rendertri
RUN.nowrites=	$(TESTER) 'env COLDTRACE_DISABLE_WRITES=true \
			$(COLDTRACE_CMD) $(RUNCMD)'
PRO.nowrites=	$(PARSE)

# file to build internal tool (freezer)
# assumes libfreeze.so to be in the current directory
#TARGET+=	freezer
DIR.freezer=	rendertri
RUN.freezer=	FREEZER_TRACE_PATH=traces $(TESTER) \
		'env LD_PRELOAD=$(ROOTDIR)/libfreeze.so $(RUNCMD)'
PRO.freezer=	$(PARSE)

# ------------------------------------------------------------------------------
BENCHMK= ../bench.mk
include $(BENCHMK)
include $(ROOTDIR)/../common.mk
