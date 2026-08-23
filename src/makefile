# CLIPS 6.4.1
# Console and Library
# macOS 13.3.1
# Ubuntu 22.04 LTS 
# Debian 11.3 
# Fedora 36 
# CentOS 9 
# Mint 20.3 

# The GNU Make Manual
# http://www.gnu.org/software/make/manual/make.html

# https://felixcrux.com/blog/creating-basic-makefile

# platform detection
PLATFORM = $(shell uname -s)

ifeq ($(PLATFORM),Darwin) # macOS
	CLIPS_OS = DARWIN
	WARNINGS = -Wall -Wundef -Wpointer-arith -Wshadow -Wstrict-aliasing \
	           -Winline -Wmissing-declarations -Wredundant-decls \
	           -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes \
	           -Waggregate-return -Wno-implicit
endif

ifeq ($(PLATFORM),Linux) # linux
	CLIPS_OS = LINUX
	WARNINGS = -Wall -Wundef -Wpointer-arith -Wshadow -Wstrict-aliasing \
               -Winline -Wredundant-decls -Waggregate-return
endif
	    
OBJS = agenda.o analysis.o argacces.o bload.o bmathfun.o bsave.o \
 	classcom.o classexm.o classfun.o classinf.o classini.o \
 	classpsr.o clsltpsr.o commline.o conscomp.o constrct.o \
 	constrnt.o crstrtgy.o cstrcbin.o cstrccom.o cstrcpsr.o \
 	cstrnbin.o cstrnchk.o cstrncmp.o cstrnops.o cstrnpsr.o \
 	cstrnutl.o default.o defins.o developr.o dffctbin.o dffctbsc.o \
 	dffctcmp.o dffctdef.o dffctpsr.o dffnxbin.o dffnxcmp.o dffnxexe.o \
 	dffnxfun.o dffnxpsr.o dfinsbin.o dfinscmp.o drive.o emathfun.o \
 	engine.o envrnmnt.o envrnbld.o evaluatn.o expressn.o exprnbin.o \
 	exprnops.o exprnpsr.o extnfunc.o factbin.o factbld.o factcmp.o \
 	factcom.o factfun.o factgen.o factgoal.o facthsh.o factfile.o factlhs.o \
 	factmch.o factmngr.o factprt.o factqpsr.o factqury.o factrete.o factrhs.o \
 	filecom.o filertr.o fileutil.o generate.o genrcbin.o genrccmp.o \
 	genrccom.o genrcexe.o genrcfun.o genrcpsr.o globlbin.o globlbsc.o \
 	globlcmp.o globlcom.o globldef.o globlpsr.o immthpsr.o incrrset.o \
 	inherpsr.o inscom.o insfile.o insfun.o insmngr.o insmoddp.o \
 	insmult.o inspsr.o insquery.o insqypsr.o iofun.o lgcldpnd.o \
 	memalloc.o miscfun.o modulbin.o modulbsc.o modulcmp.o moduldef.o \
 	modulpsr.o modulutl.o msgcom.o msgfun.o msgpass.o msgpsr.o \
 	multifld.o multifun.o objbin.o objcmp.o objrtbin.o objrtbld.o \
 	objrtcmp.o objrtfnx.o objrtgen.o objrtmch.o parsefun.o pattern.o \
 	pprint.o prccode.o prcdrfun.o prcdrpsr.o prdctfun.o prntutil.o \
 	proflfun.o reorder.o reteutil.o retract.o router.o rulebin.o \
 	rulebld.o rulebsc.o rulecmp.o rulecom.o rulecstr.o ruledef.o \
 	ruledlt.o rulelhs.o rulepsr.o scanner.o socketrtr.o sortfun.o strngfun.o \
 	strngrtr.o symblbin.o symblcmp.o symbol.o sysdep.o \
 	tablebin.o tablebsc.o tablecmp.o tabledef.o tablepsr.o textpro.o \
 	tmpltbin.o tmpltbsc.o tmpltcmp.o tmpltdef.o tmpltfun.o tmpltlhs.o \
 	tmpltpsr.o tmpltrhs.o tmpltutl.o userdata.o userfunctions.o \
 	utility.o watch.o

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
TLS_BACKEND := $(shell if $(PKG_CONFIG) --exists libssl libcrypto; then echo openssl; elif $(PKG_CONFIG) --exists wolfssl; then echo wolfssl; elif $(PKG_CONFIG) --exists mbedtls mbedx509 mbedcrypto; then echo mbedtls; elif $(PKG_CONFIG) --exists gnutls; then echo gnutls; fi)
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

# Deprecated: the default build already leaves libmagic out. Kept so existing
# instructions and scripts keep working.
NO_IMAGE_MAGICK: release

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

.c.o :
	$(CC) -c -D$(CLIPS_OS) $(CFLAGS) $(WARNINGS) $<

clips : $(TLS_CHECK) main.o libclips.a
	$(CC) -o ../clips main.o -L. -lclips $(LDLIBS)
	
libclips.a : $(OBJS)
	rm -f $@   
	ar cq $@ $(OBJS)   

clean :
	-rm -f main.o $(OBJS)
	-rm -f socktls-openssl.o socktls-mbedtls.o socktls-gnutls.o socktls-s2n.o
	-rm -f clips libclips.a tlscheck
	-rm -f *.gcda *.gcno *.gcov *.gcov.json.gz

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

.PHONY : all clips clean debug release coverage debug_cpp release_cpp NO_IMAGE_MAGICK

# Dependencies generated using "gcc -MM *.c"

agenda.o: agenda.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h crstrtgy.h agenda.h ruledef.h network.h match.h \
  symbol.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h memalloc.h modulutl.h scanner.h \
  multifld.h prntutil.h reteutil.h rulecom.h router.h rulebsc.h \
  strngrtr.h sysdep.h watch.h
  
analysis.o: analysis.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h cstrnchk.h constrnt.h evaluatn.h cstrnutl.h cstrnops.h \
  exprnpsr.h extnfunc.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h symbol.h scanner.h generate.h analysis.h \
  reorder.h pattern.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h cstrccom.h memalloc.h modulutl.h prntutil.h \
  router.h rulecstr.h rulepsr.h watch.h
  
argacces.o: argacces.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrnchk.h constrnt.h evaluatn.h constant.h extnfunc.h expressn.h \
  exprnops.h constrct.h userdata.h moduldef.h utility.h symbol.h \
  factmngr.h conscomp.h symblcmp.h tmpltdef.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h inscom.h \
  insfun.h object.h multifld.h objrtmch.h prntutil.h router.h sysdep.h \
  argacces.h
  
bload.o: bload.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bsave.h cstrnbin.h constrnt.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h memalloc.h prntutil.h router.h bload.h \
  exprnbin.h sysdep.h symblbin.h
  
bmathfun.o: bmathfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h prntutil.h router.h bmathfun.h
  
bsave.o: bsave.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrnbin.h constrnt.h exprnpsr.h scanner.h memalloc.h \
  prntutil.h router.h bsave.h
  
