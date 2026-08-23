#!/bin/sh
# What the test matrix can build on this machine, and how to get the libraries
# that it cannot build.
#
#   ./tests/provision.sh              say what is here and what is missing
#   ./tests/provision.sh --build      build every missing one that needs no root
#   ./tests/provision.sh --build s2n  build just that one
#   ./tests/provision.sh --remove     delete everything it built
#   ./tests/provision.sh --remove s2n delete one of them
#   make provision                    the first of those
#
# This script never runs sudo and never installs a system package. It builds
# each library that it knows from source into $HOME/opt. There the library
# hides no other file, and `rm -rf` removes it. If you want a library from your
# package manager, install it yourself and this script finds it there.
#
# "system" is the one exception, and this script cannot supply it. It means a
# build with no flags: the libssl and libcrypto that the compiler finds itself.
# A library under $HOME/opt is not that library. As a result, a build of
# OpenSSL here gives an "openssl" configuration and does not change "system".
#
# The build instructions are as important as the automation. Each of them took
# much work, and the notes are the important part:
#
#   LibreSSL   supplies libssl and libcrypto with the names of OpenSSL. As a
#              result, it cannot go in a default linker directory without a
#              replacement of OpenSSL. Its openssldir stays at the default,
#              inside its own prefix. LibreSSL supplies a CA bundle and
#              installs it there. The trust store is then full, and the path
#              in the library is the path of the bundle. With openssldir at
#              /etc/ssl, "make install" tries to write there and needs root.
#
#   BoringSSL  no longer needs Go, although its documentation says so. It
#              builds static libraries only. For that cause the suite asks
#              the binary what it supports and does not read ldd.
#
#   OpenSSL    comes from a clone and not from a tarball. Its Configure is a
#              Perl script in the tree, and there is no bootstrap step
#              to miss. A commit is also sufficient verification. Its
#              submodules are all test data: krb5, wycheproof and quiche.
#              This script does not fetch them.
#
#   s2n-tls    links a libcrypto and needs one to build. The libcrypto of
#              OpenSSL is correct, and the libcrypto of LibreSSL is also
#              correct.
#
#   mbedTLS 3  is of value with 2.x, because the two differ on TLS 1.3 and
#              the suite checks that. Part of its build system is in a git
#              submodule, and it does not configure without that submodule.
#
#   mbedTLS 4  is the other line with support. Its support ends in March
#              2029, and the support of 3.6 ends in March 2027. It moved the
#              random generator into PSA Crypto. The entropy and DRBG
#              contexts are gone, and the generator arguments of several
#              calls are gone with them. As a result, the 2.x argument lists
#              of the backend also serve 4.x. Its cryptography is in the
#              tf-psa-crypto submodule. It also makes part of itself at build
#              time with a Python script, and that script needs jsonschema
#              and jinja2.
#
#   mbedTLS 2  stays here on purpose. Upstream archived the 2.28 branch, and
#              that is not a cause to remove it. Ubuntu 24.04 supplies 2.28.8
#              with support into 2029, Debian 12 supplies 2.28.3, and Ubuntu
#              22.04 supplies 2.28.0. A user of this library links against
#              those versions for years, and this suite tests them.
#              Remove this line when the distributions remove the versions.
#
#   wolfSSL    needs WOLFSSL_OPENSSLALL and not only OPENSSLEXTRA. This
#              backend calls SSL_set_mode, which maps to wolfSSL_ctrl, and
#              only the larger compatibility layer exports that call. A build
#              with OPENSSLEXTRA alone compiles and then fails at link time.
#
#   GnuTLS     needs nettle, gmp and libtasn1. It does not build without
#              them, and this script does not build them. You can tell it to
#              include its own copy of unistring. Note that the result is not
#              complete in itself: its .pc file names those libraries, and
#              they must stay available.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1

BUILD=0
REMOVE=0
case ${1:-} in
	--build)  BUILD=1;  shift ;;
	--remove) REMOVE=1; shift ;;
	"") ;;
	*) echo "provision: unknown option $1" >&2; exit 2 ;;
esac

OPT=${TLS_OPT_DIR:-$HOME/opt}
SRC=${TLS_SRC_DIR:-$HOME/opt/src}

