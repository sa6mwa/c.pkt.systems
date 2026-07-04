SHELL := bash
.SHELLFLAGS := -euo pipefail -c
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

CMAKE := cmake
CTEST := ctest
RELEASE_PRESETS := x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release

.PHONY: help deps-debug deps-release deps-cross build build-debug build-release test test-debug test-all debug clangd-surface e2e-sus asan tsan msan fuzz-smoke fuzz package package-source package-source-smoke package-checksums package-verify verify-release-archives verify-release-privacy prerelease prerelease-hardening release-matrix release source-archive verify-source-archive clean clean-dist

help:
	@printf '%s\n' \
		'make deps-debug Configure the host debug dependency/build graph.' \
		'make deps-release Configure all shipped Linux release dependency/build graphs.' \
		'make deps-cross Configure cross release dependency/build graphs.' \
		'make build     Configure and build all shipped Linux dependency bundles.' \
		'make build-debug Build the host debug preset.' \
		'make build-release Build all shipped Linux release bundles.' \
		'make test      Run ABI/link smoke tests for built Linux bundles.' \
		'make test-debug Run the host debug tests.' \
		'make test-all  Run the full local confidence gate.' \
		'make debug     Build and test the host debug preset.' \
		'make clangd-surface Verify compile_commands and public hover comments for examples.' \
		'make e2e-sus   Run opt-in sus audio e2e with cached remote MP3 and tiny model.' \
		'make asan      Build and test the facade-only AddressSanitizer/UBSan preset.' \
		'make tsan      Build and test the facade-only ThreadSanitizer preset.' \
		'make msan      Build and test the facade-only MemorySanitizer preset with clang.' \
		'make fuzz-smoke Build and run bounded facade fuzz smoke tests.' \
		'make fuzz      Build and run bounded facade fuzz tests.' \
		'make package   Build package artifacts for all supported release targets.' \
		'make package-source Build the source release archive.' \
		'make package-source-smoke Verify the source release archive.' \
		'make package-checksums Verify the checksum manifest covers release artifacts.' \
		'make package-verify Verify package layout, checksums, privacy, and install-tree consumers.' \
		'make verify-release-archives Alias for package-verify.' \
		'make verify-release-privacy Alias for package-verify; privacy is part of the package gate.' \
		'make prerelease Run deterministic local pre-release confidence.' \
		'make prerelease-hardening Run expensive local pre-release confidence.' \
		'make release-matrix Build, package, checksum, and verify all release artifacts.' \
		'make release   Clean, build, package, and verify the final local release gate.' \
		'make clean     Remove generated build, cache, and dist output.' \
		'make clean-dist Remove only release artifacts under dist/.'

deps-debug:
	$(CMAKE) --preset debug

deps-release:
	@for preset in $(RELEASE_PRESETS); do \
		$(CMAKE) --preset "$$preset"; \
	done

deps-cross:
	@for preset in aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release; do \
		$(CMAKE) --preset "$$preset"; \
	done
	@if bash ./scripts/osxcross_available.sh; then \
		$(CMAKE) --preset arm64-apple-darwin-release; \
	else \
		printf '[package] skipping arm64-apple-darwin-release configure: osxcross toolchain not available\n'; \
	fi

build:
	@for preset in $(RELEASE_PRESETS); do \
		$(CMAKE) --preset "$$preset"; \
		$(CMAKE) --build --preset "$$preset"; \
	done

build-debug:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug

build-release: build

test: build
	@for preset in $(RELEASE_PRESETS); do \
		$(CTEST) --preset "$$preset"; \
	done

test-debug: build-debug
	$(CTEST) --preset debug

test-all: debug clangd-surface asan tsan msan fuzz-smoke

debug:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	$(CTEST) --preset debug

clangd-surface:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	bash ./scripts/verify-clangd-surface.sh "$$(pwd)" "$$(pwd)/build/debug"

e2e-sus:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_sus_audio_integration_test
	bash ./scripts/e2e-sus.sh "$$(pwd)/build/debug/cpkt_sus_audio_integration_test" "$$(pwd)/build/debug"

asan:
	$(CMAKE) --preset asan
	$(CMAKE) --build --preset asan
	$(CTEST) --preset asan

tsan:
	$(CMAKE) --preset tsan
	$(CMAKE) --build --preset tsan
	$(CTEST) --preset tsan

msan:
	$(CMAKE) --preset msan
	$(CMAKE) --build --preset msan
	$(CTEST) --preset msan

fuzz-smoke:
	$(CMAKE) --preset fuzz
	$(CMAKE) --build --preset fuzz
	build/fuzz/cpkt_lua_runtime_fuzz -runs=256
	$(CMAKE) --preset opcua-fuzz
	$(CMAKE) --build --preset opcua-fuzz
	build/opcua-fuzz/cpkt_opcua_facade_fuzz -runs=256

fuzz:
	$(CMAKE) --preset fuzz
	$(CMAKE) --build --preset fuzz
	build/fuzz/cpkt_lua_runtime_fuzz -runs=100000
	$(CMAKE) --preset opcua-fuzz
	$(CMAKE) --build --preset opcua-fuzz
	build/opcua-fuzz/cpkt_opcua_facade_fuzz -runs=100000

package:
	@for preset in $(RELEASE_PRESETS); do \
		$(CMAKE) --preset "$$preset"; \
		$(CMAKE) --build --preset "$$preset"; \
		$(CTEST) --preset "$$preset"; \
		$(CMAKE) --build --preset "package-$$preset"; \
	done
	@if bash ./scripts/osxcross_available.sh; then \
		$(CMAKE) --preset arm64-apple-darwin-release; \
		$(CMAKE) --build --preset arm64-apple-darwin-release; \
		$(CMAKE) --build --preset package-arm64-apple-darwin-release; \
	else \
		printf '[package] skipping arm64-apple-darwin-release: osxcross toolchain not available\n'; \
	fi

package-source:
	bash ./scripts/package-source.sh

package-source-smoke: package-source
	bash ./scripts/source-archive-verify.sh "dist/c.pkt.systems-$$(bash ./scripts/release-version.sh "$$(pwd)").tar.gz"

package-checksums:
	bash ./scripts/verify-dist-manifest.sh "$$(pwd)/dist" c.pkt.systems "$$(bash ./scripts/release-version.sh "$$(pwd)")"

package-verify:
	bash ./scripts/package-verify.sh

verify-release-archives: package-verify

verify-release-privacy: package-verify

prerelease: debug clangd-surface asan fuzz-smoke

prerelease-hardening: prerelease tsan msan release-matrix

release-matrix: package package-source package-verify

release: clean release-matrix

source-archive: package-source-smoke

verify-source-archive:
	bash ./scripts/source-archive-verify.sh "dist/c.pkt.systems-$$(bash ./scripts/release-version.sh "$$(pwd)").tar.gz"

clean:
	rm -rf build .cache dist

clean-dist:
	rm -rf dist