classcom.o: classcom.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h argacces.h classfun.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h scanner.h classcom.h classini.h modulutl.h \
  msgcom.h msgpass.h prntutil.h router.h
  
classexm.o: classexm.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h \
  scanner.h classini.h insfun.h memalloc.h msgcom.h msgpass.h msgfun.h \
  prntutil.h router.h strngrtr.h sysdep.h classexm.h
  
classfun.o: classfun.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classini.h cstrcpsr.h strngfun.h inscom.h insfun.h \
  insmngr.h memalloc.h modulutl.h scanner.h msgfun.h msgpass.h \
  prntutil.h router.h classfun.h
  
classinf.o: classinf.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h classexm.h \
  classfun.h scanner.h classini.h memalloc.h insfun.h msgcom.h msgpass.h \
  msgfun.h prntutil.h classinf.h
  
classini.o: classini.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classexm.h classfun.h \
  scanner.h classinf.h classpsr.h cstrcpsr.h strngfun.h inscom.h \
  insfun.h memalloc.h modulpsr.h modulutl.h msgcom.h msgpass.h watch.h \
  defins.h insquery.h bload.h exprnbin.h sysdep.h symblbin.h objbin.h \
  objcmp.h objrtbld.h objrtfnx.h classini.h
  
classpsr.o: classpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h clsltpsr.h cstrcpsr.h strngfun.h \
  inherpsr.h memalloc.h modulpsr.h modulutl.h msgpsr.h pprint.h \
  prntutil.h router.h classpsr.h
  
clsltpsr.o: clsltpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnchk.h cstrnpsr.h cstrnutl.h default.h insfun.h memalloc.h \
  pprint.h prntutil.h router.h clsltpsr.h
  
commline.o: commline.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h argacces.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h cstrcpsr.h strngfun.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h fileutil.h memalloc.h multifld.h \
  pprint.h prcdrfun.h prcdrpsr.h constrnt.h prntutil.h router.h \
  strngrtr.h sysdep.h commline.h
  
conscomp.o: conscomp.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrncmp.h constrnt.h \
  exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h modulcmp.h \
  prntutil.h router.h sysdep.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h symblcmp.h dffnxcmp.h dffnxfun.h tmpltcmp.h \
  tmpltdef.h factbld.h globlcmp.h globldef.h genrccmp.h genrcfun.h \
  objcmp.h object.h multifld.h objrtmch.h
  
constrct.o: constrct.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrcpsr.h strngfun.h \
  exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h miscfun.h \
  modulutl.h multifld.h prcdrfun.h prcdrpsr.h constrnt.h prntutil.h \
  router.h ruledef.h network.h match.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h cstrccom.h sysdep.h watch.h
  
constrnt.o: constrnt.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h memalloc.h \
  multifld.h router.h scanner.h constrnt.h
  
crstrtgy.o: crstrtgy.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h argacces.h \
  memalloc.h pattern.h scanner.h reorder.h reteutil.h rulecom.h
  
cstrcbin.o: cstrcbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h
  
cstrccom.o: cstrccom.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h extnfunc.h evaluatn.h expressn.h exprnops.h constrct.h \
  userdata.h moduldef.h utility.h symbol.h memalloc.h argacces.h \
  multifld.h modulutl.h scanner.h prntutil.h router.h commline.h \
  sysdep.h bload.h exprnbin.h symblbin.h cstrcpsr.h strngfun.h \
  cstrccom.h
  
cstrcpsr.o: cstrcpsr.c setup.h envrnmnt.h entities.h usrsetup.h router.h \
  watch.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h prcdrpsr.h constrnt.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h memalloc.h modulutl.h modulpsr.h \
  pprint.h prntutil.h strngrtr.h sysdep.h cstrcpsr.h strngfun.h
  
cstrnbin.o: cstrnbin.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h memalloc.h prntutil.h router.h bload.h utility.h evaluatn.h \
  moduldef.h userdata.h extnfunc.h expressn.h exprnops.h constrct.h \
  symbol.h exprnbin.h sysdep.h symblbin.h bsave.h cstrnbin.h constrnt.h
  
cstrnchk.o: cstrnchk.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrnutl.h constrnt.h evaluatn.h constant.h extnfunc.h expressn.h \
  exprnops.h constrct.h userdata.h moduldef.h utility.h symbol.h \
  multifld.h prntutil.h router.h classcom.h cstrccom.h object.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classexm.h inscom.h insfun.h cstrnchk.h
  
cstrncmp.o: cstrncmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h conscomp.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  memalloc.h prntutil.h router.h sysdep.h cstrncmp.h constrnt.h
  
cstrnops.o: cstrnops.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h constrnt.h evaluatn.h cstrnchk.h cstrnutl.h extnfunc.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  symbol.h memalloc.h multifld.h router.h scanner.h cstrnops.h
  
cstrnpsr.o: cstrnpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h cstrnchk.h constrnt.h evaluatn.h cstrnutl.h expressn.h \
  exprnops.h constrct.h userdata.h moduldef.h utility.h memalloc.h \
  pprint.h prntutil.h router.h scanner.h sysdep.h cstrnpsr.h
  
cstrnutl.o: cstrnutl.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h memalloc.h \
  multifld.h router.h scanner.h cstrnutl.h constrnt.h
  
default.o: default.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrnt.h evaluatn.h cstrnchk.h cstrnutl.h exprnpsr.h extnfunc.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  symbol.h scanner.h factmngr.h conscomp.h symblcmp.h tmpltdef.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h inscom.h insfun.h object.h multifld.h objrtmch.h pprint.h \
  prntutil.h router.h default.h
  
defins.o: defins.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h dfinsbin.h defins.h conscomp.h symblcmp.h cstrccom.h \
  object.h constrnt.h multifld.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h objrtmch.h dfinscmp.h argacces.h classcom.h classfun.h \
  scanner.h cstrcpsr.h strngfun.h insfun.h inspsr.h memalloc.h \
  modulpsr.h modulutl.h pprint.h prntutil.h router.h
  
developr.o: developr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h factmngr.h \
  conscomp.h symblcmp.h tmpltdef.h constrnt.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h inscom.h \
  insfun.h object.h multifld.h objrtmch.h modulutl.h scanner.h \
  prntutil.h router.h factfun.h classcom.h classfun.h developr.h
  
dffctbin.o: dffctbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h dffctdef.h conscomp.h symblcmp.h cstrccom.h \
  memalloc.h dffctbin.h cstrcbin.h modulbin.h
  
dffctbsc.o: dffctbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrcpsr.h strngfun.h \
  dffctdef.h conscomp.h extnfunc.h symbol.h symblcmp.h dffctpsr.h \
  factrhs.h factmngr.h tmpltdef.h constrnt.h factbld.h network.h match.h \
  ruledef.h agenda.h crstrtgy.h facthsh.h scanner.h memalloc.h \
  multifld.h router.h dffctbin.h cstrcbin.h modulbin.h dffctcmp.h \
  dffctbsc.h
  