# The expected content of each download. HTTPS gives the identity of the
# server and not the content of the file. Without these digests, the script
# would accept a file from a compromised mirror, or from a tag that someone
# moved.
#
# The OpenSSL and LibreSSL digests come from upstream. They are in the checksum
# files that upstream publishes with the releases, and they agree with the
# files that arrived here. GnuTLS publishes no digest and no signature at its
# download location. As a result, the GnuTLS digest is the hash of the file
# that this script fetched. It shows only that the file did not change after
# that fetch. It does not show that the file was ever correct.
#
# The git checkouts use commits and not tags, because someone can move a tag
# and cannot move a commit. A change to any of these values is a deliberate
# act, and it needs a review.
OPENSSL_VERSION=${OPENSSL_VERSION:-openssl-3.5.7}
LIBRESSL_SHA256=edf01aee24c65d69e6a9efcb9d44bcda682ff9d4f3bbbd95e794e1dfa90847b5
GNUTLS_SHA256=f74fc5954b27d4ec6dfbb11dea987888b5b124289a3703afcada0ee520f4173e
OPENSSL_COMMIT=8cf17aaeb4599f8af87fefd810b5b5fee90fe69e
WOLFSSL_COMMIT=ac01707f552c611fbd135cc723b2682b3e7f80f2
MBEDTLS2_COMMIT=2fc8413bfcb51354c8e679141b17b3f1a5942561
MBEDTLS3_COMMIT=068ff080b369adfac81509f9b57b2afabaf82dc5
MBEDTLS4_COMMIT=0a8fda272a5a0abef3b47c91bed37185d5a726b1
S2N_COMMIT=71adb7ab296406450a62cf59b14553d15ebae9a8
BORINGSSL_COMMIT=52ba6a143e03e9ac84369f79ac3d06dea0128dab
LIBRESSL_VERSION=${LIBRESSL_VERSION:-4.3.2}
MBEDTLS3_VERSION=${MBEDTLS3_VERSION:-mbedtls-3.6.7}
MBEDTLS2_VERSION=${MBEDTLS2_VERSION:-mbedtls-2.28.10}
MBEDTLS4_VERSION=${MBEDTLS4_VERSION:-mbedtls-4.1.1}
WOLFSSL_VERSION=${WOLFSSL_VERSION:-v5.9.2-stable}
GNUTLS_VERSION=${GNUTLS_VERSION:-3.8.3}

# These are the only directory names that this script makes, and the only
# names that it deletes. A different directory under $OPT comes from other
# software.
LOCAL_BUILDS="openssl libressl wolfssl mbedtls mbedtls3 mbedtls4 gnutls s2n boringssl"

# The configurations that need no library. A request for one of these is not an
# error. As a result, other software that runs this script can ask for each
# configuration in turn and does not need to know which of them need a library.
# A CI job that loops over each configuration is such software.
NO_BUILD_NEEDED="system no-tls"

# This code checks the names from the command line for --build and for
# --remove. As a result, the two options cannot disagree about a valid name,
# and neither of them can get a path.
check_names () {
	for one in "$@"; do
		case " $LOCAL_BUILDS $NO_BUILD_NEEDED " in
			*" $one "*) ;;
			*)
				echo "provision: \"$one\" is not one of:$LOCAL_BUILDS $NO_BUILD_NEEDED" >&2
				exit 2 ;;
		esac
	done
}

# The script writes nothing before this point. A name that does not exist must
# be an error and not a note below a report.
check_names "$@"

missing_local=

# The source of a library decides what the script can do with it. "local" is a
# library that this script built, and it can delete such a library. "system" is
# a library that it found, and the software that installed that library must
# remove it.
system_found=

have () { pkg-config --exists "$@" >/dev/null 2>&1; }

report () { printf '  %-12s %-8s %-24s %s\n' "$1" "$2" "$3" "$4"; }

