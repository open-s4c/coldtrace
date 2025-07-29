# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: MIT

# ------------------------------------------------------------------------------
# Commands and variables for all LevelDB benchmark variants
# ------------------------------------------------------------------------------
PROJECT!=	readlink -f $(ROOTDIR)/../..

BUILD_DIR=	$(PROJECT)/build
TSANO_CMD=	$(PROJECT)/deps/dice/deps/tsano/tsano
TSANO_LIBDIR=	$(BUILD_DIR)/deps/dice/deps/tsano
COLDTRACE_CMD=	COLDTRACE_MAX_FILES=3 \
		COLDTRACE_DISABLE_CLEANUP=true \
		COLDTRACE_DISABLE_COPY=true \
		$(PROJECT)/scripts/coldtracer

TESTER!=	if which hyperfine; \
		then echo "hyperfine"; \
		else echo "sh -c"; fi

DBDIR=		/tmp/bench.db
COMMAND=	./db_bench --db=$(DBDIR)
OS!=		uname
SOEXT!=		if [ "$(OS)" = "Darwin" ]; \
		then echo .dylib; \
		else echo .so; \
		fi
CONFIGURE=	cmake -S ../../leveldb -DCMAKE_BUILD_TYPE=Release \
			-DHAVE_STD_REGEX=ON -DRUN_HAVE_STD_REGEX=1
BUILD= 		$(MAKE)
PREPARE= 	rm -rf $(DBDIR) && $(COMMAND) --threads=1 --benchmarks=fillseq
RUNCMD= 	$(COMMAND) --threads=1 --benchmarks=readrandom --duration=10
PARSE=		cat $(WORKDIR)/$*.run.log  \
		| grep benchstats \
		| cut -d: -f2 \
		| xargs -n1 echo "$*"';' \
		| tee -a $(WORKDIR)/results.csv
BUNDLE_DIR=	$(PROJECT)/build/bench/lib/

# ------------------------------------------------------------------------------
# clone level, apply patches, build dice
# ------------------------------------------------------------------------------

URL=		https://github.com/google/leveldb.git
SHA=		068d5ee1a3ac40dabd00d211d5013af44be55bea

TARGET+=	clone
DIR.clone=	leveldb
CFG.clone=	git clone --recurse-submodules $(URL) .
BLD.clone=	git checkout $(SHA) && git submodule update --init --recursive

TARGET+=	patch
DEP.patch=	.clone.bld
DIR.patch=	leveldb
CFG.patch=	git checkout .
BLD.patch=	git apply $(ROOTDIR)/patches/01-leveldb

TARGET+=	populate
DEP.populate=	.patch.bld
DIR.populate=	build/leveldb
CFG.populate= 	$(CONFIGURE)
BLD.populate=	$(BUILD)
RUN.populate= 	$(PREPARE)

TARGET+=	results
PRO.results=	echo 'variant; time_s; count; count_1' > $(WORKDIR)/results.csv

# ------------------------------------------------------------------------------
# Variants
# ------------------------------------------------------------------------------

TARGET+=	baseline
DEP.baseline=	.populate.bld
DIR.baseline=	build/leveldb
CFG.baseline= 	$(CONFIGURE)
BLD.baseline=	$(BUILD)
RUN.baseline= 	$(TESTER) 'env $(RUNCMD)'
PRO.baseline=	$(PARSE)

TARGET+=	tsan
DEP.tsan=	.baseline.run
DIR.tsan=	build/leveldb-tsan
CFG.tsan= 	$(CONFIGURE) -DCMAKE_CXX_FLAGS=-fsanitize=thread
BLD.tsan=	$(BUILD)
RUN.tsan=	$(TESTER) 'env TSAN_OPTIONS=report_bugs=0 $(RUNCMD)'
PRO.tsan=	$(PARSE)
# Linux: if TSAN fails with "FATAL: ThreadSanitizer: unexpected memory mapping"
# Try running this
#	sudo sysctl vm.mmap_rnd_bits=30

TARGET+=	tsano
DEP.tsano=	.tsan.bld
DIR.tsano=	$(DIR.tsan)
RUN.tsano=	$(TESTER) 'env TSANO_LIBDIR=$(TSANO_LIBDIR) \
			$(TSANO_CMD) $(RUNCMD)'
PRO.tsano=	$(PARSE)

TARGET+=	coldtrace
DEP.coldtrace=	.tsan.bld
DIR.coldtrace=	$(DIR.tsan)
RUN.coldtrace=	$(TESTER) 'env COLDTRACE_PATH=traces $(COLDTRACE_CMD) $(RUNCMD)'
PRO.coldtrace=	$(PARSE)

# file to build internal tool (freezer)
# assumes libfreeze.so to be in the current directory
TARGET+=	freezer
DEP.freezer=	.tsan.bld
DIR.freezer=	$(DIR.tsan)
RUN.freezer=	FREEZER_TRACE_PATH=traces $(TESTER) \
		'env LD_PRELOAD=$(ROOTDIR)/libfreeze.so $(RUNCMD)'
PRO.freezer=	$(PARSE)

# ------------------------------------------------------------------------------
BENCHMK= ../bench.mk
include $(BENCHMK)
