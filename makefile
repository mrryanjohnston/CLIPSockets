# CLIPSockets builds on top of CLIPS, and this repository holds none of the
# CLIPS source. A build fetches the version you ask for, copies the files this
# project adds over it, and builds there. The binary lands in the root of this
# project, as ./clips.
#
# Which CLIPS:
#
#   make                        the 6.4.2 release tarball (the default)
#   make CLIPS_VERSION=svn-6x   branches/64x of the CLIPS Subversion repository
#   make CLIPS_VERSION=svn-7x   branches/70x
#
# The two branches are pinned to a revision, so that a build today and a build
# next month are the same build. Override CLIPS_SVN_REV to move one:
#
#   make CLIPS_VERSION=svn-7x CLIPS_SVN_REV=978
#   make CLIPS_VERSION=svn-7x CLIPS_SVN_REV=HEAD
#
# Each version is fetched into vendor/clips-source/<tag> and built in a copy of
# it under vendor/clips-build/<tag>. Switching versions therefore does not
# download again and never mixes the objects of one version into another.
# vendor/clips is a symlink to the version built last, and "make print-clips"
# says which that is.
#
# "make help" lists each target with a "##" comment. The text after ## is the
# one-line summary.

# The fetch and staging rules below are written before "all", so name the
# default goal explicitly rather than letting the first rule take it.
.DEFAULT_GOAL := all

# ---------------------------------------------------------------------------
# Which CLIPS to build against.
# ---------------------------------------------------------------------------

CLIPS_VERSION  ?= 6.4.2
CLIPS_VERSIONS := 6.4.2 svn-6x svn-7x

ARCHIVE     := clips_core_source_642.tar.gz
ARCHIVE_URL ?= https://sourceforge.net/projects/clipsrules/files/CLIPS/6.4.2/$(ARCHIVE)/download

CLIPS_SVN_ROOT   ?= https://svn.code.sf.net/p/clipsrules/code
CLIPS_SVN_6X_REV ?= 967
CLIPS_SVN_7X_REV ?= 978

# The digest SourceForge publishes for clips_core_source_642.tar.gz. It applies
# to the release only; a branch export has no such file to check, and the
# variable is empty there, which tells the fetch script to skip the check.
CLIPS_SHA256_6.4.2 := 608a1eb2fc6e9caff30d63d684095f0bca7108f2294d21ee6f5617427c10455a
CLIPS_SHA256 ?= $(CLIPS_SHA256_$(CLIPS_VERSION))
export CLIPS_SHA256

ifeq ($(CLIPS_VERSION),6.4.2)
  CLIPS_TAG    := 6.4.2
  CLIPS_FETCH   = tarball "$(ARCHIVE_URL)" "$(ARCHIVE)"
  CLIPS_ORIGIN := the 6.4.2 release tarball
else ifeq ($(CLIPS_VERSION),svn-6x)
  CLIPS_SVN_URL ?= $(CLIPS_SVN_ROOT)/branches/64x/core
  CLIPS_SVN_REV ?= $(CLIPS_SVN_6X_REV)
  CLIPS_TAG     := svn-6x-r$(CLIPS_SVN_REV)
  CLIPS_FETCH    = svn "$(CLIPS_SVN_URL)" "$(CLIPS_SVN_REV)"
  CLIPS_ORIGIN  := branches/64x at r$(CLIPS_SVN_REV)
else ifeq ($(CLIPS_VERSION),svn-7x)
  CLIPS_SVN_URL ?= $(CLIPS_SVN_ROOT)/branches/70x/core
  CLIPS_SVN_REV ?= $(CLIPS_SVN_7X_REV)
  CLIPS_TAG     := svn-7x-r$(CLIPS_SVN_REV)
  CLIPS_FETCH    = svn "$(CLIPS_SVN_URL)" "$(CLIPS_SVN_REV)"
  CLIPS_ORIGIN  := branches/70x at r$(CLIPS_SVN_REV)
else
  $(error CLIPS_VERSION is '$(CLIPS_VERSION)': expected one of $(CLIPS_VERSIONS))
endif

CLIPS_SRC_DIR := vendor/clips-source/$(CLIPS_TAG)
BUILD_DIR     := vendor/clips-build/$(CLIPS_TAG)
CLIPS_LINK    := vendor/clips

CLIPS_SRC_STAMP := $(CLIPS_SRC_DIR)/.clips-source
BUILD_STAMP     := $(BUILD_DIR)/.clips-source

BINARY := $(CURDIR)/clips

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  CLIPS_OS := DARWIN
else
  CLIPS_OS := LINUX
endif

# ---------------------------------------------------------------------------
# How the binary is configured. These four are the whole of it, and clips.mk
# has the same defaults. They are named here as well so that a build can be
# compared against the one before it: see $(CONFIG_STAMP) below.
# ---------------------------------------------------------------------------

TLS         ?= 1
TLS_BACKEND ?= auto
TLS_PREFIX  ?=
MAGIC       ?= 0

