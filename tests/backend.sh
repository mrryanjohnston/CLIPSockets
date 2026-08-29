#!/bin/sh
# Builds one configuration and runs the test suite on it.
#
#   ./tests/backend.sh gnutls               build with GnuTLS and run the suite
#   ./tests/backend.sh gnutls --magic       the same, with libmagic
#   ./tests/backend.sh no-tls               no TLS
#   ./tests/backend.sh list                 what this machine can build
#   make test-gnutls                        the first of these
#
#   ./tests/backend.sh gnutls --coverage DIR
#       an instrumented build, the suite, and the coverage of each line for
#       this configuration. The script writes the data into DIR, and
#       tests/coverage-all.sh adds it up later.
#
# tests/matrix.sh and tests/coverage-all.sh use this script, and each of them is
# a loop over it. Each rule that applies to one configuration belongs here. As a
# result, one run by hand and a run of each configuration cannot disagree.
#
# Each build starts from clean. TLS and MAGIC both change CFLAGS, and the build
# compiles the objects in place. To use those objects again would test a mixture
# of two builds.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1

. "$ROOT/tests/backends.sh"

label=${1:-}
if [ -z "$label" ] || [ "$label" = -h ] || [ "$label" = --help ]; then
	sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
fi

if [ "$label" = list ]; then
	printf '%s\n' "$BASES" | while IFS='|' read -r one _; do
		[ -n "$one" ] && echo "$one"
	done
	[ -n "$TLS_UNAVAILABLE" ] && {
		echo
		echo "not available here:$TLS_UNAVAILABLE"
		echo "  ./tests/provision.sh        what is missing and how to get it"
	}
	exit 0
fi

shift
magic=0
coverage_dir=

while [ "$#" -gt 0 ]; do
	case $1 in
		--magic)    magic=1 ;;
		--coverage) coverage_dir=${2:-}; shift ;;
		*) echo "backend.sh: unknown option $1" >&2; exit 2 ;;
	esac
	shift
done

# --- change the label into make arguments -------------------------------
args=
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
	echo "backend.sh: no configuration called \"$label\" on this machine" >&2
	echo "  ./tests/backend.sh list     what there is" >&2
	echo "  ./tests/provision.sh        what is missing and how to get it" >&2
	exit 2
fi

[ "$magic" -eq 1 ] && args="$args MAGIC=1"

# --- build ---------------------------------------------------------------
name=$label
[ "$magic" -eq 1 ] && name="$label+magic"

make clean >/dev/null 2>&1

if [ -n "$coverage_dir" ]; then
	# The build always sets MAGIC=1. As a result, the coverage data includes
	# the libmagic code, and the build does not remove that code.
	buildargs="coverage MAGIC=1 $args"
else
	buildargs="$args"
fi

if ! make $buildargs > "${TMPDIR:-/tmp}/backend-build.$$" 2>&1; then
	echo "$name: BUILD FAILED"
	grep -E 'Error|error:|not the same implementation|run time' \
		"${TMPDIR:-/tmp}/backend-build.$$" | head -4 | sed 's/^/  /'
	rm -f "${TMPDIR:-/tmp}/backend-build.$$"
	exit 1
fi
rm -f "${TMPDIR:-/tmp}/backend-build.$$"

# --- the library that the build used ------------------------------------
# The script asks the binary and does not use the command line. A second TLS
# library under /usr/local comes before the system library. As a result,
# TLS_BACKEND=openssl does not always mean OpenSSL.
backend=-
version=-
probe=${TMPDIR:-/tmp}/backend-ident.$$
printf '(printout t (tls-backend) "|" (tls-backend-version) crlf)\n(exit)\n' > "$probe"
ident=$(./clips -f2 "$probe" 2>/dev/null | tail -1)
rm -f "$probe"
case $ident in
	*"|"*) backend=${ident%%|*}; version=${ident#*|} ;;
esac

# --- run -----------------------------------------------------------------
log=${TMPDIR:-/tmp}/backend-test.$$
if ./tests/run.sh > "$log" 2>&1; then
	status=0
	result=$(grep '^files:' "$log")
else
	status=1
	result=$(grep -E '^files:|^failing:' "$log" | tr '\n' ' ')
fi

grep -E '^  (FAIL|SKIP) ' "$log" | sed 's/^/  /'
echo "$name ($backend $version): $result"

if [ -n "$coverage_dir" ]; then
	if COVERAGE_COLLECT="$coverage_dir/$name" ./tests/coverage.sh; then
		echo "  coverage collected"
	else
		echo "  no coverage data"
	fi
	# An instrumented tree would make the next usual build use the -O0
	# coverage objects again.
	make clean >/dev/null 2>&1
fi

# One row for the script that collects the results, if such a script runs.
if [ -n "${BACKEND_ROW:-}" ]; then
	printf '%s|%s|%s|%s\n' "$name" "$backend" "$version" "$result" >> "$BACKEND_ROW"
fi

rm -f "$log"
exit $status
