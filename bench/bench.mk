# Copyright (c) 2025 Diogo Behrens
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# ------------------------------------------------------------------------------
# bench.mk - a terrible benchmark framework for stuborn developers
#   version: 0.1.3
#   license: 0BSD
#
# Actions:
# - configure
# - build
# - prepare
# - run
# - process
# - clean
#
# Variables of benchmark `bench`:
# - DIR.bench
# - CFG.bench
# - BLD.bench
# - PRE.bench
# - RUN.bench
# - PRO.bench
# - DEP.bench
#
# This makefile uses the variables TARGET, VERBOSE, and FORCE for command line
# configuration. Do not use these variables for other purposes in your
# Makefile. Besides these, other "hidden" variables are _TARGET, _ACTION, _CMD,
# _DIR, and _LOG. Setting these will break this makefile. Variable WORKDIR
# configures the directory where targets are executed.
# ------------------------------------------------------------------------------

# This file is compatible with BSD Make and GNU Make
.POSIX:
.SUFFIXES:
.SECONDARY:

# General configuration
WORKDIR=	work

# User targets
help:
	@echo "Usage: make <action> TARGET=<bench>"
	@echo
	@echo "Actions:"
	@echo " configure       configure benchmark with CFG.bench"
	@echo " build           build benchmark using BLD.bench"
	@echo " run             run benchmark using RUN.bench"
	@echo " process         process results using PRO.bench"

configure:
	@$(MAKE) cfg
build:
	@$(MAKE) bld
prepare:
	@$(MAKE) pre
process:
	@$(MAKE) pro
clean:
	rm -rf .*.enable .*.dir .*.cfg .*.bld .*.run .*.pre .*.pro .*.log
	rm -rf $(WORKDIR)

# ------------------------------------------------------------------------------
# Action bootstrap
# ------------------------------------------------------------------------------
.SUFFIXES: .enable .dir .cfg .bld .pre .run .pro

dir cfg bld pre run pro:
	@if [ -z "$(TARGET)" ]; then $(MAKE) help; fi
	@for B in $(TARGET); do                           \
		$(MAKE) _dispatch _TARGET=$$B _ACTION=$@; \
	done

_dispatch: $(DEP.$(_TARGET))
	@if [ ! -f ".$(_TARGET).enable" ]; then \
		touch .$(_TARGET).enable;       \
	fi
	@if [ ! -z "$(FORCE)" ]; then         \
		rm -f .$(_TARGET).$(_ACTION); \
	fi
	@$(MAKE) .$(_TARGET).$(_ACTION)

# ------------------------------------------------------------------------------
# Inference rules
# ------------------------------------------------------------------------------

.enable.dir:
	@if [ -z "$(DIR$*)" ]; then           \
		mkdir -p $(WORKDIR)/$*;       \
	else                                  \
		mkdir -p $(WORKDIR)/$(DIR$*); \
	fi
	@touch $@

.dir.cfg:
	@$(MAKE) _exec _TARGET=$* _DIR="$(DIR$*)" _CMD="$(CFG$*)" _ACTION=$@
	@touch $@

.cfg.bld:
	@$(MAKE) _exec _TARGET=$* _DIR="$(DIR$*)" _CMD="$(BLD$*)" _ACTION=$@
	@touch $@

.bld.pre:
	@$(MAKE) _exec _TARGET=$* _DIR="$(DIR$*)" _CMD="$(PRE$*)" _ACTION=$@
	@touch $@

.pre.run:
	@$(MAKE) _exec _TARGET=$* _DIR="$(DIR$*)" _CMD="$(RUN$*)" _ACTION=$@
	@touch $@

.run.pro:
	@$(MAKE) _exec _TARGET=$* _DIR="$(DIR$*)" _CMD="$(PRO$*)" _ACTION=$@
	@touch $@

# ------------------------------------------------------------------------------
# Generic _execution targets
# ------------------------------------------------------------------------------

# arguments: _CMD _ACTION _DIR _TARGET
_exec:
	@if [ "$(_CMD)" != "" ]; then \
		$(MAKE) _exec-dir;    \
	fi

_exec-dir:
	@if [ -z "$(_DIR)" ]; then                             \
		$(MAKE) _exec-mode _DIR=$(WORKDIR)/$(_TARGET)  \
			_LOG=$(_ACTION).log ;                  \
	else                                                   \
		$(MAKE) _exec-mode _DIR=$(WORKDIR)/$(_DIR)     \
			_LOG=$(_ACTION).log;                   \
	fi

_exec-mode:
	@echo -n "$(_ACTION) ... "
	@if [ -z "$(VERBOSE)" ]; then   \
		$(MAKE) _exec-quiet;   \
	else 	                       \
		$(MAKE) _exec-verbose; \
	fi
	@echo done

_exec-verbose:
	(cd $(_DIR) && $(_CMD)) 2>&1 | tee $(_LOG)
_exec-quiet:
	@(cd $(_DIR) && $(_CMD)) > $(_LOG) 2>&1 || (cat $(_LOG) && false)