dffctcmp.o: dffctcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  dffctdef.h cstrccom.h dffctcmp.h
  
dffctdef.o: dffctdef.c setup.h envrnmnt.h entities.h usrsetup.h \
  dffctbsc.h dffctdef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h conscomp.h extnfunc.h expressn.h exprnops.h \
  symbol.h symblcmp.h cstrccom.h dffctpsr.h memalloc.h bload.h \
  exprnbin.h sysdep.h symblbin.h dffctbin.h cstrcbin.h modulbin.h \
  dffctcmp.h
  
dffctpsr.o: dffctpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h dffctbsc.h dffctdef.h conscomp.h \
  symblcmp.h cstrccom.h factrhs.h factmngr.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h facthsh.h \
  scanner.h memalloc.h modulutl.h pprint.h prntutil.h router.h \
  dffctpsr.h
  
dffnxbin.o: dffnxbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h cstrccom.h memalloc.h modulbin.h \
  dffnxbin.h dffnxfun.h
  
dffnxcmp.o: dffnxcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  dffnxcmp.h dffnxfun.h
  
dffnxexe.o: dffnxexe.c setup.h envrnmnt.h entities.h usrsetup.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h constant.h \
  prcdrfun.h prccode.h expressn.h exprnops.h scanner.h symbol.h \
  prntutil.h proflfun.h router.h watch.h dffnxexe.h dffnxfun.h
  
dffnxfun.o: dffnxfun.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h dffnxbin.h dffnxfun.h dffnxcmp.h cstrcpsr.h strngfun.h \
  dffnxpsr.h modulpsr.h scanner.h dffnxexe.h watch.h argacces.h \
  cstrccom.h memalloc.h modulutl.h multifld.h prntutil.h router.h
  
dffnxpsr.o: dffnxpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h constrnt.h cstrccom.h genrccom.h genrcfun.h cstrcpsr.h \
  strngfun.h dffnxfun.h exprnpsr.h scanner.h memalloc.h modulutl.h \
  pprint.h prccode.h prntutil.h router.h dffnxpsr.h
  
dfinsbin.o: dfinsbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h defins.h conscomp.h symblcmp.h \
  cstrccom.h object.h constrnt.h multifld.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h objrtmch.h memalloc.h modulbin.h dfinsbin.h
  
dfinscmp.o: dfinscmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  defins.h cstrccom.h object.h constrnt.h multifld.h match.h network.h \
  ruledef.h agenda.h crstrtgy.h objrtmch.h dfinscmp.h
  
drive.o: drive.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h engine.h \
  lgcldpnd.h retract.h factgoal.h tmpltdef.h factbld.h incrrset.h \
  memalloc.h prntutil.h reteutil.h rulecom.h router.h factmngr.h \
  facthsh.h drive.h
  
emathfun.o: emathfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h miscfun.h \
  prntutil.h router.h emathfun.h
  
engine.o: engine.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h argacces.h \
  commline.h factmngr.h tmpltdef.h factbld.h facthsh.h inscom.h insfun.h \
  object.h multifld.h objrtmch.h memalloc.h modulutl.h scanner.h \
  prccode.h prcdrfun.h prntutil.h proflfun.h reteutil.h rulecom.h \
  retract.h router.h ruledlt.h sysdep.h watch.h engine.h lgcldpnd.h
  
envrnbld.o: envrnbld.c setup.h envrnmnt.h entities.h usrsetup.h \
  bmathfun.h evaluatn.h constant.h commline.h emathfun.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h expressn.h exprnops.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  retract.h filecom.h iofun.h memalloc.h miscfun.h multifun.h parsefun.h \
  pprint.h prccode.h scanner.h prcdrfun.h prdctfun.h prntutil.h \
  proflfun.h router.h sortfun.h strngfun.h sysdep.h textpro.h watch.h \
  dffctdef.h genrccom.h genrcfun.h dffnxfun.h globldef.h tmpltdef.h \
  factbld.h classini.h object.h multifld.h objrtmch.h tabledef.h \
  envrnbld.h
  
envrnmnt.o: envrnmnt.c setup.h envrnmnt.h entities.h usrsetup.h \
  bmathfun.h evaluatn.h constant.h commline.h emathfun.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h expressn.h exprnops.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  retract.h filecom.h iofun.h memalloc.h miscfun.h multifun.h parsefun.h \
  prccode.h scanner.h prcdrfun.h prdctfun.h prntutil.h proflfun.h \
  router.h sortfun.h strngfun.h sysdep.h textpro.h watch.h dffctdef.h \
  genrccom.h genrcfun.h dffnxfun.h globldef.h tmpltdef.h factbld.h \
  classini.h object.h multifld.h objrtmch.h
  
evaluatn.o: evaluatn.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h factmngr.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h tmpltdef.h constrnt.h factbld.h \
  network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h \
  memalloc.h modulutl.h scanner.h router.h prcdrfun.h multifld.h \
  prntutil.h exprnpsr.h proflfun.h sysdep.h dffnxfun.h genrccom.h \
  genrcfun.h object.h objrtmch.h inscom.h insfun.h
  
expressn.o: expressn.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h memalloc.h prntutil.h router.h
  
exprnbin.o: exprnbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h dffctdef.h conscomp.h symblcmp.h cstrccom.h \
  memalloc.h network.h match.h ruledef.h agenda.h crstrtgy.h constrnt.h \
  genrcbin.h genrcfun.h dffnxbin.h dffnxfun.h factmngr.h tmpltdef.h \
  factbld.h facthsh.h tmpltbin.h cstrcbin.h modulbin.h globlbin.h \
  globldef.h objbin.h object.h multifld.h objrtmch.h insfun.h inscom.h
  
exprnops.o: exprnops.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrnchk.h constrnt.h evaluatn.h constant.h cstrnops.h cstrnutl.h \
  extnfunc.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h symbol.h memalloc.h prntutil.h router.h
  
exprnpsr.o: exprnpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrnchk.h constrnt.h memalloc.h \
  modulutl.h symbol.h scanner.h pprint.h prcdrfun.h prntutil.h router.h \
  strngrtr.h network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h genrccom.h genrcfun.h dffnxfun.h \
  exprnpsr.h
  
extnfunc.o: extnfunc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h factmngr.h conscomp.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h memalloc.h router.h inscom.h insfun.h object.h multifld.h \
  objrtmch.h
  
factbin.o: factbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h factmngr.h conscomp.h symblcmp.h tmpltdef.h \
  constrnt.h factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h \
  cstrccom.h facthsh.h memalloc.h pattern.h scanner.h reorder.h \
  reteutil.h rulecom.h rulebin.h cstrcbin.h modulbin.h factbin.h
  
factbld.o: factbld.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h factcmp.h network.h match.h ruledef.h symbol.h \
  agenda.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h \
  cstrccom.h pattern.h scanner.h reorder.h factgen.h factgoal.h \
  tmpltdef.h factbld.h factlhs.h factmch.h factmngr.h facthsh.h \
  memalloc.h modulutl.h reteutil.h rulecom.h retract.h router.h \
  ruledlt.h tmpltutl.h
  