BUILD_CONFIG  = TLS=$(TLS) TLS_BACKEND=$(TLS_BACKEND) TLS_PREFIX=$(TLS_PREFIX) MAGIC=$(MAGIC)
CONFIG_STAMP := $(BUILD_DIR)/.build-config

# What this project adds to CLIPS. userfunctions.c replaces the empty one CLIPS
# ships -- that is the hook CLIPS provides for exactly this -- and it is also
# where InitializeSocketRouter is called, so no file of the CLIPS source needs
# editing. The rest are new files that no version of CLIPS has.
SOURCES := socketrtr.c socketrtr.h \
	socktls.c socktls.h socktlsbe.h \
	socktls-openssl.c socktls-gnutls.c socktls-mbedtls.c socktls-s2n.c \
	tlscheck.c userfunctions.c

SUBMAKE = $(MAKE) -C $(BUILD_DIR) BINARY=$(BINARY) CLIPS_OS=$(CLIPS_OS) \
	TLS=$(TLS) TLS_BACKEND=$(TLS_BACKEND) TLS_PREFIX=$(TLS_PREFIX) MAGIC=$(MAGIC)

# ---------------------------------------------------------------------------
# Fetching and staging.
#
# scripts/fetch-clips.sh puts a pristine tree in $(CLIPS_SRC_DIR). The build
# happens in a copy of it, because the files this project adds have to sit
# beside the CLIPS sources for the compiler to find them, and a downloaded
# tree is left as it was downloaded.
# ---------------------------------------------------------------------------

$(CLIPS_SRC_STAMP): | scripts/fetch-clips.sh
	./scripts/fetch-clips.sh $(CLIPS_FETCH) "$(CLIPS_SRC_DIR)"

$(BUILD_STAMP): $(CLIPS_SRC_STAMP)
	mkdir -p "$(BUILD_DIR)"
	cp -R "$(CLIPS_SRC_DIR)/." "$(BUILD_DIR)/"
	touch "$@"

# vendor/clips is a symlink to the version built last.
define point_clips_link
	@[ ! -e "$(CLIPS_LINK)" ] || [ -L "$(CLIPS_LINK)" ] || rm -rf "$(CLIPS_LINK)"
	@ln -sfn "clips-build/$(CLIPS_TAG)" "$(CLIPS_LINK)"
	@echo "$(CLIPS_LINK) -> clips-build/$(CLIPS_TAG)  ($(CLIPS_ORIGIN))"
endef

# Which TLS library the binary links, and whether it has libmagic, is not
# visible in any prerequisite: every configuration builds the same objects out
# of the same sources. Without this, "make" followed by "make no-tls" would
# link objects compiled with -DUSE_TLS, and "make" followed by "make magic"
# would link a userfunctions.o that has no (mimetype) in it and say nothing.
# Recording the configuration beside the build gives make something to notice.
define check_build_config
	@if [ ! -f "$(CONFIG_STAMP)" ] || \
	   [ "$$(cat '$(CONFIG_STAMP)')" != '$(BUILD_CONFIG)' ]; then \
		if [ -f "$(CONFIG_STAMP)" ]; then \
			echo "the build configuration changed, so this build starts from clean"; \
			$(SUBMAKE) clean >/dev/null 2>&1 || true; \
		fi; \
		printf '%s\n' '$(BUILD_CONFIG)' > "$(CONFIG_STAMP)"; \
	fi
endef

# Our own files are copied on every build, with their timestamps, so make in
# the build directory rebuilds what changed and nothing else.
.PHONY: stage
stage: $(BUILD_STAMP)
	@cp -p $(SOURCES) $(BUILD_DIR)/
	@cp -p clips.mk $(BUILD_DIR)/makefile
	$(check_build_config)

# ---------------------------------------------------------------------------
# Builds.
# ---------------------------------------------------------------------------

all: stage ## Build clips with TLS (the default)
	$(SUBMAKE) release
	$(point_clips_link)
magic: MAGIC := 1
magic: stage ## Build with the optional libmagic dependency, enabling the (mimetype) function
	$(SUBMAKE) release
	$(point_clips_link)
# TLS is on by default. As a result, this target is another name for "make",
# and it stays here for instructions that use it. Select a backend with
# TLS_BACKEND=openssl|libressl|wolfssl|mbedtls|gnutls|s2n|boringssl. For an
# install at a different location, use TLS_PREFIX=/path. A variable on this
# command line reaches the sub-make, and "make tls MAGIC=1" also operates.
tls: TLS := 1
tls: stage ## Synonym for the default build
	$(SUBMAKE) release
	$(point_clips_link)
# Builds with no TLS. This build needs no library except the C standard
# library.
no-tls: TLS := 0
no-tls: stage ## Build with no TLS
	$(SUBMAKE) release
	$(point_clips_link)
