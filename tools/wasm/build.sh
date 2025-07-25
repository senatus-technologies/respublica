#!/bin/bash

set -e
set -x

# Run this program manually to generate vm_fixture.hpp
# We should really replace this script with a better, more automated process
respublica_wasm_test_dir=../tests/include/respublica/tests/wasm

mkdir -p $respublica_wasm_test_dir/stack

for file in tests/*.cpp; do
   target_name=$(basename $file .cpp)
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
      -o $target_name.wasm \
      $file

   xxd -i $target_name.wasm > $respublica_wasm_test_dir/$target_name.hpp
   rm $target_name.wasm
done

for file in tests/stack/*.cpp; do
   target_name=$(basename $file .cpp)
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
      -o $target_name.wasm \
      $file

   xxd -i $target_name.wasm > $respublica_wasm_test_dir/stack/$target_name.hpp
   rm $target_name.wasm
done
