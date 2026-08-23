#!/bin/sh
# Runs the test suite on each configuration that this machine can build.
#
#   ./tests/matrix.sh                        each configuration
#   MATRIX_FILTER=gnutls ./tests/matrix.sh   only labels with "gnutls" in them
#   make matrix
#
# This script is a loop over tests/backend.sh. That script does the work, and
# you can run it for one configuration:
#
#   ./tests/backend.sh list
#   ./tests/backend.sh gnutls
#
# The script builds each TLS library with libmagic and without libmagic.
# libmagic is the other option that changes the compiled code.
#
# The script reports the libraries that are not present and does not ignore
# them. A run that reports "all 4 configurations passed" on a machine with one
# TLS library must not look the same as a run that covered eight
# configurations.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1

MATRIX_FILTER=${MATRIX_FILTER:-}

. "$ROOT/tests/backends.sh"

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT INT TERM

rows=$tmp/rows
: > "$rows"

printf '%s\n' "$BASES" | while IFS='|' read -r label _; do
	[ -n "$label" ] && echo "$label"
done > "$tmp/labels"

failures=0
configs=0

while read -r label; do
	[ -n "$label" ] || continue

	for variant in plain magic; do
		name=$label
		flag=
		if [ "$variant" = magic ]; then
			name="$label+magic"
			flag=--magic
		fi

		case "$name" in
			*"$MATRIX_FILTER"*) ;;
			*) continue ;;
		esac

		configs=$((configs + 1))
		printf '=== %s ===\n' "$name"
		BACKEND_ROW=$rows ./tests/backend.sh "$label" $flag || failures=$((failures + 1))
	done
done < "$tmp/labels"

echo
printf '%-18s %-10s %-28s %s\n' CONFIGURATION BACKEND VERSION RESULT
printf '%-18s %-10s %-28s %s\n' ------------- ------- ------- ------
while IFS='|' read -r name backend version result; do
	printf '%-18s %-10s %-28s %s\n' "$name" "$backend" "$version" "$result"
done < "$rows"

echo
if [ -n "$TLS_UNAVAILABLE" ]; then
	echo "not built, because not installed here:$TLS_UNAVAILABLE"
	echo "  ./tests/provision.sh    what each one needs"
	echo
fi

if [ "$failures" -ne 0 ]; then
	echo "matrix: $failures of $configs configurations failed"
	exit 1
fi
echo "matrix: all $configs configurations passed"
exit 0
