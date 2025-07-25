#!/bin/bash

set -e
set -x

CDT_INSTALL_PATH=~/opt/respublica-cdt
RESPUBLICA_WASI_SDK_ROOT=~/opt/wasi-sdk-12.0

"$RESPUBLICA_WASI_SDK_ROOT/bin/clang" \
   \
   -v \
   --sysroot="$RESPUBLICA_WASI_SDK_ROOT/share/wasi-sysroot" \
   --target=wasm32-wasi \
   -L$CDT_INSTALL_PATH/lib \
   -I$CDT_INSTALL_PATH/include \
   -Wl,--no-entry \
   -Wl,--export=_start \
   -Wl,--export=__data_end \
   -Wl,--export=__heap_base \
   -Wl,--export=malloc \
   -Wl,--export=free \
   -Wl,--allow-undefined \
   -Wl,--strip-all \
   \
   -g \
   -O2 \
   \
   -o $2 \
   $1