# The identity of a library under $OPT. A build leaves a note with the name of
# what it built, because some of these libraries publish no version. s2n has no
# version, and BoringSSL has no releases by design. For a library that the
# script installed before that note existed, the script uses the installed
# files.
local_version () {
	dir=$OPT/$1

	if [ -r "$dir/.provision-version" ]; then
		cat "$dir/.provision-version"
		return
	fi

	case $1 in
		openssl|libressl)
			PKG_CONFIG_PATH=$dir/lib/pkgconfig pkg-config --modversion libssl 2>/dev/null ;;
		wolfssl|gnutls)
			PKG_CONFIG_PATH=$dir/lib/pkgconfig pkg-config --modversion "$1" 2>/dev/null ;;
		mbedtls|mbedtls3|mbedtls4)
			grep -hoE '"[0-9]+\.[0-9]+\.[0-9]+"' \
				"$dir/include/mbedtls/build_info.h" \
				"$dir/include/mbedtls/version.h" 2>/dev/null | head -1 | tr -d '"' ;;
		boringssl)
			echo "rolling" ;;
		*)
			echo "-" ;;
	esac
}

# The script writes this note after a build that succeeded. The report can then
# name the installed library, also when the library gives no version.
record_version () {
	[ -d "$OPT/$1" ] || return 0
	printf '%s\n' "$2" > "$OPT/$1/.provision-version"
}

# --- removing --------------------------------------------------------------

remove_dir () {
	[ -d "$1" ] || return 0
	size=$(du -sh "$1" 2>/dev/null | cut -f1)
	rm -rf "$1" && echo "  removed $1 ($size)"
	removed=1
}

if [ "$REMOVE" -eq 1 ]; then
	wanted=${*:-$LOCAL_BUILDS}
	removed=0

	for one in $wanted; do
		remove_dir "$OPT/$one"
		remove_dir "$SRC/$one"
		if [ "$one" = libressl ]; then
			remove_dir "$SRC/libressl-$LIBRESSL_VERSION"
			rm -f "$SRC/libressl-$LIBRESSL_VERSION.tar.gz"
		fi
	done

	if [ "$#" -eq 0 ]; then
		rm -f "$SRC/provision-build.log"
		# The script removes these directories only when they are empty.
		# Files from other software stay.
		rmdir "$SRC" 2>/dev/null
		rmdir "$OPT" 2>/dev/null
	fi

	[ "$removed" -eq 0 ] && echo "  nothing to remove"

	echo
	echo "Packaged libraries are untouched: this script never installed them,"
	echo "so it does not remove them either."
	exit 0
fi


echo "TLS libraries the matrix can use:"
echo
printf '  %-12s %-8s %-24s %s\n' LIBRARY WHERE VERSION 'FOUND AT'
printf '  %-12s %-8s %-24s %s\n' ------- ----- ------- --------

# --- one report per library, however it might be had ----------------------

# system_or_local <label> <marker under $OPT/<label>> <pkg-config module>...
#
# The script prefers a system copy, because a user of this library has that
# copy.
system_or_local () {
	label=$1; marker=$2; shift 2

	if [ "$#" -gt 0 ] && pkg-config --exists "$@" 2>/dev/null; then
		report "$label" system "$(pkg-config --modversion "$1" 2>/dev/null)" \
			"$(pkg-config --variable=libdir "$1" 2>/dev/null)"
		system_found="$system_found $label"
		return
	fi

	if [ -e "$OPT/$label/$marker" ]; then
		report "$label" local "$(local_version "$label")" "$OPT/$label"
		return
	fi

	report "$label" - - "not built"
	missing_local="$missing_local $label"
}

# The package manager supplies OpenSSL. This script can build each other
# library, and s2n needs the headers of OpenSSL to build. As a result, a local
# build of OpenSSL gives no benefit.
# The identity of the system libssl. Each supplier gives the pkg-config module
# the name libssl. As a result, the name must come from the headers. The build
# asks the same question, and for the same cause.
system_library () {
	version=$(pkg-config --modversion libssl 2>/dev/null)
	marker=
	if command -v cc >/dev/null 2>&1; then
		marker=$(printf '#include <openssl/opensslv.h>\n' \
			| cc $(pkg-config --cflags libssl 2>/dev/null) -E -dM -x c - 2>/dev/null \
			| grep -oE 'LIBRESSL_VERSION_NUMBER|OPENSSL_IS_BORINGSSL' | head -1)
	fi
	case $marker in
		LIBRESSL_VERSION_NUMBER) printf 'libressl %s' "$version" ;;
		OPENSSL_IS_BORINGSSL)    printf 'boringssl %s' "$version" ;;
		*)                       printf 'openssl %s' "$version" ;;
	esac
}

