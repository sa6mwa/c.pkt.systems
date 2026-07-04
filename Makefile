SHELL := bash
.SHELLFLAGS := -euo pipefail -c
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

CMAKE := cmake
CTEST := ctest
RELEASE_PRESETS := x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release
E2E_SUS_PRESET ?= release

.PHONY: help deps-debug deps-release deps-cross build build-debug build-release test test-debug test-all debug examples clangd-surface e2e-sus example-audio-vox-intro example-sus-vox-intro asan tsan msan fuzz-smoke fuzz package package-source package-source-smoke package-checksums package-verify verify-release-archives verify-release-privacy prerelease prerelease-hardening release-matrix release source-archive verify-source-archive clean clean-dist

help:
	@printf 'Usage: make <target>\n\n'
	@printf 'Core:\n'
	@printf '  %-30s %s\n' 'deps-debug' 'Configure the host debug dependency/build graph.'
	@printf '  %-30s %s\n' 'deps-release' 'Configure all shipped Linux release dependency/build graphs.'
	@printf '  %-30s %s\n' 'deps-cross' 'Configure cross release dependency/build graphs.'
	@printf '  %-30s %s\n' 'build' 'Configure and build all shipped Linux dependency bundles.'
	@printf '  %-30s %s\n' 'build-debug' 'Build the host debug preset.'
	@printf '  %-30s %s\n' 'build-release' 'Build all shipped Linux release bundles.'
	@printf '  %-30s %s\n' 'debug' 'Build and test the host debug preset.'
	@printf '\nTests:\n'
	@printf '  %-30s %s\n' 'test' 'Run ABI/link smoke tests for built Linux bundles.'
	@printf '  %-30s %s\n' 'test-debug' 'Run the host debug tests.'
	@printf '  %-30s %s\n' 'test-all' 'Run the full local confidence gate.'
	@printf '  %-30s %s\n' 'examples' 'Build and smoke-test source-tree examples.'
	@printf '  %-30s %s\n' 'clangd-surface' 'Verify compile_commands and public hover comments for examples.'
	@printf '  %-30s %s\n' 'e2e-sus' 'Run opt-in sus audio e2e with cached remote MP3 and tiny model.'
	@printf '  %-30s %s\n' 'example-audio-vox-intro' 'Run cached intro.mp3 VOX calibration and dump WAV segments.'
	@printf '  %-30s %s\n' 'example-sus-vox-intro' 'Run cached intro.mp3 VOX transcription and print streamed text.'
	@printf '  %-30s %s\n' 'asan' 'Build and test the facade-only AddressSanitizer/UBSan preset.'
	@printf '  %-30s %s\n' 'tsan' 'Build and test the facade-only ThreadSanitizer preset.'
	@printf '  %-30s %s\n' 'msan' 'Build and test the facade-only MemorySanitizer preset with clang.'
	@printf '  %-30s %s\n' 'fuzz-smoke' 'Build and run bounded facade fuzz smoke tests.'
	@printf '  %-30s %s\n' 'fuzz' 'Build and run bounded facade fuzz tests.'
	@printf '\nPackaging:\n'
	@printf '  %-30s %s\n' 'package' 'Build package artifacts for all supported release targets.'
	@printf '  %-30s %s\n' 'package-source' 'Build the source release archive.'
	@printf '  %-30s %s\n' 'package-source-smoke' 'Verify the source release archive.'
	@printf '  %-30s %s\n' 'package-checksums' 'Verify the checksum manifest covers release artifacts.'
	@printf '  %-30s %s\n' 'package-verify' 'Verify package layout, checksums, privacy, and install-tree consumers.'
	@printf '  %-30s %s\n' 'verify-release-archives' 'Alias for package-verify.'
	@printf '  %-30s %s\n' 'verify-release-privacy' 'Alias for package-verify; privacy is part of the package gate.'
	@printf '\nRelease:\n'
	@printf '  %-30s %s\n' 'prerelease' 'Run deterministic local pre-release confidence.'
	@printf '  %-30s %s\n' 'prerelease-hardening' 'Run expensive local pre-release confidence.'
	@printf '  %-30s %s\n' 'release-matrix' 'Build, package, checksum, and verify all release artifacts.'
	@printf '  %-30s %s\n' 'release' 'Clean, build, package, and verify the final local release gate.'
	@printf '\nCleanup:\n'
	@printf '  %-30s %s\n' 'clean' 'Remove generated build, cache, and dist output.'
	@printf '  %-30s %s\n' 'clean-dist' 'Remove only release artifacts under dist/.'

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

examples:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_lua_runtime_c89_example cpkt_opcua_c89_example cpkt_audio_sus_c89_example cpkt_audio_vox_intro_c89_example cpkt_sus_vox_intro_c89_example cpkt_abi_smoke_shared cpkt_abi_smoke_static cpkt_mqttc_smoke_shared cpkt_mqttc_smoke_static cpkt_whisper_smoke_shared
	$(CTEST) --preset debug -R 'example' --output-on-failure

clangd-surface:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	bash ./scripts/verify-clangd-surface.sh "$$(pwd)" "$$(pwd)/build/debug"

e2e-sus:
	$(CMAKE) --preset $(E2E_SUS_PRESET)
	$(CMAKE) --build --preset $(E2E_SUS_PRESET) --target cpkt_sus_audio_integration_test
	bash ./scripts/e2e-sus.sh "$$(pwd)/build/$(E2E_SUS_PRESET)/cpkt_sus_audio_integration_test" "$$(pwd)/build/$(E2E_SUS_PRESET)"

example-audio-vox-intro:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_audio_vox_intro_c89_example
	bash ./scripts/run-audio-vox-intro.sh "$$(pwd)/build/debug/cpkt_audio_vox_intro_c89_example"

example-sus-vox-intro:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_sus_vox_intro_c89_example
	bash ./scripts/run-sus-vox-intro.sh "$$(pwd)/build/debug/cpkt_sus_vox_intro_c89_example" "$$(pwd)/build/debug"

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
