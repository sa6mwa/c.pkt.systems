SHELL := bash
.SHELLFLAGS := -euo pipefail -c
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

CMAKE := cmake
CTEST := ctest
RELEASE_PRESETS := x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release
E2E_SUS_PRESET ?= release
STATIC_LIVE_PRESET ?= x86_64-linux-musl-release

.PHONY: help deps-debug deps-release deps-cross build build-debug build-release build-host cross-build test test-debug test-host test-cross cross-test test-all test-install-tree debug examples clangd-surface e2e-sus e2e-cpktxscribe example-audio-vox-intro example-audio-live-vox example-audio-live-vox-static example-sus-vox-intro example-sus-live-vox example-sus-live-vox-static cpktxscribe valgrind fuzz-smoke fuzz package package-source package-source-smoke package-checksums package-verify verify-release-archives verify-release-privacy prerelease prerelease-hardening release-matrix finalize-slice release print-release-version format source-archive verify-source-archive clean clean-dist

help:
	@printf 'Usage: make <target>\n\n'
	@printf 'Core:\n'
	@printf '  %-30s %s\n' 'help' 'Show this command index.'
	@printf '  %-30s %s\n' 'deps-debug' 'Configure the host debug dependency/build graph.'
	@printf '  %-30s %s\n' 'deps-release' 'Configure all shipped Linux release dependency/build graphs.'
	@printf '  %-30s %s\n' 'deps-cross' 'Configure cross release dependency/build graphs.'
	@printf '  %-30s %s\n' 'build' 'Configure and build all shipped Linux dependency bundles.'
	@printf '  %-30s %s\n' 'build-debug' 'Build the host debug preset.'
	@printf '  %-30s %s\n' 'build-release' 'Build all shipped Linux release bundles.'
	@printf '  %-30s %s\n' 'build-host' 'Alias for build-debug.'
	@printf '  %-30s %s\n' 'cross-build' 'Alias for build-release.'
	@printf '  %-30s %s\n' 'debug' 'Build and test the host debug preset.'
	@printf '\nTests:\n'
	@printf '  %-30s %s\n' 'test' 'Run ABI/link smoke tests for built Linux bundles.'
	@printf '  %-30s %s\n' 'test-debug' 'Run the host debug tests.'
	@printf '  %-30s %s\n' 'test-host' 'Alias for test-debug.'
	@printf '  %-30s %s\n' 'test-cross' 'Run release preset tests for cross-capable targets.'
	@printf '  %-30s %s\n' 'cross-test' 'Alias for test-cross.'
	@printf '  %-30s %s\n' 'test-all' 'Run the full local confidence gate.'
	@printf '  %-30s %s\n' 'test-install-tree' 'Run install-tree package consumer smoke tests.'
	@printf '  %-30s %s\n' 'examples' 'Build and smoke-test source-tree examples.'
	@printf '  %-30s %s\n' 'clangd-surface' 'Verify compile_commands and public hover comments for examples.'
	@printf '  %-30s %s\n' 'e2e-sus' 'Run opt-in sus audio e2e with cached remote MP3 and tiny model.'
	@printf '  %-30s %s\n' 'e2e-cpktxscribe' 'Run opt-in cpktxscribe URL e2e with remote MP3 and tiny model.'
	@printf '  %-30s %s\n' 'example-audio-vox-intro' 'Run cached intro.mp3 VOX calibration and dump WAV segments.'
	@printf '  %-30s %s\n' 'example-audio-live-vox' 'Run live microphone VOX capture and dump WAV segments.'
	@printf '  %-30s %s\n' 'example-audio-live-vox-static' 'Build the musl static live VOX example and print its path.'
	@printf '  %-30s %s\n' 'example-sus-vox-intro' 'Run cached intro.mp3 VOX transcription and print streamed text.'
	@printf '  %-30s %s\n' 'example-sus-live-vox' 'Run live microphone VOX transcription and print streamed text.'
	@printf '  %-30s %s\n' 'example-sus-live-vox-static' 'Build the musl static live sus VOX example and print its path.'
	@printf '  %-30s %s\n' 'valgrind' 'Run the facade-only Valgrind Memcheck gate.'
	@printf '  %-30s %s\n' 'fuzz-smoke' 'Build and run bounded AFL++ GCC-plugin facade fuzz smoke tests.'
	@printf '  %-30s %s\n' 'fuzz' 'Build and run bounded AFL++ GCC-plugin facade fuzz tests.'
	@printf '\nTools:\n'
	@printf '  %-30s %s\n' 'cpktxscribe' 'Build host audio transcription CLI under build/debug/tools/.'
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
	@printf '  %-30s %s\n' 'finalize-slice' 'Format and run the narrow local pre-commit gate.'
	@printf '  %-30s %s\n' 'release' 'Clean, build, package, and verify the final local release gate.'
	@printf '  %-30s %s\n' 'print-release-version' 'Print the version used by package and release artifacts.'
	@printf '  %-30s %s\n' 'format' 'Format project-owned C and header files with clang-format.'
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
	bash ./scripts/build.sh release

build-debug:
	bash ./scripts/build.sh debug

build-release: build

build-host: build-debug

cross-build: build-release

test:
	bash ./scripts/test.sh release

test-debug:
	bash ./scripts/test.sh debug

test-host: test-debug

test-cross:
	bash ./scripts/test.sh release

cross-test: test-cross

test-install-tree: package-verify

test-all: debug clangd-surface valgrind fuzz-smoke

debug:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	$(CTEST) --preset debug

