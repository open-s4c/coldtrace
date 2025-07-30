# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: MIT

RUNCMD= 	./theRestOfYourLife

# ------------------------------------------------------------------------------
# clone level, apply patches, build rendertri
# ------------------------------------------------------------------------------

URL=		https://github.com/RayTracing/raytracing.github.io

TARGET+=	clone
DIR.clone=	raytracing
CFG.clone=	git clone --branch v4.0.1 $(URL) . && \
		git apply $(ROOTDIR)/patches/reduce-samples.patch

TARGET+=	vanilla
DEP.vanilla=	.clone.bld
DIR.vanilla=	vanilla
CFG.vanilla=	cmake ../$(DIR.clone) \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_CXX_COMPILER=$(CXX)
BLD.vanilla=	$(MAKE) -j 8

TARGET+=	sanitized
DEP.sanitized=	.clone.bld
DIR.sanitized=	sanitized
CFG.sanitized=	cmake ../$(DIR.clone) \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_CXX_COMPILER=$(CXX)
			-DCMAKE_CXX_FLAGS=-fsanitize=thread \
			-DCMAKE_EXE_LINKER_FLAGS=$(LDFLAGS)
BLD.sanitized=	$(MAKE) -j 8

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------
TARGET+=	header

TARGET+=	baseline
DEP.baseline=	.vanilla.bld
DIR.baseline=	$(DIR.vanilla)
RUN.baseline=	$(TESTER) '$(RUNCMD)'
PRO.baseline=	$(PARSE)

TARGET+=	tsan
DEP.tsan=	.sanitized.bld
DIR.tsan=	$(DIR.sanitized)
RUN.tsan=	$(TESTER) 'env TSAN_OPTIONS=report_bugs=0 $(RUNCMD)'
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DEP.tsano=	.sanitized.bld
DIR.tsano=	$(DIR.sanitized)
RUN.tsano=	$(TESTER) 'env TSANO_LIBDIR=$(TSANO_LIBDIR) \
			$(TSANO_CMD) $(RUNCMD)'
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DEP.coldtrace=	.sanitized.bld
DIR.coldtrace=	$(DIR.sanitized)
RUN.coldtrace=	$(TESTER) 'env $(COLDTRACE_CMD) $(RUNCMD)'
PRO.coldtrace=	$(PARSE)

TARGET+=	nowrites
DEP.nowrites=	.sanitized.bld
DIR.nowrites=	$(DIR.sanitized)
RUN.nowrites=	$(TESTER) 'env COLDTRACE_DISABLE_WRITES=true \
			$(COLDTRACE_CMD) $(RUNCMD)'
PRO.nowrites=	$(PARSE)

# file to build internal tool (freezer)
# assumes libfreeze.so to be in the current directory
#TARGET+=	freezer
DEP.freezer=	.sanitized.bld
DIR.freezer=	$(DIR.sanitized)
RUN.freezer=	FREEZER_TRACE_PATH=traces $(TESTER) \
		'env LD_PRELOAD=$(ROOTDIR)/libfreeze.so $(RUNCMD)'
PRO.freezer=	$(PARSE)

# ------------------------------------------------------------------------------
BENCHMK= ../bench.mk
include $(BENCHMK)
include $(ROOTDIR)/../common.mk
