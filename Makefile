SHELL := bash
.SHELLFLAGS := -euo pipefail -c
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

CMAKE := cmake
CTEST := ctest
RELEASE_PRESETS := x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release

.PHONY: help build test debug clangd-surface asan tsan msan fuzz-smoke release verify-release-archives clean

help:
	@printf '%s\n' \
		'make build     Configure and build all shipped Linux dependency bundles.' \
		'make test      Run ABI/link smoke tests for built Linux bundles.' \
		'make debug     Build and test the host debug preset.' \
		'make clangd-surface Verify compile_commands and public hover comments for examples.' \
		'make asan      Build and test the facade-only AddressSanitizer/UBSan preset.' \
		'make tsan      Build and test the facade-only ThreadSanitizer preset.' \
		'make msan      Build and test the facade-only MemorySanitizer preset with clang.' \
		'make fuzz-smoke Build and run a bounded facade-only fuzz smoke.' \
		'make release   Build, package, and verify release bundles; adds Darwin when osxcross is available.' \
		'make verify-release-archives  Assert package contents and checksums for produced bundles.' \
		'make clean     Remove generated build, cache, and dist output.'

build:
	@for preset in $(RELEASE_PRESETS); do \
		$(CMAKE) --preset "$$preset"; \
		$(CMAKE) --build --preset "$$preset"; \
	done

test: build
	@for preset in $(RELEASE_PRESETS); do \
		$(CTEST) --preset "$$preset"; \
	done

debug:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	$(CTEST) --preset debug

clangd-surface:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	bash ./scripts/verify-clangd-surface.sh "$$(pwd)" "$$(pwd)/build/debug"

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

release:
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
	bash ./scripts/package-verify.sh
verify-release-archives:
	bash ./scripts/package-verify.sh

clean:
	rm -rf build .cache dist
