# "make help" lists each target with a "##" comment. The text after ## is the
# one-line summary.
all: ## Build clips with TLS (the default)
	make -C src/
magic: ## Build with the optional libmagic dependency, enabling the (mimetype) function
	make -C src/ MAGIC=1
# TLS is on by default. As a result, this target is another name for "make",
# and it stays here for instructions that use it. Select a backend with
# TLS_BACKEND=openssl|libressl|wolfssl|mbedtls|gnutls|s2n|boringssl. For an
# install at a different location, use TLS_PREFIX=/path. A variable on this
# command line reaches the sub-make, and "make tls MAGIC=1" also operates.
tls: ## Synonym for the default build
	make -C src/ TLS=1
# Builds with no TLS. This build needs no library except the C standard
# library.
no-tls: ## Build with no TLS
	make -C src/ TLS=0
# Deprecated alias for the default build, which already omits libmagic.
NO_IMAGE_MAGICK:
	make -C src/
test: ## Run the test suite against the current build
	./tests/run.sh
# Builds each configuration that this machine can build and runs the suite on
# each of them. The configurations are the build with no TLS, and each TLS
# library that is present, with libmagic and without libmagic.
matrix: ## Build and test every configuration this machine can build
	./tests/matrix.sh
# Builds with AddressSanitizer and runs the suite on that build. It finds
# defects that a check cannot find. Takes a configuration name
# as the scripts above do: "./tests/asan.sh gnutls".
asan: ## Rebuild under AddressSanitizer and run the suite
	./tests/asan.sh
# Lists all configurations that can be built.
test-list: ## Name the configurations this machine can build
	./tests/backend.sh list
test-%: ## Build and test one configuration, e.g. "make test-gnutls"
	./tests/backend.sh $*
# What the matrix can build on this machine, and how to get the other
# libraries. This target only writes a report.
# "./tests/provision.sh --build" builds the libraries.
provision: ## Report which TLS libraries are present and how to get the rest
	./tests/provision.sh
coverage: ## Line coverage for the current build's TLS backend
	make -C src/ clean
	make -C src/ coverage MAGIC=1
	-./tests/run.sh
	./tests/coverage.sh
coverage-all: ## Line coverage merged across every TLS library
	./tests/coverage-all.sh
clean: ## Remove the binary and every object file
	make -C src/ clean
	rm -f ./clips

help: ## List these targets
	@printf 'CLIPSockets targets:\n\n'
	@grep -E '^[a-zA-Z0-9_%-]+:.*## ' $(MAKEFILE_LIST) \
		| awk -F':.*## ' '{ printf "  %-14s %s\n", $$1, $$2 }' \
		| sort
	@printf '\nVariables: TLS_BACKEND, TLS_PREFIX, MAGIC.\n'
	@printf 'See the Installation section of README.md for what each accepts.\n'

.PHONY: all magic tls no-tls NO_IMAGE_MAGICK test matrix asan test-list provision \
	coverage coverage-all clean help
