#!/bin/sh
# Line coverage across each build and not only one build.
#
#   ./tests/coverage-all.sh          build each configuration, join the data,
#                                    and write the report
#   make coverage-all                the same
#
# This script is a loop over tests/backend.sh --coverage. tests/matrix.sh uses
# the same script. For one configuration alone:
#
#   ./tests/backend.sh gnutls --magic --coverage /tmp/cov
#
# One build has one TLS backend. As a result, one coverage run can measure only
# that backend. The other backends are absent and are not uncovered. A report
# from one build ignores them, which hides if any test covers them. Or it
# counts them as zero per cent, and that number is about the build and not
# about the code.
#
# As a result, this script builds and runs each configuration in turn, keeps
# the data for each line, and adds it all together at the end. A line is
# covered when any build covered it. The build with no TLS also belongs here.
# It is the only build that reaches the plaintext part of the branches in
# socketrtr.c.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1


. "$ROOT/tests/backends.sh"

data=$(mktemp -d) || exit 1
trap 'rm -rf "$data"' EXIT INT TERM

printf '%s\n' "$BASES" | while IFS='|' read -r label _; do
	[ -n "$label" ] && echo "$label"
done > "$data/labels"

while read -r label; do
	[ -n "$label" ] || continue
	# The build always includes libmagic. As a result, the coverage data
	# includes that code, and the build does not remove it.
	./tests/backend.sh "$label" --magic --coverage "$data" || true
done < "$data/labels"

echo
if [ -n "$TLS_UNAVAILABLE" ]; then
	echo "not measured, because not installed here:$TLS_UNAVAILABLE"
	echo "  ./tests/provision.sh    what each one needs"
	echo
fi

echo "merged coverage over every configuration built:"
echo

COVERAGE_MERGE="$data" ./tests/coverage.sh
status=$?

make clean >/dev/null 2>&1

exit $status