# A build with no flags: the library that the compiler finds itself. This
# script cannot supply that library. It reports the library and changes
# nothing.
if pkg-config --exists libssl libcrypto 2>/dev/null; then
	report system system "$(system_library)" "the compiler's own search path"
	system_found="$system_found system"
else
	report system - - "no libssl found; nothing here can supply one"
fi

system_or_local wolfssl   lib/pkgconfig/wolfssl.pc  wolfssl
system_or_local mbedtls   include/mbedtls/ssl.h     mbedtls mbedx509 mbedcrypto
system_or_local gnutls    lib/pkgconfig/gnutls.pc   gnutls

# --- never packaged in a usable form, so only ever local -------------------

check_local () {
	if [ -e "$OPT/$1/$2" ]; then
		report "$1" local "$(local_version "$1")" "$OPT/$1"
	else
		report "$1" - - "not built"
		missing_local="$missing_local $1"
	fi
}

check_local openssl   lib/pkgconfig/libssl.pc
check_local libressl  lib/pkgconfig/libssl.pc
check_local mbedtls3  include/mbedtls/ssl.h
check_local mbedtls4  include/mbedtls/ssl.h
check_local s2n       include/s2n.h
check_local boringssl include/openssl/ssl.h

echo
# The libraries under $OPT. --remove can act only on those libraries. A library
# from pkg-config belongs to the system, and this script does not delete it.
present_local=
for one in $LOCAL_BUILDS; do
	[ -d "$OPT/$one" ] && present_local="$present_local $one"
done

if [ -n "$present_local" ] && [ "$BUILD" -eq 0 ]; then
	echo "Built here, and removable by this script:$present_local"
	echo
	echo "  ./tests/provision.sh --remove        all of them"
	echo "  ./tests/provision.sh --remove s2n    one of them"
	echo
fi

if [ -n "$system_found" ] && [ "$BUILD" -eq 0 ]; then
	echo "Found on the system, and not this script's to remove:$system_found"
	echo
	echo "  These came from somewhere else -- a package manager, or the base"
	echo "  system. Removing them is that thing's job. This script only ever"
	echo "  deletes what it built, under $OPT."
	echo
fi

# The script does this only when the command line gives no name. A name must
# reach the loop below. That loop reports a configuration that needs no build,
# or a configuration that is already present.
if [ -z "$missing_local" ] && [ "$#" -eq 0 ]; then
	[ "$BUILD" -eq 1 ] && echo "Nothing left to build."
	exit 0
fi

if [ "$BUILD" -eq 0 ]; then
	echo "Not built here yet:$missing_local"
	echo
	echo "  ./tests/provision.sh --build         all of them"
	echo "  ./tests/provision.sh --build gnutls  one of them"
	exit 0
fi

# --- building --------------------------------------------------------------

for tool in git cmake make cc; do
	command -v "$tool" >/dev/null 2>&1 || {
		echo "provision: $tool is needed to build and is not installed" >&2
		exit 1
	}
done

mkdir -p "$OPT" "$SRC" || exit 1
BUILD_LOG=$SRC/provision-build.log
: > "$BUILD_LOG"

# The script keeps the build output and does not print it. These builds write
# much text that says nothing about the result. Examples are linker warnings
# from the assembly code of a library, and a message from ar about a modifier.
# All of that text hides the one important line when a build fails.
run_quietly () {
	if ! "$@" >> "$BUILD_LOG" 2>&1; then
		echo "  failed: $*"
		echo "  last lines of $BUILD_LOG:"
		tail -15 "$BUILD_LOG" | sed 's/^/    /'
		return 1
	fi
	return 0
}

build_openssl () {
	echo "=== $OPENSSL_VERSION ==="
	clone_pinned openssl https://github.com/openssl/openssl.git \
		"$OPENSSL_VERSION" "$OPENSSL_COMMIT" no || return 1
	cd "$SRC/openssl" || return 1
	# The script gives --libdir=lib because OpenSSL selects lib64 on some
	# systems, and each path here uses lib. install_sw does not install the
	# documentation. That documentation is most of the install time and is of
	# no use here.
	run_quietly ./Configure --prefix="$OPT/openssl" --libdir=lib \
		--openssldir="$OPT/openssl/etc/ssl" || return 1
	run_quietly make -j"$(nproc)" || return 1
	run_quietly make install_sw || return 1
	record_version openssl "${OPENSSL_VERSION#openssl-}"
	cd "$ROOT"
}

