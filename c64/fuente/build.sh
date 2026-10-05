#!/bin/sh
# Builds dist/c64.wasm with plain clang + wasm-ld (no emscripten, no libc).
set -e
cd "$(dirname "$0")"
OUT=../dist/c64.wasm
CFLAGS="--target=wasm32 -O2 -ffreestanding -nostdlib -fno-exceptions -Iinclude -Ichips -fvisibility=hidden -DNDEBUG"
clang $CFLAGS -c wrapper.c -o build_wrapper.o
clang $CFLAGS -fno-builtin -c libc.c -o build_libc.o
wasm-ld --no-entry --strip-all --export-dynamic -z stack-size=262144 --initial-memory=2097152 \
    build_wrapper.o build_libc.o -o $OUT
ls -l $OUT
rm -f build_wrapper.o build_libc.o