factcmp.o: factcmp.c setup.h envrnmnt.h entities.h usrsetup.h factbld.h \
  network.h match.h ruledef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h expressn.h exprnops.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  factmngr.h tmpltdef.h facthsh.h factcmp.h pattern.h scanner.h \
  reorder.h
  
factcom.o: factcom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h factrhs.h scanner.h \
  multifld.h pprint.h prntutil.h router.h sysdep.h tmpltutl.h factcom.h
  
factfile.o: factfile.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h cstrcpsr.h strngfun.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h factrhs.h scanner.h insmngr.h \
  object.h multifld.h objrtmch.h inscom.h insfun.h memalloc.h modulpsr.h \
  modulutl.h prntutil.h router.h strngrtr.h tmpltutl.h factfile.h
  
factfun.o: factfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h multifld.h prntutil.h \
  router.h sysdep.h tmpltutl.h constrnt.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h facthsh.h factfun.h
  
factgen.o: factgen.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h exprnpsr.h \
  extnfunc.h expressn.h exprnops.h symbol.h scanner.h factmch.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h constrnt.h cstrccom.h factmngr.h tmpltdef.h facthsh.h \
  factprt.h factrete.h memalloc.h pattern.h reorder.h prcdrpsr.h \
  reteutil.h rulecom.h router.h sysdep.h tmpltfun.h tmpltlhs.h \
  tmpltutl.h factgen.h
  
factgoal.o: factgoal.c setup.h envrnmnt.h entities.h usrsetup.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h constant.h expressn.h exprnops.h \
  symbol.h agenda.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h \
  constrnt.h cstrccom.h retract.h factmngr.h tmpltdef.h factbld.h \
  facthsh.h factgen.h reorder.h pattern.h scanner.h memalloc.h \
  multifld.h prntutil.h reteutil.h rulecom.h router.h factgoal.h
  
facthsh.o: facthsh.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  factmngr.h conscomp.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h facthsh.h memalloc.h multifld.h router.h \
  sysdep.h lgcldpnd.h
  
factlhs.o: factlhs.c setup.h envrnmnt.h entities.h usrsetup.h cstrcpsr.h \
  strngfun.h modulpsr.h evaluatn.h constant.h moduldef.h userdata.h \
  utility.h symbol.h scanner.h modulutl.h pattern.h expressn.h \
  exprnops.h constrct.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h reorder.h \
  pprint.h prntutil.h router.h tmpltdef.h factbld.h tmpltlhs.h \
  tmpltpsr.h tmpltutl.h factmngr.h facthsh.h factlhs.h
  
factmch.o: factmch.c setup.h envrnmnt.h entities.h usrsetup.h drive.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h match.h network.h ruledef.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h factgen.h reorder.h pattern.h scanner.h \
  factrete.h incrrset.h memalloc.h prntutil.h reteutil.h rulecom.h \
  router.h sysdep.h tmpltdef.h factbld.h factmch.h factmngr.h facthsh.h
  
factmngr.o: factmngr.c setup.h envrnmnt.h entities.h usrsetup.h \
  commline.h default.h constrnt.h evaluatn.h constant.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h expressn.h exprnops.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h cstrccom.h retract.h \
  factbin.h factbld.h factcmp.h pattern.h scanner.h reorder.h factcom.h \
  factfile.h factfun.h factmngr.h tmpltdef.h facthsh.h factgen.h \
  factgoal.h factmch.h factqury.h factrhs.h memalloc.h multifld.h \
  reteutil.h rulecom.h prntutil.h router.h strngrtr.h sysdep.h \
  tmpltbsc.h tmpltfun.h tmpltutl.h watch.h cstrnchk.h
  
factprt.o: factprt.c setup.h envrnmnt.h entities.h usrsetup.h factgen.h \
  reorder.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h pattern.h symbol.h scanner.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h prntutil.h router.h factprt.h
  
factqpsr.o: factqpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  exprnpsr.h extnfunc.h evaluatn.h constant.h expressn.h exprnops.h \
  constrct.h userdata.h moduldef.h utility.h symbol.h scanner.h \
  factqury.h factmngr.h conscomp.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h modulutl.h prcdrpsr.h pprint.h prntutil.h router.h \
  strngrtr.h factqpsr.h
  
factqury.o: factqury.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h memalloc.h exprnpsr.h extnfunc.h \
  symbol.h scanner.h modulutl.h tmpltutl.h constrnt.h factmngr.h \
  conscomp.h symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h insfun.h object.h multifld.h \
  objrtmch.h factqpsr.h prcdrfun.h prntutil.h router.h factqury.h
  
factrete.o: factrete.c setup.h envrnmnt.h entities.h usrsetup.h drive.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h match.h network.h ruledef.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h factgen.h reorder.h pattern.h scanner.h \
  factmch.h factbld.h factmngr.h tmpltdef.h facthsh.h factgoal.h \
  incrrset.h memalloc.h multifld.h reteutil.h rulecom.h router.h \
  factrete.h
  
factrhs.o: factrhs.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h exprnpsr.h scanner.h modulutl.h \
  modulpsr.h pattern.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h constrnt.h cstrccom.h reorder.h pprint.h \
  prntutil.h router.h strngrtr.h tmpltpsr.h tmpltdef.h factbld.h \
  tmpltrhs.h tmpltutl.h factmngr.h facthsh.h factrhs.h
  
filecom.o: filecom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h commline.h cstrcpsr.h strngfun.h fileutil.h \
  memalloc.h router.h filecom.h
  
filertr.o: filertr.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  memalloc.h router.h sysdep.h filertr.h
  
fileutil.o: fileutil.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrcpsr.h strngfun.h \
  memalloc.h prcdrfun.h pprint.h prntutil.h router.h scanner.h \
  strngrtr.h sysdep.h filecom.h fileutil.h
  
generate.o: generate.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h globlpsr.h memalloc.h pattern.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h conscomp.h symblcmp.h constrnt.h cstrccom.h \
  reorder.h prntutil.h router.h generate.h analysis.h
  
genrcbin.o: genrcbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h cstrccom.h genrccom.h genrcfun.h \
  conscomp.h symblcmp.h memalloc.h modulbin.h objbin.h object.h \
  constrnt.h multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  objrtmch.h router.h genrcbin.h
  
genrccmp.o: genrccmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  genrccom.h genrcfun.h network.h match.h ruledef.h agenda.h crstrtgy.h \
  constrnt.h cstrccom.h objcmp.h object.h multifld.h objrtmch.h \
  genrccmp.h
  
genrccom.o: genrccom.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h objrtmch.h inscom.h insfun.h cstrcpsr.h strngfun.h \
  genrcbin.h genrcfun.h genrccmp.h genrcexe.h genrcpsr.h memalloc.h \
  modulpsr.h scanner.h modulutl.h router.h strngrtr.h watch.h prntutil.h \
  genrccom.h
  
