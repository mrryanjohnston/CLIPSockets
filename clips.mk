# The makefile CLIPSockets builds CLIPS with.
#
# The top-level makefile copies this file into the build directory of the
# selected CLIPS version, vendor/clips-build/<tag>, over the makefile that
# CLIPS ships, after it has copied the sources this project adds. It is never
# edited there: that directory is a copy of a downloaded tree, and this file in
# it is replaced on every build. Edit clips.mk in the root of this project.
#
# It replaces the CLIPS makefile rather than adding to it because the object
# list here is computed and not written out. CLIPS 6.4.2, branches/64x and
# branches/70x have different files -- 70x adds the deftable construct and
# goal-driven facts, and the branches gain files between releases -- and in
# every one of them the library is every .c file except main.c. Reading the
# directory is therefore both shorter and correct for all three, where a list
# would have to be kept per version.
#
# The variables the top-level makefile passes in:
#
#   BINARY      where to write the clips executable
#   CLIPS_OS    LINUX or DARWIN, which CLIPS wants as a -D
#
# It also passes TLS, TLS_BACKEND, TLS_PREFIX and MAGIC on this makefile's
# command line, at the value you gave it or at the default it shares with this
# file, so that the four have one place to be read from and can be recorded in
# the build configuration stamp. The Installation section of README.md
# describes what each of them accepts.

PLATFORM = $(shell uname -s)

ifeq ($(PLATFORM),Darwin) # macOS
	CLIPS_OS ?= DARWIN
	WARNINGS = -Wall -Wundef -Wpointer-arith -Wshadow -Wstrict-aliasing \
	           -Winline -Wmissing-declarations -Wredundant-decls \
	           -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes \
	           -Waggregate-return -Wno-implicit
endif

ifeq ($(PLATFORM),Linux) # linux
	CLIPS_OS ?= LINUX
	WARNINGS = -Wall -Wundef -Wpointer-arith -Wshadow -Wstrict-aliasing \
               -Winline -Wredundant-decls -Waggregate-return
endif

CLIPS_OS ?= LINUX

# Where the executable goes. This file runs in vendor/clips-build/<tag>, three
# levels below the root of the project, and the binary belongs in the root,
# where the tests and the examples look for it. The top-level makefile always
# passes an absolute BINARY; this default is for running make in here by hand.
BINARY ?= ../../../clips

# Every .c file in the directory is part of the library, except these. main.c
# builds the executable around the library. tlscheck.c is a standalone program
# that the tls-check target compiles and runs. The socktls-* files are the TLS
# backends, and a build compiles exactly one of them -- TLS_OBJS below names
# which.
NOT_LIB = main.c tlscheck.c socktls.c \
	socktls-openssl.c socktls-mbedtls.c socktls-gnutls.c socktls-s2n.c

OBJS = $(patsubst %.c,%.o,$(filter-out $(NOT_LIB),$(wildcard *.c)))
# libmagic (the "file" command's library) is an optional dependency. It is
# only used by the (mimetype) function and is left out by default so that no
# extra libraries are needed to build. To build with it:
#
#   make MAGIC=1
MAGIC ?= 0

ifeq ($(MAGIC),0)
MAGIC_CFLAGS =
MAGIC_LDLIBS =
else
MAGIC_CFLAGS = -DUSE_LIBMAGIC
MAGIC_LDLIBS = -lmagic
endif

