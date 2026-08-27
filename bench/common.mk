# ROOTDIR is given by bench.mk, it refers to the directory of the benchmark
# PROJECT refers to the coldtrace source directory
PROJECT!=	readlink -f $(ROOTDIR)/../..

# The expected BUILD_DIR of coldtrace is $(PROJECT)/build
BUILD_DIR=	$(PROJECT)/build

# Common coldtrace options and configuration of tsano command
TSANO_CMD=	$(PROJECT)/deps/dice/deps/tsano/tsano
TSANO_LIBDIR=	$(BUILD_DIR)/deps/dice/deps/tsano
COLDTRACE_CMD=	COLDTRACE_PATH=traces \
		COLDTRACE_MAX_FILES=3 \
		COLDTRACE_DISABLE_CLEANUP=true \
		COLDTRACE_DISABLE_COPY=true \
		$(PROJECT)/scripts/coldtrace

# Compiler and linker configuration
CC=		gcc
CXX=		g++
CFLAGS_EXTRA=	-fsanitize=thread
CXXFLAGS_EXTRA=	-fsanitize=thread
# Force clang++ to link libtsan as shared library. (Yes, the flag is libsan)
LDFLAGS!=	if [ "$(CXX)" = "clang++" ]; then echo '-shared-libsan'; fi

# ------------------------------------------------------------------------------
# Run-time wrapper (TESTER) selection
#
#   PROFILE=1       -> wrap each run in `perf record`
#   NO_HYPERFINE=1  -> standalone wall-clock timer ($(PROJECT)/scripts/bench-time.sh)
#   otherwise       -> hyperfine if available, else plain `sh -c`
# ------------------------------------------------------------------------------

FREQ?=		    999
CALL_GRAPH?=	dwarf
PERF?=		    perf
PERF_EVENT?=

TESTER!=	if [ -n "$(PROFILE)" ]; then \
				echo "$(PERF) record -g --call-graph $(CALL_GRAPH) -F $(FREQ) $(PERF_EVENT) -o $(WORKDIR)/\$$*.perf.data -- sh -c"; \
			elif [ -n "$(NO_HYPERFINE)" ]; then \
				echo "$(PROJECT)/scripts/bench-time.sh"; \
			elif which hyperfine > /dev/null 2>&1; then \
				echo "hyperfine --warmup 1"; \
			else \
				echo "sh -c"; fi

# Parser for hyperfine output: mean (+ stddev), converted to milliseconds.
PARSE_HF=   awk -v tgt=$* '/Time/ && /mean/ { \
                m = \$$\$$5; \
                if (\$$\$$6 ~ /^s\$$\$$/) m = m * 1000; \
                s = \$$\$$8; \
                if (\$$\$$9 ~ /^s\$$\$$/) s = s * 1000; \
                print tgt, m, s \
            }' $(WORKDIR)/$*.run.log | sed 's/ /;/g' | tee -a $(WORKDIR)/results.csv

# Parser for the standalone timer output: "time_ms <ms>".
PARSE_NOHF= awk -v tgt=$* '/^time_ms/ { print tgt, \$$\$$2 }' \
            $(WORKDIR)/$*.run.log | sed 's/ /;/g' | tee -a $(WORKDIR)/results.csv

# Select the parser at run time based on NO_HYPERFINE.
PARSE=      if [ -n '$(NO_HYPERFINE)' ]; then $(PARSE_NOHF); else $(PARSE_HF); fi

# Add TARGET+=header to initialize the results.csv file.
PRO.header= if [ -n '$(NO_HYPERFINE)' ]; then \
                echo 'variant; time_ms' > $(WORKDIR)/results.csv; \
            else \
                echo 'variant; time_ms; stddev_ms' > $(WORKDIR)/results.csv; fi