build_libressl () {
	echo "=== libressl $LIBRESSL_VERSION ==="
	tarball=libressl-$LIBRESSL_VERSION.tar.gz
	fetch "$SRC/$tarball" "$LIBRESSL_SHA256" \
		"https://cdn.openbsd.org/pub/OpenBSD/LibreSSL/$tarball" \
		"https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/$tarball" || return 1
	rm -rf "$SRC/libressl-$LIBRESSL_VERSION"
	run_quietly tar xf "$SRC/$tarball" -C "$SRC" || return 1
	cd "$SRC/libressl-$LIBRESSL_VERSION" || return 1
	# openssldir stays at its default of $prefix/etc/ssl. See the note
	# above.
	run_quietly ./configure --prefix="$OPT/libressl" || return 1
	run_quietly make -j"$(nproc)" || return 1
	run_quietly make install || return 1
	record_version libressl "$LIBRESSL_VERSION"
	cd "$ROOT"
}

# A download goes to a temporary name, and the script gives it the final name
# only after curl reports success. With a write to the final name, an
# incomplete download leaves a short file that looks like a cached file. Each
# later build then fails on that file and never fetches it again.
# fetch <destination> <sha256> <url>...
#
# Tries each source in turn and keeps the first file that agrees with the
# digest. More than one source is of value, because an origin can stop or can
# refuse traffic. The digest makes a mirror as safe as the origin: the file
# gives the expected hash, or the script discards it.
fetch () {
	dest=$1
	want=$2
	shift 2

	# The script discards a cached file that no longer agrees with the
	# digest, and it fetches the file again. A refusal and a stop would leave
	# an incorrect file, and a user could correct that only with a manual
	# delete. The .part rename below prevents the same problem.
	if [ -s "$dest" ]; then
		verify_sha256 "$dest" "$want" && return 0
		echo "    discarding it and downloading again"
		rm -f "$dest"
	fi

	for url in "$@"; do
		run_quietly curl -fsSL -o "$dest.part" "$url" || { rm -f "$dest.part"; continue; }

		# The script checks the file before it gives the file its real
		# name. As a result, a file that fails the check never looks like a
		# complete download.
		if verify_sha256 "$dest.part" "$want"; then
			mv "$dest.part" "$dest"
			return 0
		fi

		echo "    from $url"
		rm -f "$dest.part"
	done

	echo "  no source produced a usable $dest"
	return 1
}

verify_sha256 () {
	got=$(sha256sum "$1" 2>/dev/null | cut -d" " -f1)
	[ "$got" = "$2" ] && return 0

	echo "  $1"
	echo "    expected sha256 $2"
	echo "    got             ${got:-<could not read>}"
	return 1
}

# A clone gets the current commit of the tag. This value is the commit of that
# tag at the time of this script.
verify_commit () {
	got=$(git -C "$1" rev-parse HEAD 2>/dev/null)
	[ "$got" = "$2" ] && return 0

	echo "  $1"
	echo "    expected commit $2"
	echo "    got             ${got:-<not a checkout>}"
	return 1
}