# The build includes TLS by default:
#
#   make                              use the backend that is installed
#   make TLS_BACKEND=wolfssl          name a backend
#   make TLS_PREFIX=/opt/foo          look in a different install prefix
#   make TLS=0                        build with no TLS
#
# TLS_BACKEND is openssl, libressl, wolfssl, boringssl, mbedtls, gnutls, s2n or
# auto. The default is auto. Automatic detection asks pkg-config for "libssl
# libcrypto", then for "wolfssl", then for "mbedtls mbedx509 mbedcrypto", and
# then for "gnutls". It takes the first library that answers. If it finds no
# library, it stops the build and selects nothing. It also writes the name of
# the library that it selected, because the same command can otherwise give
# different results on two machines. It never detects boringssl and s2n,
# because those two install nothing to detect.
#
#   openssl, libressl  These two have the same pkg-config files. The compiler
#                      tells them apart with LIBRESSL_VERSION_NUMBER.
#
#   wolfssl            This backend needs -DTLS_BACKEND_WOLFSSL.
#
#   mbedtls, gnutls    pkg-config finds these two under their own names, and
#                      they need no define. mbedtls has three modules and not
#                      one, because it keeps the protocol, the certificates
#                      and the ciphers apart. Neither library uses the
#                      OpenSSL API, and each of them has a backend object
#                      of its own.
#
#   boringssl, s2n     These two need TLS_PREFIX. They supply no pkg-config
#                      files and have no install convention.
#                      boringssl needs no define, because it
#                      defines OPENSSL_IS_BORINGSSL in its headers. It is
#                      C++ below, and the link line needs libstdc++. s2n is
#                      built on libcrypto and does not supply it, and the link
#                      line also needs libcrypto.
#
# The headers of a TLS library obey the standards of that library and not the
# standards of this project. -isystem gives that rule. gcc reports nothing from
# a directory that -isystem names, and that keeps -Wredundant-decls quiet about
# the PSA functions that mbedTLS 3.x declares two times.
#
# This file also changes the -I flags from pkg-config. gcc
# searches each -I directory before each -isystem directory. Without the
# change, the same headers would go back on the path with the warnings, and the
# -isystem flags would have no effect.
TLS_AS_SYSTEM = $(patsubst -I%,-isystem %,$(1))

TLS ?= 1
TLS_BACKEND ?= auto
TLS_PREFIX ?=

