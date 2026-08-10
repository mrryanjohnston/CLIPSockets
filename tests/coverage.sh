#!/bin/sh
# Line coverage for the code CLIPSockets adds to CLIPS.
#
#   make coverage            build instrumented, run tests, print this report
#   ./tests/coverage.sh      report from an existing instrumented run
#
# Environment:
#   MIN_COVERAGE   fail with exit 1 below this total percentage (default 0)
#
# Everything in socketrtr.c and userfunctions.c is counted. The functions
# registered with AddUDF are listed by their CLIPS name, read straight out of
# the registration table so a newly added UDF appears here automatically -- at
# 0% until someone tests it. The rest are the router callbacks, the syscall
# wrappers and the lookup helpers those UDFs are built on: not callable from
# CLIPS, but as much a part of this library as the UDFs themselves.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT/src" || exit 1

MIN_COVERAGE=${MIN_COVERAGE:-0}
SOURCES="socketrtr.c userfunctions.c"

if ! ls *.gcda >/dev/null 2>&1; then
	echo "coverage.sh: no .gcda files in src/ -- build with 'make coverage' first" >&2
	exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

# UDF name -> C function name, straight from the registration table.
awk -F'"' '/AddUDF\(env,/ {
	n = split($0, p, ",")
	cfn = p[n-2]
	gsub(/^[ \t]+|[ \t]+$/, "", cfn)
	print cfn "\t" $2
}' userfunctions.c | sort > "$work/roster"

# C function name -> percentage, lines. "-n" keeps gcov from littering the
# tree with .gcov files we do not read.
gcov -f -n $SOURCES 2>/dev/null | awk '
	/^Function/ {
		fn = $2
		gsub(/'\''/, "", fn)
		next
	}
	/^Lines executed:/ && fn != "" {
		split($0, a, /[:%]/)
		split($0, b, / of /)
		print fn "\t" a[2] "\t" b[2]
		fn = ""
	}
' | sort > "$work/gcov"

join -t "$(printf '\t')" -a 1 "$work/roster" "$work/gcov" > "$work/udfs"
join -t "$(printf '\t')" -v 2 "$work/roster" "$work/gcov" > "$work/support"

# Sorted by coverage ascending so the least-tested code reads first.
printf '%-28s %-36s %6s %8s\n' "UDF" "C FUNCTION" "LINES" "COVERED"
printf '%s\n' "--------------------------------------------------------------------------------"
sort -t "$(printf '\t')" -k3,3n "$work/udfs" | awk -F'\t' '
	{
		if ($3 == "") { printf "%-28s %-36s %6s %8s\n", $2, $1, "-", "not built"; notbuilt++; next }
		lines += $4; covered += $4 * $3 / 100
		printf "%-28s %-36s %6d %7.1f%%%s\n", $2, $1, $4, $3, ($3 + 0 == 0 ? "  <-- UNTESTED" : "")
		if ($3 + 0 == 0) untested++
	}
	END {
		printf "%-28s %-36s %6d %7.1f%%\n", "subtotal (" NR - notbuilt " UDFs)", "", lines, (lines ? covered * 100 / lines : 0)
		print lines "\t" covered > "'"$work"'/udf-totals"
		if (untested) printf "%d UDF(s) never executed by the suite.\n", untested
		if (notbuilt) printf "%d UDF(s) not present in this build (compiled out).\n", notbuilt
	}'

echo
printf '%-38s %-26s %6s %8s\n' "SUPPORTING FUNCTION" "" "LINES" "COVERED"
printf '%s\n' "--------------------------------------------------------------------------------"
sort -t "$(printf '\t')" -k2,2n "$work/support" | awk -F'\t' '
	$2 != "" {
		lines += $3; covered += $3 * $2 / 100
		printf "%-38s %-26s %6d %7.1f%%%s\n", $1, "", $3, $2, ($2 + 0 == 0 ? "  <-- UNTESTED" : "")
		if ($2 + 0 == 0) untested++
	}
	END {
		printf "%-38s %-26s %6d %7.1f%%\n", "subtotal (" NR " functions)", "", lines, (lines ? covered * 100 / lines : 0)
		print lines "\t" covered > "'"$work"'/support-totals"
		if (untested) printf "%d supporting function(s) never executed by the suite.\n", untested
	}'

total=$(awk -F'\t' '{ lines += $1; covered += $2 } END { printf "%.1f", (lines ? covered * 100 / lines : 0) }' \
	"$work/udf-totals" "$work/support-totals")
total_lines=$(awk -F'\t' '{ lines += $1 } END { print lines }' "$work/udf-totals" "$work/support-totals")

echo
printf '%s\n' "--------------------------------------------------------------------------------"
printf '%-38s %-26s %6d %7.1f%%\n' "TOTAL" "" "$total_lines" "$total"

echo
if awk "BEGIN { exit !($total < $MIN_COVERAGE) }"; then
	echo "FAIL: total coverage ${total}% is below MIN_COVERAGE=${MIN_COVERAGE}%"
	exit 1
fi
echo "total line coverage: ${total}% (MIN_COVERAGE=${MIN_COVERAGE}%)"
exit 0
