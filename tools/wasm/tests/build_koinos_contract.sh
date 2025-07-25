#!/bin/bash

set -e
set -x

CDT_INSTALL_PATH=~/opt/respublica-cdt
RESPUBLICA_WASI_SDK_ROOT=~/opt/wasi-sdk-12.0

"$RESPUBLICA_WASI_SDK_ROOT/bin/clang++" \
   \
   -v \
   --sysroot="$RESPUBLICA_WASI_SDK_ROOT/share/wasi-sysroot" \
   --target=wasm32-wasi \
   -L$CDT_INSTALL_PATH/lib \
   -I$CDT_INSTALL_PATH/include \
   -L$RESPUBLICA_WASI_SDK_ROOT/share/wasi-sysroot/lib/wasm32-wasi \
   -I$RESPUBLICA_WASI_SDK_ROOT/share/wasi-sysroot/include \
   -lrespublica_proto_embedded \
   -lrespublica_api \
   -lrespublica_api_cpp \
   -lrespublica_wasi_api \
   -Wl,--no-entry \
   -Wl,--allow-undefined \
   \
   -O3 \
   -std=c++17 \
   \
   -o $2 \
   $1