build_gnutls () {
	echo "=== gnutls $GNUTLS_VERSION ==="
	series=${GNUTLS_VERSION%.*}
	tarball=gnutls-$GNUTLS_VERSION.tar.xz
	# The mirror comes first, and that is on purpose. The GnuTLS releases are
	# on gnupg.org. The gcrypt/ directory is shared infrastructure for the GNU
	# crypto projects, and GnuPG, Libgcrypt and GnuTLS are all there. But that
	# host frequently gives 403 for this file, and the script tries the
	# mirror first. The origin stays as the second source. A mirror costs
	# nothing here, because the digest decides if the script keeps a download.
	#
	# dotsrc.org is the mirror service of Aalborg University in Denmark.
	fetch "$SRC/$tarball" "$GNUTLS_SHA256" \
		"https://mirrors.dotsrc.org/gcrypt/gnutls/v$series/$tarball" \
		"https://www.gnupg.org/ftp/gcrypt/gnutls/v$series/$tarball" || return 1
	rm -rf "$SRC/gnutls-$GNUTLS_VERSION"
	run_quietly tar xf "$SRC/$tarball" -C "$SRC" || return 1
	cd "$SRC/gnutls-$GNUTLS_VERSION" || return 1
	# nettle, gmp and libtasn1 must already be present. GnuTLS can supply its
	# own unistring. The script turns off the other options to keep the list
	# of necessary libraries short.
	run_quietly ./configure --prefix="$OPT/gnutls" \
		--with-included-unistring --without-p11-kit --without-idn \
		--disable-doc --disable-tools --disable-tests \
		--disable-full-test-suite --disable-libdane || return 1
	run_quietly make -j"$(nproc)" || return 1
	run_quietly make install || return 1
	record_version gnutls "$GNUTLS_VERSION"
	cd "$ROOT"
}

# clone_pinned <name> <url> <tag or ""> <commit> <submodules: yes|no>
#
# The script uses a commit and not a tag, because someone can move a tag and a
# commit is a hash of its own content. For the same cause, a checkout needs no
# separate digest: git checks it.
#
# The caller asks for the submodules, and the script does not fetch them by
# default. mbedTLS 3.6 keeps part of its build system in a submodule and does
# not configure without it. The eleven submodules of OpenSSL are all test data,
# and to fetch them costs much more than their value.
clone_pinned () {
	name=$1; url=$2; ref=$3; commit=$4; subs=$5

	rm -rf "$SRC/$name"

	set -- --depth 1
	[ "$subs" = yes ] && set -- "$@" --recurse-submodules --shallow-submodules
	[ -n "$ref" ] && set -- "$@" --branch "$ref"

	run_quietly git clone "$@" "$url" "$SRC/$name" || return 1
	verify_commit "$SRC/$name" "$commit" || return 1
}