examples:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_lua_runtime_c89_example cpkt_opcua_c89_example cpkt_audio_sus_c89_example cpkt_audio_vox_intro_c89_example cpkt_audio_live_vox_c89_example cpkt_sus_vox_intro_c89_example cpkt_sus_live_vox_c89_example cpkt_abi_smoke_shared cpkt_abi_smoke_static cpkt_mqttc_smoke_shared cpkt_mqttc_smoke_static cpkt_whisper_smoke_shared
	$(CTEST) --preset debug -R 'example' --output-on-failure

clangd-surface:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug
	bash ./scripts/verify-clangd-surface.sh "$$(pwd)" "$$(pwd)/build/debug"

e2e-sus:
	$(CMAKE) --preset $(E2E_SUS_PRESET)
	$(CMAKE) --build --preset $(E2E_SUS_PRESET) --target cpkt_sus_audio_integration_test
	bash ./scripts/e2e-sus.sh "$$(pwd)/build/$(E2E_SUS_PRESET)/cpkt_sus_audio_integration_test" "$$(pwd)/build/$(E2E_SUS_PRESET)"

e2e-cpktxscribe:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpktxscribe
	bash ./scripts/e2e-cpktxscribe.sh "$$(pwd)/build/debug/tools/cpktxscribe" "$$(pwd)/build/debug"

example-audio-vox-intro:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_audio_vox_intro_c89_example
	bash ./scripts/run-audio-vox-intro.sh "$$(pwd)/build/debug/cpkt_audio_vox_intro_c89_example"

example-audio-live-vox:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_audio_live_vox_c89_example
	"$$(pwd)/build/debug/cpkt_audio_live_vox_c89_example" $(CPKT_AUDIO_LIVE_VOX_ARGS)

example-audio-live-vox-static:
	$(CMAKE) --preset $(STATIC_LIVE_PRESET)
	$(CMAKE) --build --preset $(STATIC_LIVE_PRESET) --target cpkt_audio_live_vox_static_c89_example
	@printf 'built: %s\n' "$$(pwd)/build/$(STATIC_LIVE_PRESET)/cpkt_audio_live_vox_static_c89_example"

example-sus-vox-intro:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_sus_vox_intro_c89_example
	bash ./scripts/run-sus-vox-intro.sh "$$(pwd)/build/debug/cpkt_sus_vox_intro_c89_example" "$$(pwd)/build/debug"

example-sus-live-vox:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpkt_sus_live_vox_c89_example
	"$$(pwd)/build/debug/cpkt_sus_live_vox_c89_example" $(CPKT_SUS_LIVE_VOX_ARGS)

example-sus-live-vox-static:
	$(CMAKE) --preset $(STATIC_LIVE_PRESET)
	$(CMAKE) --build --preset $(STATIC_LIVE_PRESET) --target cpkt_sus_live_vox_static_c89_example
	@printf 'built: %s\n' "$$(pwd)/build/$(STATIC_LIVE_PRESET)/cpkt_sus_live_vox_static_c89_example"

cpktxscribe:
	$(CMAKE) --preset debug
	$(CMAKE) --build --preset debug --target cpktxscribe
	@printf 'built: %s\n' "$$(pwd)/build/debug/tools/cpktxscribe"

valgrind:
	@command -v valgrind >/dev/null || { printf 'valgrind is required for make valgrind; install it with the host OS package manager\n' >&2; exit 1; }
	$(CMAKE) --preset valgrind
	$(CMAKE) --build --preset valgrind
	valgrind --error-exitcode=1 --leak-check=full --track-origins=yes --show-leak-kinds=definite,indirect build/valgrind/cpkt_lua_runtime_mock_test

fuzz-smoke:
	bash ./scripts/configure-preset.sh --fresh fuzz
	$(CMAKE) --build --preset fuzz
	bash ./scripts/run-afl-fuzz.sh smoke build/fuzz/cpkt_lua_runtime_fuzz fuzz/seeds/lua
	bash ./scripts/configure-preset.sh debug
	$(CMAKE) --build --preset debug --target cpkt_opcua_static
	bash ./scripts/configure-preset.sh --fresh opcua-fuzz
	$(CMAKE) --build --preset opcua-fuzz
	bash ./scripts/run-afl-fuzz.sh smoke build/opcua-fuzz/cpkt_opcua_facade_fuzz fuzz/seeds/opcua

fuzz:
	bash ./scripts/configure-preset.sh --fresh fuzz
	$(CMAKE) --build --preset fuzz
	bash ./scripts/run-afl-fuzz.sh standard build/fuzz/cpkt_lua_runtime_fuzz fuzz/seeds/lua
	bash ./scripts/configure-preset.sh debug
	$(CMAKE) --build --preset debug --target cpkt_opcua_static
	bash ./scripts/configure-preset.sh --fresh opcua-fuzz
	$(CMAKE) --build --preset opcua-fuzz
	bash ./scripts/run-afl-fuzz.sh standard build/opcua-fuzz/cpkt_opcua_facade_fuzz fuzz/seeds/opcua

package:
	bash ./scripts/package.sh

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

prerelease: debug clangd-surface valgrind fuzz-smoke

prerelease-hardening: prerelease fuzz release-matrix

release-matrix: package package-source package-checksums package-verify

finalize-slice: format debug clangd-surface

print-release-version:
	@bash ./scripts/release-version.sh "$$(pwd)"

format:
	@command -v clang-format >/dev/null || { printf 'clang-format is required for make format\n' >&2; exit 1; }
	find include src tests examples fuzz tools -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i

release: clean release-matrix

source-archive: package-source-smoke

verify-source-archive:
	bash ./scripts/source-archive-verify.sh "dist/c.pkt.systems-$$(bash ./scripts/release-version.sh "$$(pwd)").tar.gz"

clean:
	bash ./scripts/clean.sh all

clean-dist:
	bash ./scripts/clean.sh dist
