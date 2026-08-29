#!/bin/sh
# Builds against every CLIPS version this project supports and runs the whole
# suite on each one.
#
# What varies between them is the source the wrappers are compiled into: the
# 6.4.2 release tarball, branches/64x of the CLIPS Subversion repository, and
# branches/70x. Each is fetched under vendor/clips-source/<tag>, built in a
# copy of itself under vendor/clips-build/<tag>, and the suite is run against
# the binary that comes out. Nothing else varies -- one TLS backend, one set of
# tests -- so a failure here is about the CLIPS version and not about anything
# else.
#
# The branches are pinned to the revisions the makefile names, so a run today
# and a run next month are the same run. Move one with CLIPS_SVN_REV, or take
# the head of a branch with CLIPS_SVN_REV=HEAD, which is how a change upstream
# is found while it is still on a branch.
#
# Usage:
#   scripts/test-clips-versions.sh              every version the makefile names
#   scripts/test-clips-versions.sh 6.4.2 svn-7x only these
#   make test-clips                             the first of these
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root" || exit 1

default_versions=$(make -s print-clips-versions 2>/dev/null)
[ -n "$default_versions" ] || default_versions='6.4.2 svn-6x svn-7x'
versions=${*:-$default_versions}

# The version a plain "make" builds, so the tree is left the way it was found.
default_version=$(make -s print-clips 2>/dev/null | awk '$1 == "CLIPS_VERSION" { print $2 }')
[ -n "$default_version" ] || default_version=6.4.2

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT INT TERM

results=$tmp/results
: > "$results"

echo "testing: $versions"
echo

failures=0

for version in $versions; do
	printf '=== CLIPS %s ' "$version"
	printf '%.0s=' $(seq 1 $((58 - ${#version}))) 2>/dev/null
	echo

	# Each version has its own build directory, so there is nothing to clean
	# between them: an object of one version is never in the tree of another.
	if ! make CLIPS_VERSION="$version" > "$tmp/build.log" 2>&1; then
		echo "  BUILD FAILED"
		grep -E 'Error|error:|fetch-clips:' "$tmp/build.log" | head -10 | sed 's/^/    /'
		printf '%-8s %-16s %-38s %s\n' "$version" "-" "-" "build failed" >> "$results"
		failures=$((failures + 1))
		echo
		continue
	fi

	build=$(make -s CLIPS_VERSION="$version" print-clips | awk '$1 == "build" { print $2 }')
	source_of=$(cat "$build/.clips-source" 2>/dev/null || echo unknown)

	# The directory name, which is the version and, for a branch, the revision
	# it was pinned at. The line above is the whole URL and is too wide for the
	# table at the end.
	tag=$(basename "$build")

	# The version the source calls itself, out of the header CLIPS keeps it
	# in. A branch head reports the release it is working towards, so this
	# and the line above answer different questions and both are printed.
	banner=$(sed -n 's/^#define VERSION_STRING "\(.*\)"/\1/p' \
		"$build/constant.h" 2>/dev/null | head -1)
	[ -n "$banner" ] || banner=-

	if ./tests/run.sh > "$tmp/test.log" 2>&1; then
		outcome=PASSED
	else
		outcome=FAILED
		failures=$((failures + 1))
	fi

	counts=$(grep '^files:' "$tmp/test.log" | tail -1)
	if [ -z "$counts" ]; then
		counts="no file count printed"
		[ "$outcome" = PASSED ] && failures=$((failures + 1))
		outcome="FAILED (did not finish)"
		tail -5 "$tmp/test.log" | sed 's/^/    /'
	fi

	echo "  source:  $source_of"
	echo "  reports: CLIPS $banner"
	echo "  $counts"
	[ "$outcome" = PASSED ] || grep -E '^  FAIL |^failing:' "$tmp/test.log" | head -20 | sed 's/^/    /'
	echo "  $outcome"
	echo

	printf '%-8s %-16s %-38s %s\n' "$version" "$tag" "$counts" "$outcome" >> "$results"
done

# Leave ./clips and the vendor/clips symlink pointing at the version a plain
# "make" builds, rather than at whichever version came last here.
echo "restoring the default build (CLIPS $default_version)"
make CLIPS_VERSION="$default_version" >/dev/null 2>&1

echo
echo "================================================================"
cat "$results"
echo "================================================================"

if [ "$failures" -gt 0 ]; then
	echo "$failures version(s) failed"
	exit 1
fi

echo "every version passed"