genrcexe.o: genrcexe.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  insfun.h argacces.h genrccom.h genrcfun.h prcdrfun.h prccode.h \
  prntutil.h proflfun.h router.h genrcexe.h
  
genrcfun.o: genrcfun.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h argacces.h cstrcpsr.h strngfun.h \
  genrccom.h genrcfun.h genrcexe.h memalloc.h modulutl.h prccode.h \
  prntutil.h router.h
  
genrcpsr.o: genrcpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h dffnxfun.h classfun.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h scanner.h classcom.h cstrcpsr.h strngfun.h \
  exprnpsr.h genrccom.h genrcfun.h immthpsr.h memalloc.h modulutl.h \
  pprint.h prcdrpsr.h prccode.h prntutil.h router.h genrcpsr.h
  
globlbin.o: globlbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h globlbsc.h globldef.h conscomp.h symblcmp.h \
  cstrccom.h memalloc.h multifld.h globlbin.h modulbin.h cstrcbin.h
  
globlbsc.o: globlbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h constant.h \
  extnfunc.h expressn.h exprnops.h symbol.h globlbin.h modulbin.h \
  cstrcbin.h globldef.h conscomp.h symblcmp.h cstrccom.h globlcmp.h \
  globlcom.h multifld.h watch.h globlbsc.h
  
globlcmp.o: globlcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  globldef.h cstrccom.h globlcmp.h
  
globlcom.o: globlcom.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h globldef.h \
  conscomp.h symblcmp.h cstrccom.h prntutil.h router.h globlcom.h
  
globldef.o: globldef.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h globlbin.h modulbin.h cstrcbin.h globldef.h conscomp.h \
  symblcmp.h cstrccom.h commline.h globlbsc.h globlcmp.h globlcom.h \
  globlpsr.h memalloc.h modulpsr.h scanner.h modulutl.h multifld.h \
  prntutil.h router.h strngrtr.h
  
globlpsr.o: globlpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h exprnpsr.h scanner.h globlbsc.h \
  globldef.h conscomp.h symblcmp.h cstrccom.h memalloc.h modulpsr.h \
  modulutl.h multifld.h pprint.h prntutil.h router.h watch.h globlpsr.h
  
immthpsr.o: immthpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnutl.h exprnpsr.h genrcpsr.h genrcfun.h memalloc.h prccode.h \
  immthpsr.h
  
incrrset.o: incrrset.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h argacces.h \
  drive.h engine.h lgcldpnd.h retract.h factgoal.h tmpltdef.h factbld.h \
  pattern.h scanner.h reorder.h router.h reteutil.h rulecom.h incrrset.h
  
inherpsr.o: inherpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  memalloc.h modulutl.h pprint.h prntutil.h router.h inherpsr.h
  
inscom.o: inscom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  classinf.h commline.h exprnpsr.h insfile.h insfun.h insmngr.h inscom.h \
  insmoddp.h insmult.h inspsr.h lgcldpnd.h memalloc.h msgcom.h msgpass.h \
  msgfun.h prntutil.h router.h strngrtr.h sysdep.h
  
insfile.o: insfile.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  memalloc.h factmngr.h tmpltdef.h factbld.h facthsh.h inscom.h insfun.h \
  insmngr.h inspsr.h prntutil.h router.h strngrtr.h symblbin.h sysdep.h \
  insfile.h
  
insfun.o: insfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnchk.h drive.h engine.h lgcldpnd.h retract.h inscom.h insfun.h \
  insmngr.h memalloc.h modulutl.h msgcom.h msgpass.h msgfun.h prccode.h \
  prntutil.h router.h
  
insmngr.o: insmngr.c setup.h envrnmnt.h entities.h usrsetup.h network.h \
  match.h ruledef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h expressn.h exprnops.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  drive.h objrtmch.h object.h multifld.h lgcldpnd.h objrtfnx.h \
  classcom.h classfun.h scanner.h cstrnchk.h engine.h retract.h insfun.h \
  memalloc.h miscfun.h modulutl.h msgcom.h msgpass.h msgfun.h prccode.h \
  prntutil.h router.h sysdep.h insmngr.h inscom.h watch.h
  
insmoddp.o: insmoddp.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h inscom.h insfun.h \
  object.h constrnt.h multifld.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h symblcmp.h cstrccom.h objrtmch.h insmngr.h \
  inspsr.h memalloc.h miscfun.h msgcom.h msgpass.h msgfun.h prccode.h \
  scanner.h prntutil.h router.h insmoddp.h
  
insmult.o: insmult.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h insfun.h object.h constrnt.h \
  multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h cstrccom.h objrtmch.h msgfun.h msgpass.h multifun.h \
  prntutil.h router.h insmult.h
  
inspsr.o: inspsr.c setup.h envrnmnt.h entities.h usrsetup.h classcom.h \
  cstrccom.h moduldef.h userdata.h utility.h evaluatn.h constant.h \
  constrct.h object.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h classinf.h \
  exprnpsr.h pprint.h prntutil.h router.h inspsr.h
  
insquery.o: insquery.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h \
  scanner.h exprnpsr.h insfun.h insmngr.h inscom.h insqypsr.h memalloc.h \
  prcdrfun.h prntutil.h router.h insquery.h
  
insqypsr.o: insqypsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h exprnpsr.h scanner.h \
  insquery.h prcdrpsr.h pprint.h prntutil.h router.h strngrtr.h \
  insqypsr.h
  
iofun.o: iofun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h commline.h extnfunc.h symbol.h filertr.h \
  memalloc.h miscfun.h prntutil.h router.h scanner.h strngrtr.h sysdep.h \
  iofun.h
  
lgcldpnd.o: lgcldpnd.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h engine.h lgcldpnd.h match.h network.h \
  ruledef.h symbol.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h retract.h factmngr.h tmpltdef.h \
  factbld.h facthsh.h memalloc.h pattern.h scanner.h reorder.h \
  reteutil.h rulecom.h router.h
  
main.o: main.c clips.h setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h memalloc.h cstrcpsr.h strngfun.h \
  fileutil.h envrnbld.h extnfunc.h symbol.h commline.h prntutil.h \
  router.h filertr.h strngrtr.h iofun.h sysdep.h bmathfun.h exprnpsr.h \
  scanner.h miscfun.h watch.h modulbsc.h bload.h exprnbin.h symblbin.h \
  bsave.h ruledef.h network.h match.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h constrnt.h cstrccom.h rulebsc.h engine.h lgcldpnd.h \
  retract.h drive.h incrrset.h rulecom.h dffctdef.h dffctbsc.h \
  tmpltdef.h factbld.h tmpltbsc.h tmpltfun.h factmngr.h facthsh.h \
  reorder.h pattern.h factcom.h factfile.h factfun.h globldef.h \
  globlbsc.h globlcom.h dffnxfun.h genrccom.h genrcfun.h classcom.h \
  object.h multifld.h objrtmch.h classexm.h classfun.h classinf.h \
  classini.h classpsr.h defins.h inscom.h insfun.h insfile.h insmngr.h \
  msgcom.h msgpass.h
  
