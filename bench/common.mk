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
		$(PROJECT)/scripts/coldtracer

# Compiler and linker configuration
CC=		gcc
CXX=		g++
CFLAGS_EXTRA=	-fsanitize=thread
CXXFLAGS_EXTRA=	-fsanitize=thread
# Force clang++ to link libtsan as shared library. (Yes, the flag is libsan)
LDFLAGS!=	if [ "$(CXX)" = "clang++" ]; then echo '-shared-libsan'; fi

# For testing we use hyperfine if available, otherwise simply call command
TESTER!=	if which hyperfine > /dev/null; \
		then echo "hyperfine"; \
		else echo "sh -c"; fi

# If hyperfine is used, we can parse the results with the following command
PARSE=		cat $(WORKDIR)/$*.run.log \
		| grep Time | tr -s ' ' \
		| cut -d' ' -f6,9 \
		| tr ' ' '\;' \
		| xargs -n1 echo "$*"';' \
		| tee -a $(WORKDIR)/results.csv

# Add TARGET+=header to initialize the results.csv file
PRO.header=	echo 'variant; time_ms; stddev' > $(WORKDIR)/results.csv
