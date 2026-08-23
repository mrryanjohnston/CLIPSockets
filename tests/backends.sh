# The TLS configurations that this machine can build, as
# "label|make arguments". tests/matrix.sh and tests/coverage-all.sh both source
# this file, and the two cannot disagree about the available
# configurations.
#
# The script looks for each library two times. First it looks on the system,
# where it uses the library as it is. Then it looks under $TLS_OPT_DIR, where
# tests/provision.sh puts the libraries that it builds. A system copy wins,
# because a user of this library has that copy. No code here installs
# anything.
#
# TLS_UNAVAILABLE collects the libraries that the script did not find. As a
# result, a run can report what it did not test. Without that list, a run
# reports success for the libraries that are present and says nothing about
# the others.

TLS_OPT_DIR=${TLS_OPT_DIR:-$HOME/opt}

probe () { pkg-config "$@" >/dev/null 2>&1; }

BASES="no-tls|TLS=0"
TLS_UNAVAILABLE=

# add_backend <label> <TLS_BACKEND value> <marker under $TLS_OPT_DIR/<label>>
#             <pkg-config module>...
#
# The label "system" is different from the others. It means a build with no
# arguments, with the libssl and libcrypto that the compiler finds itself. The
# machine decides which library that is. It is usually OpenSSL, LibreSSL on
# OpenBSD, or a library that someone installed under /usr/local. As a result,
# the label gives the source of the library and not its name, because the
# script cannot know that name.
add_backend () {
	label=$1
	backend=$2
	marker=$3
	shift 3

	if [ "$#" -gt 0 ] && probe --exists "$@"; then
		if [ "$label" = system ]; then
			BASES="$BASES
system|"
		else
			BASES="$BASES
$label|TLS_BACKEND=$backend"
		fi
		return 0
	fi

	dir=$TLS_OPT_DIR/$label
	if [ -n "$marker" ] && [ -e "$dir/$marker" ]; then
		BASES="$BASES
$label|TLS_BACKEND=$backend TLS_PREFIX=$dir"
		return 0
	fi

	TLS_UNAVAILABLE="$TLS_UNAVAILABLE $label"
	return 1
}

# The script prefers a system copy. tests/provision.sh builds a copy under
# $TLS_OPT_DIR when there is no system copy.
add_backend system    openssl  ""                           libssl libcrypto
add_backend wolfssl   wolfssl  lib/pkgconfig/wolfssl.pc     wolfssl
add_backend mbedtls   mbedtls  include/mbedtls/ssl.h        mbedtls mbedx509 mbedcrypto
add_backend gnutls    gnutls   lib/pkgconfig/gnutls.pc      gnutls

# No package supplies these libraries in a usable form, and they are
# always local. LibreSSL supplies libssl and libcrypto with the names of
# OpenSSL, and it cannot be a system library beside OpenSSL. No package
# supplies the other three libraries. mbedtls3 and mbedtls4 are here with the
# packaged mbedtls, because the three differ in ways that the suite checks. 2.x
# and 3.x differ on TLS 1.3, and 4.x differs on the source of the random
# generator. Upstream also supports only those two lines, and the packaged
# library is the version of the system. Here openssl means a build under
# $TLS_OPT_DIR through TLS_PREFIX. The script does not let it use the system
# copy, and that is on purpose. It would then be a second name for "system".
add_backend openssl   openssl  lib/pkgconfig/libssl.pc
add_backend libressl  libressl lib/pkgconfig/libssl.pc
add_backend mbedtls3  mbedtls  include/mbedtls/ssl.h
add_backend mbedtls4  mbedtls  include/mbedtls/ssl.h
add_backend s2n       s2n      include/s2n.h
add_backend boringssl boringssl include/openssl/ssl.h

# A prefix on the command line wins over each entry above.
[ -n "${LIBRESSL_PREFIX:-}" ] && BASES="$BASES
libressl|TLS_BACKEND=libressl TLS_PREFIX=$LIBRESSL_PREFIX"
[ -n "${MBEDTLS3_PREFIX:-}" ] && BASES="$BASES
mbedtls3|TLS_BACKEND=mbedtls TLS_PREFIX=$MBEDTLS3_PREFIX"
[ -n "${MBEDTLS4_PREFIX:-}" ] && BASES="$BASES
mbedtls4|TLS_BACKEND=mbedtls TLS_PREFIX=$MBEDTLS4_PREFIX"
[ -n "${S2N_PREFIX:-}" ] && BASES="$BASES
s2n|TLS_BACKEND=s2n TLS_PREFIX=$S2N_PREFIX"
[ -n "${BORINGSSL_PREFIX:-}" ] && BASES="$BASES
boringssl|TLS_BACKEND=boringssl TLS_PREFIX=$BORINGSSL_PREFIX"

# A prefix on the command line can supply a library that the script counted as
# absent.
for named in ${LIBRESSL_PREFIX:+libressl} ${MBEDTLS3_PREFIX:+mbedtls3} \
             ${MBEDTLS4_PREFIX:+mbedtls4} \
             ${S2N_PREFIX:+s2n} ${BORINGSSL_PREFIX:+boringssl}; do
	TLS_UNAVAILABLE=$(printf '%s' "$TLS_UNAVAILABLE" | sed "s/ $named\$//; s/ $named / /")
done

# Two equal labels would build the same configuration two times.
BASES=$(printf '%s\n' "$BASES" | awk -F'|' '!seen[$1]++')