memalloc.o: memalloc.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h memalloc.h prntutil.h router.h utility.h evaluatn.h \
  moduldef.h userdata.h
  
miscfun.o: miscfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h scanner.h \
  memalloc.h multifld.h prntutil.h router.h sysdep.h dffnxfun.h \
  factfun.h factmngr.h conscomp.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h tmpltutl.h miscfun.h
  
modulbin.o: modulbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h memalloc.h modulbin.h
  
modulbsc.o: modulbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h modulbin.h cstrcbin.h modulcmp.h multifld.h \
  prntutil.h router.h modulbsc.h
  
modulcmp.o: modulcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  sysdep.h modulcmp.h
  
moduldef.o: moduldef.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h modulbin.h cstrcbin.h memalloc.h modulbsc.h \
  modulcmp.h modulpsr.h scanner.h prntutil.h router.h
  
modulpsr.o: modulpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrcpsr.h strngfun.h extnfunc.h \
  symbol.h memalloc.h modulutl.h scanner.h pprint.h prntutil.h router.h \
  bload.h exprnbin.h sysdep.h symblbin.h modulpsr.h
  
modulutl.o: modulutl.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrcpsr.h strngfun.h memalloc.h modulpsr.h evaluatn.h constant.h \
  moduldef.h userdata.h utility.h symbol.h scanner.h pprint.h prntutil.h \
  router.h sysdep.h watch.h expressn.h exprnops.h constrct.h modulutl.h
  
msgcom.o: msgcom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h classinf.h msgpsr.h insfun.h \
  insmoddp.h msgfun.h msgpass.h memalloc.h prccode.h prntutil.h router.h \
  watch.h msgcom.h
  
msgfun.o: msgfun.c setup.h envrnmnt.h entities.h usrsetup.h classcom.h \
  cstrccom.h moduldef.h userdata.h utility.h evaluatn.h constant.h \
  constrct.h object.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h inscom.h \
  insfun.h memalloc.h msgcom.h msgpass.h prccode.h prntutil.h router.h \
  msgfun.h
  
msgpass.o: msgpass.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  commline.h exprnpsr.h inscom.h insfun.h memalloc.h msgcom.h msgpass.h \
  msgfun.h prccode.h prcdrfun.h prntutil.h proflfun.h router.h \
  strngfun.h
  
msgpsr.o: msgpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h cstrcpsr.h strngfun.h cstrnchk.h \
  exprnpsr.h insfun.h memalloc.h modulutl.h msgcom.h msgpass.h msgfun.h \
  pprint.h prccode.h prntutil.h router.h strngrtr.h msgpsr.h
  
multifld.o: multifld.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h evaluatn.h exprnops.h expressn.h constrct.h userdata.h \
  moduldef.h utility.h memalloc.h object.h constrnt.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h objrtmch.h scanner.h prntutil.h \
  router.h strngrtr.h
  
multifun.o: multifun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h memalloc.h multifld.h multifun.h object.h constrnt.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h pprint.h prcdrpsr.h prcdrfun.h prntutil.h \
  router.h
  
objbin.o: objbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h objrtmch.h classfun.h scanner.h classini.h cstrcbin.h \
  cstrnbin.h insfun.h memalloc.h modulbin.h msgcom.h msgpass.h msgfun.h \
  prntutil.h router.h objrtbin.h objbin.h
  
objcmp.o: objcmp.c setup.h envrnmnt.h entities.h usrsetup.h conscomp.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h constant.h \
  extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h classcom.h \
  cstrccom.h object.h constrnt.h multifld.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h objrtmch.h classfun.h scanner.h classini.h \
  cstrncmp.h objrtfnx.h sysdep.h objrtcmp.h objcmp.h
  
objrtbin.o: objrtbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h classfun.h object.h constrnt.h multifld.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h scanner.h classcom.h memalloc.h insfun.h \
  pattern.h reorder.h reteutil.h rulecom.h rulebin.h cstrcbin.h \
  modulbin.h objrtbin.h
  
objrtbld.o: objrtbld.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnutl.h cstrnchk.h cstrnops.h drive.h inscom.h insfun.h insmngr.h \
  memalloc.h pattern.h reorder.h prntutil.h reteutil.h rulecom.h \
  rulepsr.h exprnpsr.h objrtgen.h objrtfnx.h pprint.h router.h \
  objrtbin.h objrtcmp.h objrtbld.h
  
objrtcmp.o: objrtcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  classcom.h cstrccom.h object.h constrnt.h multifld.h match.h network.h \
  ruledef.h agenda.h crstrtgy.h objrtmch.h objrtfnx.h pattern.h \
  scanner.h reorder.h sysdep.h objrtcmp.h
  
objrtfnx.o: objrtfnx.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  bload.h exprnbin.h sysdep.h symblbin.h drive.h engine.h lgcldpnd.h \
  retract.h memalloc.h prntutil.h reteutil.h rulecom.h router.h \
  objrtfnx.h
  
objrtgen.o: objrtgen.c setup.h envrnmnt.h entities.h usrsetup.h \
  classfun.h object.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h objrtmch.h scanner.h classcom.h \
  objrtfnx.h objrtgen.h reorder.h pattern.h
  
objrtmch.o: objrtmch.c setup.h envrnmnt.h entities.h usrsetup.h \
  classfun.h object.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h objrtmch.h scanner.h classcom.h \
  memalloc.h drive.h engine.h lgcldpnd.h retract.h incrrset.h objrtfnx.h \
  prntutil.h reteutil.h rulecom.h ruledlt.h reorder.h pattern.h router.h \
  insmngr.h inscom.h insfun.h
  
parsefun.o: parsefun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrcpsr.h strngfun.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h memalloc.h multifld.h pprint.h \
  prcdrpsr.h constrnt.h prntutil.h router.h strngrtr.h parsefun.h
  
pattern.o: pattern.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrnt.h evaluatn.h cstrnchk.h cstrnutl.h exprnpsr.h extnfunc.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  symbol.h scanner.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h cstrccom.h memalloc.h pprint.h prntutil.h \
  reteutil.h rulecom.h router.h rulecmp.h pattern.h reorder.h
  
pprint.o: pprint.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  memalloc.h sysdep.h utility.h evaluatn.h moduldef.h userdata.h \
  pprint.h
  
prccode.o: prccode.c setup.h envrnmnt.h entities.h usrsetup.h memalloc.h \
  constant.h globlpsr.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h multifld.h object.h constrnt.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h conscomp.h symblcmp.h cstrccom.h objrtmch.h \
  pprint.h prcdrpsr.h prntutil.h router.h prccode.h
  
prcdrfun.o: prcdrfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h constrnt.h cstrnchk.h cstrnops.h \
  exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h multifld.h \
  prcdrpsr.h router.h prcdrfun.h globldef.h conscomp.h symblcmp.h \
  cstrccom.h
  
