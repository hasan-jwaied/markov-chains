# Convenience wrapper around the CMake presets and the web app.
#   make            build and test natively (Debug, strict warnings)
#   make asan       build and test with AddressSanitizer + UBSan
#   make valgrind   run the Valgrind suite (zero leaks)
#   make bench      before/after benchmarks (Release)
#   make wasm       compile the engine to WebAssembly (needs emsdk)
#   make web        build the site into web/dist (runs `make wasm` first)
#   make dev        start the web dev server

.PHONY: all build test asan valgrind coverage bench wasm web dev clean

all: test

build:
	cmake --preset dev
	cmake --build --preset dev -j

test: build
	ctest --preset dev -LE valgrind

asan:
	cmake --preset asan
	cmake --build --preset asan -j
	ctest --preset asan

valgrind: build
	ctest --preset dev -L valgrind

coverage:
	cmake --preset coverage
	cmake --build --preset coverage -j
	ctest --preset coverage

bench:
	cmake --preset release
	cmake --build --preset release -j
	cmake --build --preset release --target benchmark

wasm:
	cmake --preset wasm
	cmake --build --preset wasm
	node tests/wasm/golden_wasm.mjs

web: wasm
	cd web && npm ci && npm run check && npm test && npm run build

dev: wasm
	cd web && npm install && npm run dev

clean:
	rm -rf build web/dist
