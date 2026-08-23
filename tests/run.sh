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

# The script asks the binary if the build includes an optional feature. It does
# not read the dynamic dependencies of the binary. A library that the build
# links statically leaves nothing for ldd, and BoringSSL supplies no shared
# objects at all. A list of library names also becomes incorrect at each new
# backend. As a result, the script names a function that the binary has only
# with that feature, and that question operates for each backend.
has_function () {
	probe=$(mktemp) || return 1
	printf '(%s)\n(exit)\n' "$1" > "$probe"
	out=$("$CLIPS_BIN" -f2 "$probe" 2>&1)
	rm -f "$probe"

	case "$out" in
		*"Missing function declaration"*) return 1 ;;
		*) return 0 ;;
	esac
}

# The (mimetype) UDF only exists in a MAGIC=1 build. Tests that need it carry
# a "requires: libmagic" marker and are skipped otherwise.
if has_function mimetype; then
	HAVE_LIBMAGIC=1
else
	HAVE_LIBMAGIC=0
fi

# The tls-* functions exist only in a TLS build. A test that needs them has a
# "requires: tls" marker, and the runner skips that test in other builds.
if has_function tls-backend; then
	HAVE_TLS=1
	NO_TLS_REASON=
else
	HAVE_TLS=0
	NO_TLS_REASON="built without TLS"
fi

# DTLS is a property of the library and not of the build, and the two
# directions differ. s2n-tls implements no DTLS. A backend with no stateless
# cookie exchange can be a DTLS client but cannot be a server. A test that
# needs DTLS has a "requires: dtls" marker.
#
# The script asks the library and does not use a list here. As a result, a
# backend that adds DTLS starts to run these tests, and no one must remember to
# change this file. The script asks about the server, because the server needs
# more. Each DTLS test to this day runs the two ends in one process.
HAVE_DTLS=0
NO_DTLS_REASON="built without TLS"

if [ "$HAVE_TLS" -eq 1 ]; then
	probe=$(mktemp)
	printf '(if (tls-supports-dtls DTLS_SERVER) then (printout t "yes" crlf))\n(exit)\n' > "$probe"
	if "$CLIPS_BIN" -f2 "$probe" 2>/dev/null | grep -q '^yes'; then
		HAVE_DTLS=1
		NO_DTLS_REASON=
	else
		NO_DTLS_REASON="this TLS backend has no DTLS server"
	fi
	rm -f "$probe"
fi

# The TLS tests need a certificate authority and a server certificate. A script
# makes those files, and the repository does not hold them.
# tests/fixtures/regenerate.sh gives the cause. As a result, a new checkout has
# none of them, and this code makes them at the first run. The user needs no
# separate setup step.
if [ "$HAVE_TLS" -eq 1 ]; then
	# A missing file of any of these names makes the script run again, and
	# the script writes all of them.
	for f in ca.pem ca-key.pem server.pem server-key.pem client.pem client-key.pem \
	         wildcard.pem wildcard-key.pem bad/not-a-pem.txt; do
		[ -f "tests/fixtures/$f" ] && continue

		if ! command -v openssl >/dev/null 2>&1; then
			HAVE_TLS=0
			NO_TLS_REASON="no openssl to generate certificates"
			break
		fi

		echo "generating TLS certificates in tests/fixtures ..."
		if ! sh tests/fixtures/regenerate.sh >/dev/null; then
			HAVE_TLS=0
			NO_TLS_REASON="could not generate certificates"
		fi
		break
	done
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
checks_skipped=0
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

	if grep -q 'requires: tls' "$t" && [ "$HAVE_TLS" -eq 0 ]; then
		echo "  SKIP $t ($NO_TLS_REASON)"
		skipped=$((skipped + 1))
		continue
	fi

	if grep -q 'requires: dtls' "$t" && [ "$HAVE_DTLS" -eq 0 ]; then
		echo "  SKIP $t ($NO_DTLS_REASON)"
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
		# These are the checks that the backend cannot do. The count is
		# separate from the file count above, and that count tells if a file
		# ran.
		nskip=$(printf '%s\n' "$out" | sed -n 's/^##SUMMARY .*skipped=\([0-9]*\).*/\1/p')
		nskip=${nskip:-0}
		checks_skipped=$((checks_skipped + nskip))
		if [ "$nskip" -gt 0 ]; then
			printf '%s\n' "$out" | grep -E '^  SKIP |^       ' | sed -n '1,99p'
			echo "  PASS $t (${counts:-0} checks, $nskip skipped)"
		else
			echo "  PASS $t (${counts:-0} checks)"
		fi
		passed=$((passed + 1))
	else
		echo "  FAIL $t ($reason)"
		printf '%s\n' "$out" | sed 's/^/       | /'
		failed=$((failed + 1))
		failed_names="$failed_names $t"
	fi
done

echo
if [ "$checks_skipped" -gt 0 ]; then
	echo "files: $passed passed, $failed failed, $skipped skipped; checks: $checks_skipped skipped"
else
	echo "files: $passed passed, $failed failed, $skipped skipped"
fi
if [ "$failed" -ne 0 ]; then
	echo "failing:$failed_names"
	exit 1
fi
exit 0