prcdrpsr.o: prcdrpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h constrnt.h cstrnchk.h cstrnops.h \
  cstrnutl.h exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h \
  modulutl.h multifld.h pprint.h prntutil.h router.h prcdrpsr.h \
  globldef.h conscomp.h symblcmp.h cstrccom.h globlpsr.h
  
prdctfun.o: prdctfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h multifld.h router.h prdctfun.h
  
prntutil.o: prntutil.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrcpsr.h strngfun.h factmngr.h \
  conscomp.h extnfunc.h symbol.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h inscom.h insfun.h object.h multifld.h objrtmch.h insmngr.h \
  memalloc.h multifun.h router.h scanner.h strngrtr.h sysdep.h \
  prntutil.h
  
proflfun.o: proflfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h dffnxfun.h \
  genrccom.h genrcfun.h memalloc.h msgcom.h msgpass.h router.h sysdep.h \
  proflfun.h
  
reorder.o: reorder.c setup.h envrnmnt.h entities.h usrsetup.h cstrnutl.h \
  constrnt.h evaluatn.h constant.h extnfunc.h expressn.h exprnops.h \
  constrct.h userdata.h moduldef.h utility.h symbol.h memalloc.h \
  pattern.h scanner.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h cstrccom.h reorder.h prntutil.h router.h \
  rulelhs.h
  
reteutil.o: reteutil.c setup.h envrnmnt.h entities.h usrsetup.h drive.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h match.h network.h ruledef.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h incrrset.h memalloc.h pattern.h \
  scanner.h reorder.h prntutil.h router.h rulecom.h reteutil.h
  
retract.o: retract.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h argacces.h \
  drive.h engine.h lgcldpnd.h retract.h factgoal.h tmpltdef.h factbld.h \
  memalloc.h prntutil.h reteutil.h rulecom.h router.h factmngr.h \
  facthsh.h
  
router.o: router.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h filertr.h memalloc.h \
  prntutil.h scanner.h strngrtr.h sysdep.h router.h
  
rulebin.o: rulebin.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h bload.h \
  exprnbin.h sysdep.h symblbin.h bsave.h engine.h lgcldpnd.h retract.h \
  memalloc.h pattern.h scanner.h reorder.h reteutil.h rulecom.h \
  rulebsc.h rulebin.h cstrcbin.h modulbin.h
  
rulebld.o: rulebld.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h drive.h \
  expressn.h exprnops.h match.h network.h ruledef.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  incrrset.h memalloc.h pattern.h scanner.h reorder.h prntutil.h \
  reteutil.h rulecom.h router.h rulebld.h rulepsr.h watch.h
  
rulebsc.o: rulebsc.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h drive.h match.h network.h ruledef.h symbol.h \
  agenda.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h \
  cstrccom.h engine.h lgcldpnd.h retract.h factgoal.h tmpltdef.h \
  factbld.h multifld.h reteutil.h rulecom.h router.h watch.h factmngr.h \
  facthsh.h rulebin.h cstrcbin.h modulbin.h rulecmp.h rulebsc.h
  
rulecmp.o: rulecmp.c setup.h envrnmnt.h entities.h usrsetup.h factbld.h \
  network.h match.h ruledef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h expressn.h exprnops.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  pattern.h scanner.h reorder.h reteutil.h rulecom.h rulecmp.h
  
rulecom.o: rulecom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h crstrtgy.h agenda.h ruledef.h network.h match.h \
  symbol.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h incrrset.h memalloc.h multifld.h \
  pattern.h scanner.h reorder.h prntutil.h reteutil.h rulecom.h router.h \
  ruledlt.h sysdep.h watch.h factmngr.h tmpltdef.h factbld.h facthsh.h \
  rulebin.h cstrcbin.h modulbin.h
  
rulecstr.o: rulecstr.c setup.h envrnmnt.h entities.h usrsetup.h \
  analysis.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h reorder.h pattern.h symbol.h scanner.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h cstrnchk.h cstrnops.h cstrnutl.h \
  prcdrpsr.h prntutil.h router.h rulepsr.h rulecstr.h
  
ruledef.o: ruledef.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h drive.h \
  engine.h lgcldpnd.h retract.h memalloc.h pattern.h scanner.h reorder.h \
  reteutil.h rulecom.h rulebsc.h rulepsr.h ruledlt.h bload.h exprnbin.h \
  sysdep.h symblbin.h rulebin.h cstrcbin.h modulbin.h rulecmp.h
  
ruledlt.o: ruledlt.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h bload.h \
  exprnbin.h sysdep.h symblbin.h drive.h engine.h lgcldpnd.h retract.h \
  factgoal.h tmpltdef.h factbld.h memalloc.h pattern.h scanner.h \
  reorder.h reteutil.h rulecom.h ruledlt.h
  
rulelhs.o: rulelhs.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h symbol.h conscomp.h \
  extnfunc.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h argacces.h \
  cstrnchk.h exprnpsr.h scanner.h memalloc.h pattern.h reorder.h \
  pprint.h prntutil.h router.h rulelhs.h
  
rulepsr.o: rulepsr.c setup.h envrnmnt.h entities.h usrsetup.h analysis.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h reorder.h pattern.h symbol.h scanner.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h cstrcpsr.h strngfun.h cstrnchk.h \
  cstrnops.h engine.h lgcldpnd.h retract.h exprnpsr.h incrrset.h \
  memalloc.h modulutl.h prccode.h prcdrpsr.h pprint.h prntutil.h \
  router.h rulebld.h rulebsc.h rulecstr.h ruledlt.h rulelhs.h watch.h \
  tmpltfun.h factmngr.h tmpltdef.h factbld.h facthsh.h bload.h \
  exprnbin.h sysdep.h symblbin.h rulepsr.h
  
scanner.o: scanner.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  memalloc.h pprint.h prntutil.h router.h symbol.h sysdep.h utility.h \
  evaluatn.h moduldef.h userdata.h scanner.h
  
sortfun.o: sortfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h dffnxfun.h extnfunc.h symbol.h memalloc.h \
  multifld.h sysdep.h sortfun.h
  
strngfun.o: strngfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrcpsr.h strngfun.h \
  engine.h lgcldpnd.h match.h network.h ruledef.h symbol.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  retract.h exprnpsr.h scanner.h memalloc.h miscfun.h multifld.h \
  prcdrpsr.h pprint.h prntutil.h router.h strngrtr.h sysdep.h drive.h
  
strngrtr.o: strngrtr.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h memalloc.h prntutil.h router.h sysdep.h strngrtr.h \
  utility.h evaluatn.h moduldef.h userdata.h
  
symblbin.o: symblbin.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h bsave.h cstrnbin.h constrnt.h exprnpsr.h scanner.h \
  memalloc.h router.h
  
symblcmp.o: symblcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h conscomp.h extnfunc.h symbol.h \
  symblcmp.h cstrncmp.h constrnt.h cstrccom.h exprnpsr.h scanner.h \
  memalloc.h prntutil.h router.h sysdep.h
  
symbol.o: symbol.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h memalloc.h multifld.h prntutil.h router.h \
  sysdep.h symbol.h
  
