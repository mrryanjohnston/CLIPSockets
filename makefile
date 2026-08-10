all:
	make -C src/
# Build with the optional libmagic dependency, enabling the (mimetype) function.
magic:
	make -C src/ MAGIC=1
# Deprecated alias for the default build, which already omits libmagic.
NO_IMAGE_MAGICK:
	make -C src/
test:
	./tests/run.sh
# Regression floor for per-UDF line coverage. Raise it as tests are added;
# never lower it to make a build pass. Override for a one-off run with
# "make coverage MIN_COVERAGE=0".
MIN_COVERAGE ?= 95
# Instrumented build + suite + per-UDF line coverage. Builds from clean because
# objects are compiled in place and would otherwise be reused uninstrumented.
coverage:
	make -C src/ clean
	make -C src/ coverage MAGIC=1
	-./tests/run.sh
	MIN_COVERAGE=$(MIN_COVERAGE) ./tests/coverage.sh
clean:
	make -C src/ clean
	rm -f ./clips

.PHONY: all magic NO_IMAGE_MAGICK test coverage clean
