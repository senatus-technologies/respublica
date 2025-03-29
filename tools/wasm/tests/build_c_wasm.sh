#!/bin/bash

set -e
set -x

CDT_INSTALL_PATH=~/opt/koinos-cdt
KOINOS_WASI_SDK_ROOT=~/opt/wasi-sdk-12.0

"$KOINOS_WASI_SDK_ROOT/bin/clang" \
   \
   -v \
   --sysroot="$KOINOS_WASI_SDK_ROOT/share/wasi-sysroot" \
   --target=wasm32-wasi \
   -L$CDT_INSTALL_PATH/lib \
   -I$CDT_INSTALL_PATH/include \
   -Wl,--no-entry \
   -Wl,--allow-undefined \
   \
   -O2 \
   \
   -o $2 \
   $1