sysdep.o: sysdep.c setup.h envrnmnt.h entities.h usrsetup.h memalloc.h \
  sysdep.h
  
tablebin.o: tablebin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h memalloc.h tabledef.h cstrccom.h tablebin.h \
  cstrcbin.h modulbin.h
  
tablebsc.o: tablebsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrcpsr.h strngfun.h \
  extnfunc.h symbol.h memalloc.h multifld.h prntutil.h router.h \
  scanner.h tabledef.h tablebin.h cstrcbin.h modulbin.h tablecmp.h \
  tablebsc.h
  
tablecmp.o: tablecmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  tabledef.h cstrccom.h tablecmp.h
  
tabledef.o: tabledef.c setup.h envrnmnt.h entities.h usrsetup.h \
  memalloc.h symbol.h tablebsc.h tabledef.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h constant.h cstrccom.h tablepsr.h \
  bload.h extnfunc.h expressn.h exprnops.h exprnbin.h sysdep.h \
  symblbin.h tablebin.h cstrcbin.h modulbin.h tablecmp.h
  
tablepsr.o: tablepsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h memalloc.h modulutl.h scanner.h \
  pprint.h prntutil.h router.h tablebsc.h tabledef.h cstrccom.h \
  tablepsr.h
  
textpro.o: textpro.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h commline.h extnfunc.h symbol.h memalloc.h \
  prntutil.h router.h sysdep.h textpro.h
  
tmpltbin.o: tmpltbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrnbin.h constrnt.h factbin.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h cstrccom.h \
  factmngr.h tmpltdef.h facthsh.h memalloc.h tmpltpsr.h tmpltutl.h \
  tmpltbin.h cstrcbin.h modulbin.h
  
tmpltbsc.o: tmpltbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrcpsr.h strngfun.h \
  extnfunc.h symbol.h factrhs.h factmngr.h conscomp.h symblcmp.h \
  tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h facthsh.h scanner.h memalloc.h multifld.h router.h \
  tmpltbin.h cstrcbin.h modulbin.h tmpltcmp.h tmpltpsr.h tmpltutl.h \
  tmpltbsc.h
  
tmpltcmp.o: tmpltcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  cstrncmp.h constrnt.h factcmp.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h pattern.h scanner.h reorder.h tmpltdef.h \
  factbld.h tmpltcmp.h
  
tmpltdef.o: tmpltdef.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrccom.h moduldef.h userdata.h utility.h evaluatn.h constant.h \
  constrct.h cstrnchk.h constrnt.h exprnops.h expressn.h memalloc.h \
  modulpsr.h symbol.h scanner.h modulutl.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h pattern.h \
  reorder.h router.h tmpltbsc.h tmpltdef.h factbld.h tmpltfun.h \
  factmngr.h facthsh.h tmpltpsr.h tmpltutl.h bload.h exprnbin.h sysdep.h \
  symblbin.h tmpltbin.h cstrcbin.h modulbin.h tmpltcmp.h
  
tmpltfun.o: tmpltfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrnchk.h constrnt.h \
  default.h exprnpsr.h extnfunc.h symbol.h scanner.h factmch.h factbld.h \
  network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h factmngr.h tmpltdef.h facthsh.h factrhs.h memalloc.h \
  modulutl.h multifld.h pprint.h prcdrpsr.h prntutil.h reorder.h \
  pattern.h retract.h router.h sysdep.h tmpltlhs.h tmpltrhs.h tmpltutl.h \
  tmpltfun.h
  
tmpltlhs.o: tmpltlhs.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constrnt.h exprnpsr.h extnfunc.h expressn.h exprnops.h symbol.h \
  scanner.h factrhs.h factmngr.h conscomp.h symblcmp.h tmpltdef.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h memalloc.h modulutl.h pattern.h reorder.h pprint.h \
  prntutil.h router.h tmpltutl.h tmpltlhs.h
  
tmpltpsr.o: tmpltpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h cstrnchk.h constrnt.h cstrnpsr.h \
  cstrnutl.h default.h exprnpsr.h scanner.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h facthsh.h memalloc.h modulutl.h pattern.h \
  reorder.h pprint.h prntutil.h router.h tmpltbsc.h watch.h tmpltpsr.h
  
tmpltrhs.o: tmpltrhs.c setup.h envrnmnt.h entities.h usrsetup.h default.h \
  constrnt.h evaluatn.h constant.h extnfunc.h expressn.h exprnops.h \
  constrct.h userdata.h moduldef.h utility.h symbol.h factrhs.h \
  factmngr.h conscomp.h symblcmp.h tmpltdef.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h scanner.h \
  memalloc.h modulutl.h pprint.h prntutil.h router.h tmpltfun.h \
  reorder.h pattern.h tmpltlhs.h tmpltutl.h tmpltrhs.h
  
tmpltutl.o: tmpltutl.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrnchk.h constrnt.h extnfunc.h \
  symbol.h memalloc.h modulutl.h scanner.h multifld.h prntutil.h \
  router.h sysdep.h tmpltbsc.h tmpltdef.h conscomp.h symblcmp.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  tmpltfun.h factmngr.h facthsh.h reorder.h pattern.h tmpltpsr.h watch.h \
  tmpltutl.h
  
userdata.o: userdata.c setup.h envrnmnt.h entities.h usrsetup.h \
  userdata.h
  
userfunctions.o: userfunctions.c clips.h setup.h envrnmnt.h entities.h \
  usrsetup.h argacces.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h constant.h memalloc.h cstrcpsr.h \
  strngfun.h fileutil.h envrnbld.h extnfunc.h symbol.h commline.h \
  prntutil.h router.h filertr.h strngrtr.h iofun.h sysdep.h bmathfun.h \
  exprnpsr.h scanner.h miscfun.h watch.h modulbsc.h bload.h exprnbin.h \
  symblbin.h bsave.h ruledef.h network.h match.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h constrnt.h cstrccom.h rulebsc.h engine.h \
  lgcldpnd.h retract.h drive.h incrrset.h rulecom.h dffctdef.h \
  dffctbsc.h tmpltdef.h factbld.h tmpltbsc.h tmpltfun.h factmngr.h \
  facthsh.h reorder.h pattern.h factcom.h factfile.h factfun.h \
  globldef.h globlbsc.h globlcom.h dffnxfun.h genrccom.h genrcfun.h \
  classcom.h object.h multifld.h objrtmch.h classexm.h classfun.h \
  classinf.h classini.h classpsr.h defins.h inscom.h insfun.h insfile.h \
  insmngr.h msgcom.h msgpass.h
  
utility.o: utility.c setup.h envrnmnt.h entities.h usrsetup.h commline.h \
  evaluatn.h constant.h factmngr.h conscomp.h constrct.h userdata.h \
  moduldef.h utility.h extnfunc.h expressn.h exprnops.h symbol.h \
  symblcmp.h tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h memalloc.h multifld.h \
  prntutil.h router.h sysdep.h
  
watch.o: watch.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h memalloc.h router.h watch.h
