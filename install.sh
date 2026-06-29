#!/bin/sh
set -e

if [ ! -f lil.c ]; then
    echo "error: run this script from the lil project directory"
    exit 1
fi

PREFIX="${1:-$HOME/.local}"
echo "building and installing lil to $PREFIX/bin..."
make install PREFIX="$PREFIX"
echo "make sure $PREFIX/bin is in your PATH"
