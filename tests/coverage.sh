#!/bin/sh
# Line coverage for the code CLIPSockets adds to CLIPS.
#
#   make coverage            build instrumented, run tests, print this report
#   ./tests/coverage.sh      report from an existing instrumented run
#
# Everything in socketrtr.c and userfunctions.c is counted. The functions
# registered with AddUDF are listed by their CLIPS name, read straight out of
# the registration table so a newly added UDF appears here automatically -- at
# 0% until someone tests it. The rest are the router callbacks, the syscall
# wrappers and the lookup helpers those UDFs are built on: not callable from
# CLIPS, but as much a part of this library as the UDFs themselves.

set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# These two directories come from the caller, and a relative one names a
# directory beside the caller. This script works in src/, where gcov keeps its
# data, and the same relative name there is a different directory. As a result,
# the script makes each of them absolute before it moves.
#
# tests/coverage-all.sh passes the path of a temporary directory, which is
# absolute, and never met this. The CI job passes "coverage-data". Without
# these two lines the data goes to src/coverage-data, and the job that adds it
# all together looks in the directory that it named and finds nothing.
COVERAGE_COLLECT=${COVERAGE_COLLECT:-}
COVERAGE_MERGE=${COVERAGE_MERGE:-}

case $COVERAGE_COLLECT in
	""|/*) ;;
	*) COVERAGE_COLLECT=$PWD/$COVERAGE_COLLECT ;;
esac

case $COVERAGE_MERGE in
	""|/*) ;;
	*) COVERAGE_MERGE=$PWD/$COVERAGE_MERGE ;;
esac

cd "$ROOT/src" || exit 1

# One build has one TLS backend. As a result, the sources to measure are the
# sources that this build made coverage data for. A list with each source would
# report the absent backends as zero per cent, and that would count code that
# the build correctly left out.
SOURCES="socketrtr.c userfunctions.c"
for candidate in socktls.c socktls-openssl.c socktls-mbedtls.c socktls-gnutls.c socktls-s2n.c; do
	[ -f "${candidate%.c}.gcda" ] && SOURCES="$SOURCES $candidate"
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

# This script reads the coverage from the JSON of gcov and not from its text
# report. The JSON lets the script add several builds together. It gives the
# line range of each function and the execution count of each line. As a
# result, a line is covered when any build covered it. A backend that one build
# leaves out is measured in the build that has it.
collect_json () {
	if ! ls *.gcda >/dev/null 2>&1; then
		echo "coverage.sh: no .gcda files in src/ -- build with 'make coverage' first" >&2
		exit 2
	fi

	rm -f *.gcov.json.gz
	gcov --json-format $SOURCES >/dev/null 2>&1

	mkdir -p "$1"
	for j in *.gcov.json.gz; do
		[ -e "$j" ] || continue
		cp "$j" "$1/"
	done
	rm -f *.gcov.json.gz
}

if [ -n "$COVERAGE_COLLECT" ]; then
	collect_json "$COVERAGE_COLLECT"
	exit 0
fi

if [ -n "$COVERAGE_MERGE" ]; then
	data=$COVERAGE_MERGE
else
	data=$work/single
	collect_json "$data"
fi

found=$(find "$data" -name '*.gcov.json.gz' | wc -l)
if [ "$found" -eq 0 ]; then
	echo "coverage.sh: no coverage data under $data" >&2
	exit 2
fi

# UDF name -> C function name, straight from the registration table.
awk -F'"' '/AddUDF\(env,/ {
	n = split($0, p, ",")
	cfn = p[n-2]
	gsub(/^[ \t]+|[ \t]+$/, "", cfn)
	print cfn "\t" $2
}' userfunctions.c | sort > "$work/roster"

# Changes each collected run into two types of record: one execution count for
# each source line, and one line range for each function.
find "$data" -name '*.gcov.json.gz' | while read -r j; do
	gzip -dc "$j" | jq -r '
		.files[] | .file as $f |
		( (.lines[]?     | "L\t\($f)\t\(.line_number)\t\(.count)"),
		  (.functions[]? | "F\t\($f)\t\(.start_line)\t\(.end_line)\t\(.name)") )'
done > "$work/flat"

# The name of a C function gives a percentage and a line count. A line is
# covered if any run covered it. The lines of a function are the lines with
# code inside its range.
awk -F'\t' '
	$1 == "L" {
		key = $2 SUBSEP $3
		seen[key] = 1
		if ($4 + 0 > count[key]) count[key] = $4 + 0
		next
	}
	$1 == "F" { range[$2 SUBSEP $3 SUBSEP $4] = $5 }
	END {
		for (k in range) {
			split(k, p, SUBSEP)
			lines = 0; covered = 0
			for (l = p[2] + 0; l <= p[3] + 0; l++) {
				kk = p[1] SUBSEP l
				if (! (kk in seen)) continue
				lines++
				if (count[kk] > 0) covered++
			}
			if (lines > 0) printf "%s\t%.2f\t%d\n", range[k], covered * 100 / lines, lines
		}
	}' "$work/flat" | sort > "$work/gcov"

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
echo "total line coverage: ${total}%"
exit 0
