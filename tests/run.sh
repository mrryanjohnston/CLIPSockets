#!/bin/sh
# Run the CLIPSockets test suite.
#
#   ./tests/run.sh                     run every test
#   ./tests/run.sh tests/unit/errno.clp  run specific files
#
# Environment:
#   CLIPS_BIN      path to the clips binary   (default ./clips)
#   TEST_TIMEOUT   seconds allowed per file   (default 15)
#
# Each test file is run in its own clips process with stdin closed. A file
# passes only if it exits 0 *and* prints the ##SUMMARY sentinel, so a test
# that crashes or falls off the end of the batch is reported as incomplete
# rather than silently passing.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1

CLIPS_BIN=${CLIPS_BIN:-./clips}
TEST_TIMEOUT=${TEST_TIMEOUT:-15}

if [ ! -x "$CLIPS_BIN" ]; then
	echo "run.sh: no clips binary at $CLIPS_BIN (run make first)" >&2
	exit 2
fi

# The (mimetype) UDF only exists in a MAGIC=1 build. Tests that need it carry
# a "requires: libmagic" marker and are skipped otherwise.
if ldd "$CLIPS_BIN" 2>/dev/null | grep -q libmagic; then
	HAVE_LIBMAGIC=1
else
	HAVE_LIBMAGIC=0
fi

if [ "$#" -gt 0 ]; then
	TESTS=$*
else
	TESTS=$(find tests/unit tests/integration tests/error -name '*.clp' 2>/dev/null | sort)
fi

if [ -z "$TESTS" ]; then
	echo "run.sh: no test files found" >&2
	exit 2
fi

passed=0
failed=0
skipped=0
failed_names=

for t in $TESTS; do
	if [ ! -f "$t" ]; then
		echo "  SKIP $t (no such file)"
		skipped=$((skipped + 1))
		continue
	fi

	if grep -q 'requires: libmagic' "$t" && [ "$HAVE_LIBMAGIC" -eq 0 ]; then
		echo "  SKIP $t (built without libmagic)"
		skipped=$((skipped + 1))
		continue
	fi

	out=$(timeout "$TEST_TIMEOUT" "$CLIPS_BIN" -f2 "$t" </dev/null 2>&1)
	status=$?

	if [ "$status" -eq 124 ]; then
		reason="timed out after ${TEST_TIMEOUT}s"
	elif ! printf '%s\n' "$out" | grep -q '^##SUMMARY'; then
		reason="did not reach (test-summary) -- exit $status"
	elif [ "$status" -ne 0 ]; then
		reason="assertions failed"
	else
		reason=
	fi

	if [ -z "$reason" ]; then
		counts=$(printf '%s\n' "$out" | sed -n 's/^##SUMMARY .*run=\([0-9]*\).*/\1/p')
		echo "  PASS $t (${counts:-0} checks)"
		passed=$((passed + 1))
	else
		echo "  FAIL $t ($reason)"
		printf '%s\n' "$out" | sed 's/^/       | /'
		failed=$((failed + 1))
		failed_names="$failed_names $t"
	fi
done

echo
echo "files: $passed passed, $failed failed, $skipped skipped"
if [ "$failed" -ne 0 ]; then
	echo "failing:$failed_names"
	exit 1
fi
exit 0
