# Copyright (C) 2025 Huawei Technologies Co., Ltd.
# SPDX-License-Identifier: MIT

# ------------------------------------------------------------------------------
# Commands and variables for all LevelDB benchmark variants
# ------------------------------------------------------------------------------
DBDIR=		/tmp/bench.db
COMMAND=	./db_bench --db=$(DBDIR)
RUNCMD= 	$(COMMAND) --threads=1 --benchmarks=readrandom --reads=500000
BUNDLE_DIR=	$(PROJECT)/build/bench/lib/

# ------------------------------------------------------------------------------
# clone level, apply patches, build, populate db
# ------------------------------------------------------------------------------

URL=		https://github.com/google/leveldb.git
SHA=		068d5ee1a3ac40dabd00d211d5013af44be55bea

TARGET+=	clone
DIR.clone=	leveldb
CFG.clone=	git clone --recurse-submodules $(URL) . && \
		git checkout $(SHA) && \
		git submodule update --init --recursive && \
		git apply $(ROOTDIR)/patches/01-leveldb

TARGET+=	vanilla
DEP.vanilla=	.clone.cfg
DIR.vanilla=	vanilla
CFG.vanilla=	cmake -S ../$(DIR.clone) \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_CXX_COMPILER=$(CXX) \
			-DHAVE_STD_REGEX=ON \
			-DRUN_HAVE_STD_REGEX=1
BLD.vanilla=	$(MAKE) -j 8

TARGET+=	sanitized
DEP.sanitized=	.clone.cfg
DIR.sanitized=	sanitized
CFG.sanitized=	cmake -S ../$(DIR.clone) \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_CXX_COMPILER=$(CXX) \
			-DCMAKE_CXX_FLAGS=$(CXXFLAGS_EXTRA) \
			-DCMAKE_EXE_LINKER_FLAGS=$(LDFLAGS) \
			-DHAVE_STD_REGEX=ON \
			-DRUN_HAVE_STD_REGEX=1
BLD.sanitized=	$(MAKE) -j 8

TARGET+=	populate
DIR.populate=	$(DIR.vanilla)
RUN.populate= 	rm -rf $(DBDIR) && $(COMMAND) --threads=1 --benchmarks=fillseq

TSAN_ASLR_WORKAROUND := $(shell \
    if [ "$$(uname -s)" = "Linux" ] && \
       uname -r | awk -F. '{if ($$1 > 6 || ($$1 == 6 && $$2 >= 6)) exit 0; else exit 1}'; then \
        echo "setarch $$(uname -m) -R"; \
    fi \
)

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------
TARGET+=	header

TARGET+=	baseline
DEP.baseline=	.populate.run
DIR.baseline=	$(DIR.vanilla)
RUN.baseline= 	$(TESTER) '$(RUNCMD)'
PRO.baseline=	$(PARSE)

TARGET+=	tsan
DEP.tsan=	.populate.run
DIR.tsan=	$(DIR.sanitized)
RUN.tsan=	$(TESTER) 'env TSAN_OPTIONS=report_bugs=0 $(TSAN_ASLR_WORKAROUND) $(RUNCMD)'
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DEP.tsano=	.populate.run
DIR.tsano=	$(DIR.sanitized)
RUN.tsano=	$(TESTER) 'env TSANO_LIBDIR=$(TSANO_LIBDIR) $(TSANO_CMD) $(RUNCMD)'
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DEP.coldtrace=	.populate.run
DIR.coldtrace=	$(DIR.sanitized)
DIR.coldtrace=	$(DIR.tsan)
RUN.coldtrace=	$(TESTER) 'env $(COLDTRACE_CMD) $(RUNCMD)'
PRO.coldtrace=	$(PARSE)

TARGET+=	nowrites
DEP.nowrites=	.populate.run
DIR.nowrites=	$(DIR.sanitized)
DIR.nowrites=	$(DIR.tsan)
RUN.nowrites=	$(TESTER) 'env COLDTRACE_DISABLE_WRITES=true \
			$(COLDTRACE_CMD) $(RUNCMD)'
PRO.nowrites=	$(PARSE)

# file to build internal tool (freezer)
# assumes libfreeze.so to be in the current directory
#TARGET+=	freezer
DEP.freezer=	.populate.run
DIR.freezer=	$(DIR.sanitized)
DIR.freezer=	$(DIR.tsan)
RUN.freezer=	FREEZER_TRACE_PATH=traces $(TESTER) \
		'env LD_PRELOAD=$(ROOTDIR)/libfreeze.so $(RUNCMD)'
PRO.freezer=	$(PARSE)

# ------------------------------------------------------------------------------
BENCHMK= ../bench.mk
include $(BENCHMK)
include $(ROOTDIR)/../common.mk
