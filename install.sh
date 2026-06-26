#!/bin/sh
set -e

if [ ! -f lil.c ]; then
    echo "error: run this script from the lil project directory"
    exit 1
fi

if command -v gcc >/dev/null 2>&1; then
    CC=gcc
elif command -v cc >/dev/null 2>&1; then
    CC=cc
else
    echo "error: no C compiler found"
    exit 1
fi

echo "building lil..."
$CC -Wall -Wextra -O2 -std=c99 -o lil lil.c

PREFIX="${1:-$HOME/.local/bin}"
mkdir -p "$PREFIX"
cp lil "$PREFIX/lil"
chmod +x "$PREFIX/lil"

echo "installed lil to $PREFIX/lil"
echo "make sure $PREFIX is in your PATH"
