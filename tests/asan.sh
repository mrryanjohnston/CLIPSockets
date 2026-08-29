#!/bin/sh
# Builds with AddressSanitizer and runs the suite on that build.
#
#   ./tests/asan.sh                 the default build, with the TLS library
#                                   that it selects
#   ./tests/asan.sh gnutls          one configuration, with the label that
#                                   backend.sh uses
#   ./tests/asan.sh gnutls tests/error/tls-context-lifetime.clp
#                                   the same, with only these test files
#
# You cannot write a check in CLIPS for some defects, because CLIPS never sees
# them. A read after the end of an allocation gives the next value in memory.
# That value is usually a zero, and a zero looks like the correct answer. The
# suite then passes and the defect stays. The only method to fail such a test
# is a machine that checks each access, and this script does that.
#
# As a result, this is a test and not a tool for debug work. It is not part of
# "make test" because it builds the tree from clean and runs several times more
# slowly. It is not optional.
#
# ASAN_OPTIONS: halt_on_error stops at the first report. Without it, a process
# with damaged memory continues and gives more reports. detect_leaks is off
# because CLIPS keeps its own memory pool and frees it at exit through code
# that a leak checker reads as lost memory. That is a different subject.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1

. "$ROOT/tests/backends.sh"

label=${1:-}
case $label in
	-h|--help)
		sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
		exit 2 ;;
esac

[ "$#" -gt 0 ] && shift

# --- change the label into make arguments, as backend.sh does --------------
args=
if [ -n "$label" ]; then
	found=0
	while IFS='|' read -r one rest; do
		[ "$one" = "$label" ] || continue
		args=$rest
		found=1
		break
	done <<EOF
$(printf '%s\n' "$BASES")
EOF

	if [ "$found" -eq 0 ]; then
		echo "asan.sh: no configuration called \"$label\" on this machine" >&2
		echo "  ./tests/backend.sh list     what there is" >&2
		exit 2
	fi
fi

# --- build ----------------------------------------------------------------
# The sanitizer must be in two places: in CFLAGS to instrument the code, and in
# LDLIBS to link the library that writes the reports. A CFLAGS value on the
# command line replaces the value in the makefile. As a result, this script
# must repeat the flags that build a correct clips: the standard, the TLS
# defines and the include paths. The script asks the makefile for those flags
# and does not write them here. As a result, they cannot become different from
# the flags of a usual build.
tls_cflags=$(make $args --no-print-directory print-TLS_CFLAGS 2>/dev/null | tail -1)
tls_ldlibs=$(make $args --no-print-directory print-TLS_LDLIBS 2>/dev/null | tail -1)

SAN="-fsanitize=address -fno-omit-frame-pointer"

echo "asan: building ${label:-default} ..."
make clean >/dev/null 2>&1

log=${TMPDIR:-/tmp}/asan-build.$$
if ! make debug $args \
	CFLAGS="$tls_cflags -std=c99 -O1 -g $SAN" \
	LDLIBS="-lm $tls_ldlibs $SAN" > "$log" 2>&1
then
	echo "asan: BUILD FAILED"
	grep -E 'Error|error:' "$log" | head -5 | sed 's/^/  /'
	rm -f "$log"
	exit 1
fi
rm -f "$log"

# --- run ------------------------------------------------------------------
# A sanitizer report goes to stderr and the process ends. As a result, run.sh
# sees a stop and reports the file as a failure. That is the full mechanism. No
# code here parses the output of ASAN. It only shows that output.
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1}
export ASAN_OPTIONS

out=${TMPDIR:-/tmp}/asan-run.$$
timeout_status=0
./tests/run.sh "$@" > "$out" 2>&1 || timeout_status=$?

cat "$out"

if grep -q 'ERROR: AddressSanitizer' "$out"; then
	echo
	echo "asan: sanitizer reports above -- these are failures, not warnings"
	rm -f "$out"
	# An instrumented tree would make the next usual build use the -O1
	# objects again. Those objects have no coverage data and have a sanitizer
	# that the build does not link.
	make clean >/dev/null 2>&1
	exit 1
fi

rm -f "$out"
make clean >/dev/null 2>&1

exit $timeout_status