ifeq ($(TLS_PREFIX),)
PKG_CONFIG = pkg-config
TLS_RPATH =
else
# Debian and the systems from it put .pc files in a multiarch directory and not
# in lib/pkgconfig. As a result, TLS_PREFIX=/usr must also look there. Without
# that, it finds nothing where the library is.
TLS_EMPTY :=
TLS_SPACE := $(TLS_EMPTY) $(TLS_EMPTY)
TLS_PC_DIRS = $(TLS_PREFIX)/lib/pkgconfig $(TLS_PREFIX)/lib64/pkgconfig $(wildcard $(TLS_PREFIX)/lib/*/pkgconfig)
PKG_CONFIG_BASE = PKG_CONFIG_PATH=$(subst $(TLS_SPACE),:,$(strip $(TLS_PC_DIRS))):$(PKG_CONFIG_PATH) pkg-config
# A .pc file gives the prefix of the configuration of the library, and the
# files are not always at that prefix. A staged install and a distribution
# package in a directory both leave .pc files that give /usr or /usr/local for
# files at a different location.
#
# TLS_PREFIX corrects this. A new value for the prefix variable makes
# pkg-config calculate each path in the file again from that prefix. That is
# correct for a lib layout, a lib64 layout and a multiarch lib/<triplet>
# layout. (pkg-config also has --define-prefix. That option calculates the
# prefix and removes two parts of the path of the .pc file. It is correct for
# lib/pkgconfig and incorrect for multiarch, where it goes one level too
# deep.)
PKG_CONFIG = $(PKG_CONFIG_BASE) --define-variable=prefix=$(TLS_PREFIX)
# The loader must also find a library outside its search path at run time, and
# not only at link time.
TLS_RPATH = -Wl,-rpath,$(TLS_PREFIX)/lib
endif

ifeq ($(TLS),0)
TLS_CFLAGS =
TLS_LDLIBS =
TLS_OBJS =
else

ifeq ($(TLS_BACKEND),auto)
# "override", because the top-level makefile passes TLS_BACKEND on this
# makefile's command line so that it is recorded in the build configuration
# stamp. A command-line variable wins over a plain assignment here, and the
# detected name would never take effect without this word.
override TLS_BACKEND := $(shell if $(PKG_CONFIG) --exists libssl libcrypto; then echo openssl; elif $(PKG_CONFIG) --exists wolfssl; then echo wolfssl; elif $(PKG_CONFIG) --exists mbedtls mbedx509 mbedcrypto; then echo mbedtls; elif $(PKG_CONFIG) --exists gnutls; then echo gnutls; fi)
ifeq ($(TLS_BACKEND),)
$(error No TLS library found, and TLS is built by default. Looked for pkg-config "libssl libcrypto", "wolfssl", "mbedtls mbedx509 mbedcrypto" and "gnutls". Install one (libssl-dev on Debian/Ubuntu, openssl-devel on Fedora), or set TLS_PREFIX to the prefix yours is under, or build without TLS using "make TLS=0")
endif
TLS_AUTO := 1
endif

ifeq ($(TLS_BACKEND),s2n)

ifeq ($(TLS_PREFIX),)
$(error TLS_BACKEND=s2n needs TLS_PREFIX set to the directory holding its include/ and lib/, because s2n-tls installs no pkg-config files to find it by)
endif

# s2n is built on libcrypto and does not supply it. As a result, the link line
# names libcrypto here. This project also uses libcrypto directly, to read a
# name out of a peer certificate. s2n gives that certificate as DER only.
TLS_PKG =
TLS_CFLAGS = -DUSE_TLS -isystem $(TLS_PREFIX)/include
TLS_LDLIBS = -L$(TLS_PREFIX)/lib $(TLS_RPATH) -ls2n -lcrypto -lpthread

else ifeq ($(TLS_BACKEND),boringssl)

ifeq ($(TLS_PREFIX),)
$(error TLS_BACKEND=boringssl needs TLS_PREFIX set to the directory holding its include/ and lib/, because BoringSSL installs no pkg-config files to find it by)
endif

# BoringSSL is C++. It also has no version numbers, and its documentation says
# that its API can change.
TLS_PKG =
TLS_CFLAGS = -DUSE_TLS -isystem $(TLS_PREFIX)/include
TLS_LDLIBS = -L$(TLS_PREFIX)/lib $(TLS_RPATH) -lssl -lcrypto -lstdc++ -lpthread

else

ifeq ($(TLS_BACKEND),wolfssl)
TLS_PKG = wolfssl
TLS_BACKEND_DEFINE = -DTLS_BACKEND_WOLFSSL
else ifeq ($(TLS_BACKEND),openssl)
TLS_PKG = libssl libcrypto
TLS_BACKEND_DEFINE =
else ifeq ($(TLS_BACKEND),libressl)
TLS_PKG = libssl libcrypto
TLS_BACKEND_DEFINE =
else ifeq ($(TLS_BACKEND),gnutls)
TLS_PKG = gnutls
TLS_BACKEND_DEFINE =
else ifeq ($(TLS_BACKEND),mbedtls)
# mbedTLS has three modules, and the build needs each of them: the TLS
# protocol, the certificate code that it verifies with, and the ciphers below
# the two.
TLS_PKG = mbedtls mbedx509 mbedcrypto
TLS_BACKEND_DEFINE =
else
$(error TLS_BACKEND is "$(TLS_BACKEND)"; expected openssl, libressl, wolfssl, boringssl, mbedtls, gnutls or s2n)
endif

# Without this check, a backend that is not installed passes this point. The
# $(shell) calls below hide the failure. USE_TLS then has a value and there is
# no include path, and the build stops much later on a header that is
# absent.
ifneq ($(shell $(PKG_CONFIG) --exists $(TLS_PKG) && echo found),found)
$(error TLS_BACKEND is "$(TLS_BACKEND)" but pkg-config cannot find "$(TLS_PKG)". Install its development package, or set TLS_PREFIX to the prefix it is installed under)
endif

ifeq ($(TLS_PREFIX),)
TLS_CFLAGS = -DUSE_TLS $(TLS_BACKEND_DEFINE) $(call TLS_AS_SYSTEM,$(shell $(PKG_CONFIG) --cflags $(TLS_PKG)))
TLS_LDLIBS = $(shell $(PKG_CONFIG) --libs $(TLS_PKG))
else
# pkg-config gives no -I and no -L for a directory that the compiler already
# searches. That is usually correct and is incorrect here. The compiler
# searches a second TLS library under /usr/local before /usr. As a result, a
# request for the library under /usr with no flags pairs its headers with the
# other library, and the link fails on symbols that only one of them has. A
# prefix must mean that prefix. As a result, this file puts the directories on
# the command line, and it does not use the decision of pkg-config.
TLS_INCDIR = $(shell $(PKG_CONFIG) --variable=includedir $(firstword $(TLS_PKG)))
TLS_LIBDIR = $(shell $(PKG_CONFIG) --variable=libdir $(firstword $(TLS_PKG)))
TLS_CFLAGS = -DUSE_TLS $(TLS_BACKEND_DEFINE) -isystem $(TLS_INCDIR) $(call TLS_AS_SYSTEM,$(shell $(PKG_CONFIG) --cflags $(TLS_PKG)))
TLS_LDLIBS = -L$(TLS_LIBDIR) $(shell $(PKG_CONFIG) --libs $(TLS_PKG)) -Wl,-rpath,$(TLS_LIBDIR)
endif

endif

# This code writes the library that automatic detection selected, after the
# include path is known. pkg-config cannot tell the forks apart, because
# OpenSSL, LibreSSL and BoringSSL all install libssl.pc and libcrypto.pc under
# those exact names. As a result, "openssl" from pkg-config alone would be
# incorrect on each machine with LibreSSL in /usr/local.
ifeq ($(TLS_AUTO),1)
TLS_HASH := \#
TLS_FORK := $(shell printf '$(TLS_HASH)include <openssl/opensslv.h>\n' | \
	$(CC) $(TLS_CFLAGS) -E -dM -x c - 2>/dev/null | \
	grep -oE 'LIBRESSL_VERSION_NUMBER|OPENSSL_IS_BORINGSSL' | head -1)
ifeq ($(TLS_FORK),LIBRESSL_VERSION_NUMBER)
TLS_REPORTED := libressl
else ifeq ($(TLS_FORK),OPENSSL_IS_BORINGSSL)
TLS_REPORTED := boringssl
else
TLS_REPORTED := $(TLS_BACKEND)
endif
$(info TLS backend: $(TLS_REPORTED) (auto-detected; set TLS_BACKEND to override))
endif

# socktls.o is the generic half: the buffers, the tlsio router and the CLIPS
# functions. The second object is the
# backend itself. OpenSSL, LibreSSL, BoringSSL and wolfSSL share one object,
# because they are forks of the OpenSSL API or a compatibility layer above it.
ifeq ($(TLS_BACKEND),mbedtls)
TLS_OBJS = socktls.o socktls-mbedtls.o
else ifeq ($(TLS_BACKEND),gnutls)
TLS_OBJS = socktls.o socktls-gnutls.o
else ifeq ($(TLS_BACKEND),s2n)
TLS_OBJS = socktls.o socktls-s2n.o
else
TLS_OBJS = socktls.o socktls-openssl.o
endif

# tlscheck.c checks that the library for the link is the same implementation as
# the headers of the compile. It asks the question with OpenSSL calls. As a
# result, it applies only to the libraries that have those calls. The build
# also does not use it for wolfSSL. wolfSSL reaches this code through a
# compatibility layer that does not always have OpenSSL_version. Its headers
# and its library also come from one pkg-config file, and the two cannot
# differ in the manner of the other libraries.
ifneq ($(TLS_BACKEND),wolfssl)
ifneq ($(TLS_BACKEND),mbedtls)
ifneq ($(TLS_BACKEND),gnutls)
ifneq ($(TLS_BACKEND),s2n)
TLS_CHECK = tls-check
endif
endif
endif
endif
endif

OBJS += $(TLS_OBJS)

all: release

.PHONY: tls-check
tls-check:
	@if $(CC) $(MAGIC_CFLAGS) $(TLS_CFLAGS) -o tlscheck tlscheck.c $(TLS_LDLIBS) >/dev/null 2>&1; then \
		./tlscheck; status=$$?; \
		rm -f tlscheck; \
		if [ $$status -eq 127 ]; then \
			echo "" >&2; \
			echo "The TLS library was found at link time but not at run time, so a" >&2; \
			echo "binary built now would not start. Either add its directory to the" >&2; \
			echo "loader's search path -- on Linux that is ldconfig -- or build with" >&2; \
			echo "TLS_PREFIX set to its prefix, which records the location in the" >&2; \
			echo "binary itself." >&2; \
			echo "" >&2; \
			exit 1; \
		fi; \
		if [ $$status -ne 0 ]; then exit 1; fi; \
	else \
		echo "tls-check: skipped, this backend could not build the check" >&2; \
	fi

debug : CC = gcc
debug : CFLAGS = $(MAGIC_CFLAGS) $(TLS_CFLAGS) -std=c99 -O0 -g
debug : LDLIBS = -lm $(MAGIC_LDLIBS) $(TLS_LDLIBS)
debug : clips

release : CC = gcc
release : CFLAGS = $(MAGIC_CFLAGS) $(TLS_CFLAGS) -std=c99 -O3 -fno-strict-aliasing
release : LDLIBS = -lm -lc $(MAGIC_LDLIBS) $(TLS_LDLIBS)
release : clips

# Instrumented build for tests/coverage.sh. -O0 is required: at -O3 inlining
# scrambles the line attribution gcov reports. Always build this from clean
coverage : CC = gcc
coverage : CFLAGS = $(MAGIC_CFLAGS) $(TLS_CFLAGS) -std=c99 -O0 -g --coverage
coverage : LDLIBS = -lm -lc --coverage $(MAGIC_LDLIBS) $(TLS_LDLIBS)
coverage : clips

debug_cpp : CC = g++
debug_cpp : CFLAGS = $(MAGIC_CFLAGS) $(TLS_CFLAGS) -x c++ -std=c++11 -O0 -g
ifeq ($(PLATFORM),Darwin) # macOS
debug_cpp : WARNINGS += -Wcast-qual
endif
debug_cpp : LDLIBS = -lstdc++ $(MAGIC_LDLIBS) $(TLS_LDLIBS)
debug_cpp : clips

release_cpp : CC = g++
release_cpp : CFLAGS = $(MAGIC_CFLAGS) $(TLS_CFLAGS) -x c++ -std=c++11 -O3 -fno-strict-aliasing
ifeq ($(PLATFORM),Darwin) # macOS
release_cpp : WARNINGS += -Wcast-qual
endif
release_cpp : LDLIBS = -lstdc++ $(MAGIC_LDLIBS) $(TLS_LDLIBS)
release_cpp : clips

# -MMD -MP writes the header dependencies of each object beside it, so a change
# to a header rebuilds what includes it. The CLIPS makefile carries that list
# written out, from "gcc -MM *.c". A written list would be wrong here: it is
# different for each of the three CLIPS versions this builds against, and it
# would not mention the headers this project adds.
#
# The flags are on this line rather than in CFLAGS because tests/asan.sh and
# tests/coverage.sh pass a CFLAGS of their own on the command line, which
# replaces the value set here.
%.o : %.c
	$(CC) -c -D$(CLIPS_OS) $(CFLAGS) $(WARNINGS) -MMD -MP $<

clips : $(TLS_CHECK) main.o libclips.a
	$(CC) -o $(BINARY) main.o -L. -lclips $(LDLIBS)

libclips.a : $(OBJS)
	rm -f $@
	ar cq $@ $(OBJS)

clean :
	-rm -f main.o $(OBJS)
	-rm -f socktls.o socktls-openssl.o socktls-mbedtls.o socktls-gnutls.o socktls-s2n.o
	-rm -f $(BINARY) libclips.a tlscheck
	-rm -f *.d *.gcda *.gcno *.gcov *.gcov.json.gz

# The final value of any variable: "make print-TLS_CFLAGS". "make -p" would
# give the definition and not the value, because $(shell) calls make these
# values and -p writes them without their results.
#
# tests/asan.sh uses this target to build again with the flags of a usual
# build. A CFLAGS value on the command line replaces the value in the makefile.
# As a result, the sanitizer build must name each flag that a correct clips
# needs. This target keeps that list equal to the real list.
print-% :
	@echo '$($*)'

.PHONY : all clips clean debug release coverage debug_cpp release_cpp

-include $(OBJS:.o=.d) main.d