build_cmake () {
	name=$1; url=$2; ref=$3; commit=$4; subs=$5; shift 5
	echo "=== $name ==="
	clone_pinned "$name" "$url" "$ref" "$commit" "$subs" || return 1
	cd "$SRC/$name" || return 1
	run_quietly cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$OPT/$name" "$@" || return 1
	run_quietly cmake --build build -j"$(nproc)" || return 1
	run_quietly cmake --install build || return 1
	# The script writes the tag if there is one, and the commit if there is
	# none. It clones s2n and BoringSSL from their default branch, and those
	# two have no release name.
	version=$ref
	[ -n "$version" ] || version=$(git -C "$SRC/$name" rev-parse --short HEAD 2>/dev/null)
	version=${version#mbedtls-}; version=${version#v}; version=${version%-stable}
	record_version "$name" "${version:-unknown}"
	cd "$ROOT"
}

wanted=${*:-$missing_local}

failed=
for one in $wanted; do
	case " $NO_BUILD_NEEDED " in
		*" $one "*)
			echo "=== $one ==="
			echo "  nothing to build: this configuration uses what is already here"
			continue ;;
	esac

	# A request for a library that is already present is not an error. But
	# the script reports that condition and does not look like it did
	# work.
	case " $missing_local " in
		*" $one "*) ;;
		*)
			echo "=== $one ==="
			echo "  already present at $OPT/$one"
			continue ;;
	esac

	case $one in
		openssl)
			build_openssl || failed="$failed openssl" ;;
		libressl)
			build_libressl || failed="$failed libressl" ;;
		wolfssl)
			# The build uses OPENSSLALL and not OPENSSLEXTRA. See the note
			# at the top. A default wolfSSL build has DTLS off. That
			# condition shows as an undefined WOLFSSL_DTLS in the
			# installed options.h, and nothing is absent at link time. The
			# DTLS tests would then skip here and in no other
			# configuration. Such a gap can stay unknown for a release.
			build_cmake wolfssl https://github.com/wolfSSL/wolfssl.git "$WOLFSSL_VERSION" "$WOLFSSL_COMMIT" yes \
				-DBUILD_SHARED_LIBS=ON -DWOLFSSL_OPENSSLALL=yes \
				-DWOLFSSL_TLS13=yes -DWOLFSSL_SNI=yes -DWOLFSSL_DTLS=yes \
				-DWOLFSSL_EXAMPLES=no -DWOLFSSL_CRYPT_TESTS=no \
				|| failed="$failed wolfssl" ;;
		mbedtls)
			# The build sets CMAKE_POLICY_VERSION_MINIMUM because mbedTLS
			# 2.28 asks for a cmake_minimum_required value that CMake 4
			# refuses. Older versions of CMake ignore this variable.
			build_cmake mbedtls https://github.com/Mbed-TLS/mbedtls.git "$MBEDTLS2_VERSION" "$MBEDTLS2_COMMIT" yes \
				-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
				-DENABLE_TESTING=Off -DENABLE_PROGRAMS=Off -DUSE_SHARED_MBEDTLS_LIBRARY=On \
				|| failed="$failed mbedtls" ;;
		gnutls)
			missing_deps=
			for dep in nettle hogweed libtasn1; do
				pkg-config --exists "$dep" 2>/dev/null || missing_deps="$missing_deps $dep"
			done
			if [ -n "$missing_deps" ]; then
				echo "=== gnutls ==="
				echo "  needs these to build, and they are not here:$missing_deps"
				echo "  on Debian or Ubuntu: nettle-dev libtasn1-6-dev libgmp-dev"
				echo "  this script does not install them: they are somebody else's"
				echo "  libraries, not a TLS library it is here to provide."
				failed="$failed gnutls"
			else
				build_gnutls || failed="$failed gnutls"
			fi ;;
		mbedtls3)
			build_cmake mbedtls3 https://github.com/Mbed-TLS/mbedtls.git "$MBEDTLS3_VERSION" "$MBEDTLS3_COMMIT" yes \
				-DENABLE_TESTING=Off -DENABLE_PROGRAMS=Off -DUSE_SHARED_MBEDTLS_LIBRARY=On \
				|| failed="$failed mbedtls3" ;;
		mbedtls4)
			# Version 4.x makes its PSA driver wrappers at build time with
			# a Python script, and that script imports these modules. An
			# absent module stops the build after one minute, at 3%, and
			# the traceback is deep in the output of a parallel make. As a
			# result, the script checks for the modules here, where a
			# message is still of use.
			missing_deps=
			for dep in jsonschema jinja2; do
				"${PYTHON:-python3}" -c "import $dep" 2>/dev/null || missing_deps="$missing_deps $dep"
			done
			if [ -n "$missing_deps" ]; then
				echo "=== mbedtls4 ==="
				echo "  needs these Python modules to build, and they are not here:$missing_deps"
				echo "  on Debian or Ubuntu: python3-jsonschema python3-jinja2"
				echo "  or into whichever interpreter builds it: pip install jsonschema jinja2"
				echo "  this script does not install them: they belong to your Python"
				echo "  installation, not to a TLS library it is here to provide."
				failed="$failed mbedtls4"
			else
				# Version 4.x keeps all of its cryptography in the
				# tf-psa-crypto submodule. As a result, the recursive
				# clone is more important here than for 3.6. Without it
				# there is nothing to configure.
				build_cmake mbedtls4 https://github.com/Mbed-TLS/mbedtls.git "$MBEDTLS4_VERSION" "$MBEDTLS4_COMMIT" yes \
					-DENABLE_TESTING=Off -DENABLE_PROGRAMS=Off -DUSE_SHARED_MBEDTLS_LIBRARY=On \
					|| failed="$failed mbedtls4"
			fi ;;
		s2n)
			build_cmake s2n https://github.com/aws/s2n-tls.git "" "$S2N_COMMIT" yes \
				-DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON \
				|| failed="$failed s2n" ;;
		boringssl)
			build_cmake boringssl https://boringssl.googlesource.com/boringssl "" "$BORINGSSL_COMMIT" yes \
				-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
				|| failed="$failed boringssl" ;;
	esac
done

echo
if [ -n "$failed" ]; then
	echo "could not build:$failed"
	exit 1
fi

echo "built into $OPT. tests/matrix.sh and tests/coverage-all.sh find these on"
echo "their own; nothing else has to be told about them."
exit 0