# Deprecated alias for the default build, which already omits libmagic.
NO_IMAGE_MAGICK: all
# The debug and coverage builds of clips.mk. tests/asan.sh and tests/backend.sh
# reach them through here, so a build by hand and a build by a script go
# through the same path.
debug: stage ## Build with debugging symbols and no optimisation
	$(SUBMAKE) debug
	$(point_clips_link)
release: all ## Synonym for the default build

# Fetching the selected CLIPS without building it, for priming a cache or for
# looking at what a branch is doing.
clips-source: $(CLIPS_SRC_STAMP) ## Fetch the selected CLIPS source and stop
	@cat "$(CLIPS_SRC_STAMP)"

print-clips: ## Say which CLIPS this build uses and where it is
	@echo 'CLIPS_VERSION $(CLIPS_VERSION)'
	@echo 'origin        $(CLIPS_ORIGIN)'
	@echo 'source        $(CLIPS_SRC_DIR)'
	@echo 'build         $(BUILD_DIR)'
	@echo 'binary        $(BINARY)'
	@[ -f "$(CLIPS_SRC_STAMP)" ] && printf 'fetched       ' && cat "$(CLIPS_SRC_STAMP)" || true

print-clips-versions:
	@echo '$(CLIPS_VERSIONS)'

# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------

test: ## Run the test suite against the current build
	./tests/run.sh
# Builds each configuration that this machine can build and runs the suite on
# each of them. The configurations are the build with no TLS, and each TLS
# library that is present, with libmagic and without libmagic.
matrix: ## Build and test every configuration this machine can build
	./tests/matrix.sh
# Builds against each CLIPS version this project supports, one after another,
# and runs the suite on each.
test-clips: ## Build and test against 6.4.2, branches/64x and branches/70x
	./scripts/test-clips-versions.sh
# Builds with AddressSanitizer and runs the suite on that build. It finds
# defects that a check cannot find. Takes a configuration name
# as the scripts above do: "./tests/asan.sh gnutls".
asan: ## Rebuild under AddressSanitizer and run the suite
	./tests/asan.sh
# Lists all configurations that can be built.
test-list: ## Name the configurations this machine can build
	./tests/backend.sh list
test-%: ## Build and test one configuration, e.g. "make test-gnutls"
	./tests/backend.sh $*
# What the matrix can build on this machine, and how to get the other
# libraries. This target only writes a report.
# "./tests/provision.sh --build" builds the libraries.
provision: ## Report which TLS libraries are present and how to get the rest
	./tests/provision.sh
coverage: MAGIC := 1
coverage: stage ## Line coverage for the current build's TLS backend
	$(SUBMAKE) clean
	$(SUBMAKE) coverage
	$(point_clips_link)
	-./tests/run.sh
	./tests/coverage.sh
coverage-all: ## Line coverage merged across every TLS library
	./tests/coverage-all.sh

# ---------------------------------------------------------------------------
# Cleaning. The fetched sources under vendor/clips-source are left alone: they
# are the slow half to get back, and nothing a build writes goes there.
# ---------------------------------------------------------------------------
clean: ## Remove the binary and every build tree
	-rm -rf vendor/clips-build "$(CLIPS_LINK)"
	-rm -f ./clips
distclean: clean ## Also remove the fetched CLIPS source and the archive
	rm -rf vendor
	rm -f "$(ARCHIVE)"

# The value of a variable in the build makefile: "make print-TLS_CFLAGS".
# tests/asan.sh reads the flags of a usual build this way, so a sanitized build
# cannot drift from a usual one. The targets above that start with print- are
# explicit rules, and make prefers those to this pattern.
print-%: stage
	@$(SUBMAKE) --no-print-directory print-$*

help: ## List these targets
	@printf 'CLIPSockets targets:\n\n'
	@grep -E '^[a-zA-Z0-9_%-]+:.*## ' $(MAKEFILE_LIST) \
		| awk -F':.*## ' '{ printf "  %-14s %s\n", $$1, $$2 }' \
		| sort
	@printf '\nThis build uses CLIPS %s: %s.\n' '$(CLIPS_VERSION)' '$(CLIPS_ORIGIN)'
	@printf '\n  CLIPS_VERSION=6.4.2   the release tarball from SourceForge\n'
	@printf '  CLIPS_VERSION=svn-6x  branches/64x, pinned at r%s\n' '$(CLIPS_SVN_6X_REV)'
	@printf '  CLIPS_VERSION=svn-7x  branches/70x, pinned at r%s\n' '$(CLIPS_SVN_7X_REV)'
	@printf '  CLIPS_SVN_REV=        build a branch at another revision, or at\n'
	@printf '                        HEAD (needs svn installed)\n'
	@printf '  CLIPS_SVN_URL=        take the branch from somewhere else\n'
	@printf '\nOther variables: TLS_BACKEND, TLS_PREFIX, MAGIC.\n'
	@printf 'See the Installation section of README.md for what each accepts.\n'

.PHONY: all magic tls no-tls NO_IMAGE_MAGICK debug release clips-source \
	print-clips print-clips-versions \
	test matrix test-clips asan test-list provision \
	coverage coverage-all clean distclean help
