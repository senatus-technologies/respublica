// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(validation_stack, select_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      i32.const 0
      i32.const 1
      select
      unreachable ;; don't check stack after this
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800410041011b000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack_type, select_stack_underflow_2)
{
    /* wat2wasm --no-check
    (func (param i32) (result i32)
      i32.const 1
      local.get 0
      select
      i32.const 0
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017f030201000a0b010900410120001b41000b");

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, unreachable_select)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      i32.const 1
      select
    )
    */
    const auto wasm1 = from_hex("0061736d010000000105016000017f030201000a0a01080000410041011b0b");
    EXPECT_NO_THROW(parse(wasm1));

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      select
    )
    */
    const auto wasm2 = from_hex("0061736d010000000105016000017f030201000a080106000041001b0b");
    EXPECT_NO_THROW(parse(wasm2));

    /* wat2wasm
    (func (result i32)
      unreachable
      select
    )
    */
    const auto wasm3 = from_hex("0061736d010000000105016000017f030201000a06010400001b0b");
    EXPECT_NO_THROW(parse(wasm3));
}

TEST(validation_stack, func_stack_underflow)
{
    /* wat2wasm --no-check
    (func (param i32 i32) (result i32)
      local.get 0
      local.get 1
      i32.add
      i32.add
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001070160027f7f017f030201000a0a010800200020016a6a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, func_missing_result)
{
    /* wat2wasm --no-check
    (func (result i32)
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a040102000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, func_too_many_results)
{
    /* wat2wasm --no-check
    (func
      i64.const 0
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0601040042000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (param i64) (result i64)
      local.get 0
      local.get 0
    )
    */
    const auto wasm2 = from_hex("0061736d0100000001060160017e017e030201000a08010600200020000b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "too many results");
}

TEST(validation_stack, block_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      i32.const 2
      (block
        drop
      )
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800410202401a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, block_with_result)
{
    /* wat2wasm
    (func
      (block (result i32)
        i32.const -1
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800027f417f0b1a0b");
    const auto module = parse(wasm);
    EXPECT_EQ(module->codesec[0].max_stack_height, 1);
}

TEST(validation_stack, block_missing_result)
{
    /* wat2wasm --no-check
    (func
      (block (result i32)
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a08010600027f0b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, block_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        i32.const -1
      )
      i32.add
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0a010800027f417f0b6a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, block_too_many_results)
{
    /* wat2wasm --no-check
    (func
      (local i32)
      (block
        local.get 0  ;; Leaves an item on the stack what makes the block invalid.
      )
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0b010901017f024020000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "too many results");
}

TEST(validation_stack, loop_stack_underflow)
{
    /* wat2wasm --no-check
    (func (param i32)
      local.get 0
      (loop
        i32.eqz
        drop
      )
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a0b01090020000340451a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, loop_with_result)
{
    /* wat2wasm
    (func
      (loop (result i32)
        i32.const -1
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800037f417f0b1a0b");
    const auto module = parse(wasm);
    EXPECT_EQ(module->codesec[0].max_stack_height, 1);
}

TEST(validation_stack, loop_missing_result)
{
    /* wat2wasm --no-check
    (func
      (loop (result i32)
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a08010600037f0b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, loop_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func (result i32)
      (loop (result i32)
        i32.const -1
      )
      i32.add
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0a010800037f417f0b6a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, loop_too_many_results)
{
    /* wat2wasm --no-check
    (func
      (local i32)
      (loop
        local.get 0  ;; Leaves an item on the stack what makes the loop invalid.
      )
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0b010901017f034020000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (param i32)
      (loop
        local.get 0  ;; This is the input argument.
        i32.const 1
        i32.sub
        local.tee 0
        br_if 0
        local.get 0  ;; Leaves an item on the stack what makes the loop invalid.
      )
    )
    */
    const auto wasm2 =
        from_hex("0061736d0100000001050160017f00030201000a120110000340200041016b22000d0020000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "too many results");
}

TEST(validation_stack, call_stack_underflow)
{
    /* wat2wasm --no-check
    (func $f (param i32) (result i32)
      local.get 0
    )
    (func (result i32)
      ;; Call argument missing.
      call $f
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010a0260017f017f6000017f03030200010a0b02040020000b040010000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, call_1arg_in_block)
{
    /* wat2wasm
    (func $f (param i32))
    (func
      i32.const -1
      (block
        i32.const 0
        call $f
      )
      drop
    )
    */
    const auto wasm_valid = from_hex(
        "0061736d0100000001080260017f0060000003030200010a110202000b0c00417f0240410010000b1a0b");
    parse(wasm_valid);

    /* wat2wasm --no-check
    (func $f (param i32))
    (func
      i32.const -1
      (block
        call $f
      )
      drop
    )
    */
    const auto wasm_invalid1 = from_hex(
        "0061736d0100000001080260017f0060000003030200010a0f0202000b0a00417f024010000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid1), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func $f (param i32))
    (func
      i32.const -1
      (block
        call $f
      )
    )
    */
    const auto wasm_invalid2 =
        from_hex("0061736d0100000001080260017f0060000003030200010a0e0202000b0900417f024010000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid2), validation_error, "stack underflow");
}

TEST(validation_stack, call_1arg_1result_in_block)
{
    /* wat2wasm
    (func $f (param i32) (result i32) (local.get 0))
    (func
      i32.const -1
      (block
        i32.const 0
        call $f
        drop
      )
      drop
    )
    */
    const auto wasm_valid = from_hex(
        "0061736d0100000001090260017f017f60000003030200010a1402040020000b0d00417f0240410010001a0b1a"
        "0b");
    parse(wasm_valid);

    /* wat2wasm --no-check
    (func $f (param i32) (result i32) (local.get 0))
    (func
      i32.const -1
      (block
        call $f
      )
      drop
    )
    */
    const auto wasm_invalid1 = from_hex(
        "0061736d0100000001090260017f017f60000003030200010a1102040020000b0a00417f024010000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid1), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func $f (param i32) (result i32) (local.get 0))
    (func
      i32.const -1
      (block
        call $f
        drop
      )
    )
    */
    const auto wasm_invalid2 = from_hex(
        "0061736d0100000001090260017f017f60000003030200010a1102040020000b0a00417f024010001a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid2), validation_error, "stack underflow");
}

TEST(validation_stack, call_stack_underflow_imported_function)
{
    /* wat2wasm --no-check
    (func $f (import "m" "f") (param i32) (result i32))
    (func (result i32)
      ;; Call argument missing.
      call $f
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010a0260017f017f6000017f020701016d01660000030201010a0601040010000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, call_indirect_stack_underflow)
{
    /* wat2wasm --no-check
      (type (func (param i32)))
      (table anyfunc (elem 0))
      (func (param i32) nop)
      (func (param i32)
        ;; Call argument missing.
        (call_indirect (type 0) (local.get 0))
      )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f000303020000040501700101010907010041000b01000a0d020300010b0700"
        "20001100000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, call_indirect_1arg_in_loop)
{
    /* wat2wasm
      (type (func (param i32)))
      (table anyfunc (elem 0))
      (func
        i64.const -1
        (loop
          i32.const 0
          i32.const 0
          (call_indirect (type 0))
        )
        drop
      )
    */
    const auto wasm_valid = from_hex(
        "0061736d0100000001080260017f0060000003020101040501700101010907010041000b01000a11010f00427f"
        "0340410041001100000b1a0b");
    parse(wasm_valid);

    /* wat2wasm --no-check
      (type (func (param i32)))
      (table anyfunc (elem 0))
      (func
        i64.const -1
        (loop
          i32.const 0
          (call_indirect (type 0))
        )
        drop
      )
    */
    const auto wasm_invalid = from_hex(
        "0061736d0100000001080260017f0060000003020101040501700101010907010041000b01000a0f010d00427f"
        "034041001100000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid), validation_error, "stack underflow");
}

TEST(validation_stack, call_indirect_1arg_1result_in_loop)
{
    /* wat2wasm
      (type (func (param i32) (result i32)))
      (table anyfunc (elem 0))
      (func
        i64.const -1
        (loop
          i32.const 0
          i32.const 0
          (call_indirect (type 0))
          drop
        )
        drop
      )
    */
    const auto wasm_valid = from_hex(
        "0061736d0100000001090260017f017f60000003020101040501700101010907010041000b01000a1201100042"
        "7f0340410041001100001a0b1a0b");
    parse(wasm_valid);

    /* wat2wasm --no-check
      (type (func (param i32) (result i32)))
      (table anyfunc (elem 0))
      (func
        i64.const -1
        (loop
          i32.const 0
          (call_indirect (type 0))
        )
        drop
      )
    */
    const auto wasm_invalid = from_hex(
        "0061736d0100000001090260017f017f60000003020101040501700101010907010041000b01000a0f010d0042"
        "7f034041001100000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid), validation_error, "stack underflow");
}

TEST(validation_stack, unreachable)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.eqz
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0601040000450b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 0);
}

TEST(validation_stack, unreachable_2)
{
    /* wat2wasm
    (func
      unreachable
      i32.add
      i32.add
      i32.add
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a09010700006a6a6a1a0b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 0);
}

TEST(validation_stack, unreachable_3)
{
    /* wat2wasm
    (func (result i32)
      (block (result i32) unreachable)
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a08010600027f000b0b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 1);
}

TEST(validation_stack, unreachable_4)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      (if (result i32) (then i32.const 0) (else i32.const 1))
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000a0d010b0000047f41000541010b0b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 1);
}

TEST(validation_stack, unreachable_5)
{
    /* wat2wasm
    (func (result i32)
      i64.const 0
      unreachable
      i32.eqz
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a08010600420000450b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 1);
}


TEST(validation_stack, unreachable_call)
{
    /* wat2wasm
    (func $f (param i32) (result i32)
      local.get 0
    )
    (func (result i32)
      unreachable
      ;; Call argument missing.
      call $f
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010a0260017f017f6000017f03030200010a0c02040020000b05000010000b");

    parse(wasm);
}

TEST(validation_stack, unreachable_call_indirect)
{
    /* wat2wasm
      (type (func (param i32)))
      (table anyfunc (elem 0))
      (func (param i32) nop)
      (func (param i32)
        unreachable
        ;; Call argument missing.
        (call_indirect (type 0) (local.get 0))
      )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f000303020000040501700101010907010041000b01000a0e020300010b0800"
        "0020001100000b");

    parse(wasm);
}

TEST(validation_stack, unreachable_too_many_results)
{
    // It is not allowed to have additional items in polymorphic stack (after unreachable).

    /* wat2wasm --no-check
    (func
      unreachable
      i32.const 1
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a070105000041010b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (param i32) (result i32)
      local.get 0
      br 0
      i32.const 1
      i32.const 2
    )
    */
    const auto wasm2 =
        from_hex("0061736d0100000001060160017f017f030201000a0c010a0020000c00410141020b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      drop
      i32.const 0
      i32.const 0
    )
    */
    const auto wasm3 = from_hex("0061736d010000000105016000017f030201000a0a010800001a410041000b");
    EXPECT_THROW_MESSAGE(parse(wasm3), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i32.add
      i32.const 0
    )
    */
    const auto wasm4 = from_hex("0061736d010000000105016000017f030201000a08010600006a41000b");
    EXPECT_THROW_MESSAGE(parse(wasm4), validation_error, "too many results");


    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      (block (result i32)
        unreachable
        drop
        i32.const 0
        i32.const 0
      )
      drop
    )
    */
    const auto wasm5 =
        from_hex("0061736d010000000105016000017f030201000a10010e004100027f001a410041000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm5), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (param i32) (param i32) (result i32)
      unreachable
      call 0
      i32.const 0
    )
    */
    const auto wasm6 = from_hex("0061736d0100000001070160027f7f017f030201000a0901070000100041000b");
    EXPECT_THROW_MESSAGE(parse(wasm6), validation_error, "too many results");

    /* wat2wasm --no-check
    (table anyfunc (elem 0))
    (func (param i32) (param i32) (result i32)
      unreachable
      (call_indirect (type 0))
      i32.const 0
    )
    */
    const auto wasm7 = from_hex(
        "0061736d0100000001070160027f7f017f03020100040501700101010907010041000b01000a0a010800001100"
        "0041000b");
    EXPECT_THROW_MESSAGE(parse(wasm7), validation_error, "too many results");

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i32.const 0
      (block (result i32) i32.const 0)
    )
    */
    const auto wasm8 =
        from_hex("0061736d010000000105016000017f030201000a0c010a00004100027f41000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm8), validation_error, "too many results");
}

TEST(validation_stack, br)
{
    /* wat2wasm
    (func
      (block
        br 0
        i32.eqz  ;; unreachable
        drop
      )
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0b01090002400c00451a0b0b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 0);
}

TEST(validation_stack, br_table)
{
    /* wat2wasm
    (func (param i32)
      (block
        i32.const 1001
        local.get 0
        br_table 0 1
        i32.mul  ;; unreachable
        i32.mul
        i32.mul
        drop
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000a14011200024041e90720000e0100016c6c6c1a0b0b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 2);
}

TEST(validation_stack, return_1)
{
    /* wat2wasm
    (func
      return
      i32.eqz  ;; unreachable
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a070105000f451a0b");
    const auto module = parse(wasm);
    EXPECT_THAT(module->codesec[0].max_stack_height, 0);
}

TEST(validation_stack, if_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if
        (then
          local.set 0  ;; stack underflow
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a10010e01017e42014102044021000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_missing_result)
{
    /* wat2wasm --no-check
    (func
      i32.const 0
      (if (result i32)
        (then
        )
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a0108004100047f0b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_missing_result_v2)
{
    /* NO wat2wasm (it always omits empty (else)).
    (func
      i32.const 0
      (if (result i32)
        (then
        )
        (else
        )
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0c010a004100047f05010b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_missing_result_v3)
{
    /* wat2wasm --no-check
    (func
      i32.const 0
      (if (result i32)
        (then
        )
        (else
          i32.const 2
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a0d010b004100047f0541020b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, else_missing_result)
{
    /* NO wat2wasm (it always omits empty (else)).
    (func
      i32.const 0
      (if (result i32)
        (then
          i32.const 1
        )
        (else
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a0e010c004100047f410105010b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (i32.const 0))
        (else)
      )
    )
    */
    const auto wasm_missing_else =
        from_hex("0061736d010000000105016000017f030201000a0b0109004100047f41000b0b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_missing_else), validation_error, "missing result in else branch");

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      (if (result i32)
        (then (i32.const 0))
        (else)
      )
    )
    */
    const auto wasm_unreachable =
        from_hex("0061736d010000000105016000017f030201000a0a01080000047f41000b0b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_unreachable), validation_error, "missing result in else branch");
}

TEST(validation_stack, else_missing_result_v2)
{
    /* wat2wasm --no-check
    (func
      i32.const 0
      (if (result i32)
        (then
          i32.const 1
        )
        (else
          i32.const 2
          drop
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a10010e004100047f41010541021a0b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, else_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if
        (then)
        (else
          local.set 0  ;; stack underflow
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a11010f01017e4201410204400521000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          local.set 0  ;; stack underflow
          i64.const -1
        )
        (else
          i64.const -2
        )
      )
      drop
      drop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a16011401017e42014102047e2100427f05427e0b1a1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, else_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          i64.const -1
        )
        (else
          local.set 0  ;; stack underflow
          i64.const -2
        )
      )
      drop
      drop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a16011401017e42014102047e427f052100427e0b1a1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_else_stack_height)
{
    /* wat2wasm
    (func
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          i64.const 1
        )
        (else
          i64.const 3
        )
      )
      drop
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a1201100042014102047e42010542030b1a1a0b");
    const auto module = parse(wasm);
    EXPECT_EQ(module->codesec[0].max_stack_height, 2);
}

TEST(validation_stack, if_invalid_end_stack_height)
{
    /* wat2wasm --no-check
    (func
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          i64.const 1
          i64.const 2  ;; Stack height 2, but should be 1.
        )
        (else
          i64.const 3
          i64.const 4
          drop
        )
      )
      drop
      drop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a1701150042014102047e4201420205420342041a0b1a1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "too many results");
}

TEST(validation_stack, if_with_unreachable)
{
    /* wat2wasm --no-check
    (func (param i32) (result i64)
      local.get 0
      (if (result i64)
        (then
          unreachable
          i64.const 1
        )
        (else
          drop ;; Stack underflow.
        )
      )
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017e030201000a0e010c002000047e004201051a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");

    /* wat2wasm
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (unreachable))
        (else (i32.const 0))
      )
    )
    */
    const auto wasm2 =
        from_hex("0061736d010000000105016000017f030201000a0d010b004100047f000541000b0b");
    EXPECT_NO_THROW(parse(wasm2));

    /* wat2wasm
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (i32.const 0))
        (else (unreachable))
      )
    )
    */
    const auto wasm3 =
        from_hex("0061736d010000000105016000017f030201000a0d010b004100047f410005000b0b");
    EXPECT_NO_THROW(parse(wasm3));
}

TEST(validation_stack, br_missing_result)
{
    /* wat2wasm --no-check
    (func
      (block (result i32)
        br 0
      )
      drop
    )
    */
    const auto wasm1 = from_hex("0061736d01000000010401600000030201000a0a010800027f0c000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      (block br 1)
    )
    */
    const auto wasm2 = from_hex("0061736d010000000105016000017f030201000a0b010900410002400c010b0b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "stack underflow");

    /* wat2wasm
    (func
      (block (result i32)
        unreachable
        br 0
      )
      drop
    )
    */
    const auto wasm3 = from_hex("0061736d01000000010401600000030201000a0b010900027f000c000b1a0b");
    EXPECT_NO_THROW(parse(wasm3));

    /* wat2wasm
    (func
      (loop (result i32)
        (block (result i32)
          br 1
        )
      )
      drop
    )
    */
    const auto wasm4 =
        from_hex("0061736d01000000010401600000030201000a0d010b00037f027f0c010b0b1a0b");
    EXPECT_NO_THROW(parse(wasm4));
}

TEST(validation_stack, br_if_missing_result)
{
    /* wat2wasm --no-check
    (func
      (block (result i32)
        i32.const 0
        br_if 0
      )
      drop
    )
    */
    const auto wasm1 = from_hex("0061736d01000000010401600000030201000a0c010a00027f41000d000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      (block
        i32.const 0
        br_if 1
      )
    )
    */
    const auto wasm2 =
        from_hex("0061736d010000000105016000017f030201000a0d010b004100024041000d010b0b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "stack underflow");

    /* wat2wasm
    (func
      (block (result i32)
        unreachable
        br_if 0
      )
      drop
    )
    */
    const auto wasm3 = from_hex("0061736d01000000010401600000030201000a0b010900027f000d000b1a0b");
    EXPECT_NO_THROW(parse(wasm3));

    /* wat2wasm
    (func
      (loop (result i32)
        (block (result i32)
          i32.const 0
          br_if 1
          i32.const 0
        )
      )
      drop
    )
    */
    const auto wasm4 =
        from_hex("0061736d01000000010401600000030201000a11010f00037f027f41000d0141000b0b1a0b");
    EXPECT_NO_THROW(parse(wasm4));
}

TEST(validation_stack, br_table_missing_result)
{
    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        i32.const 0
        br_table 0 1
      )
      drop
    )
    */
    const auto wasm1 =
        from_hex("0061736d010000000105016000017f030201000a0e010c00027f41000e0100010b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      (block (result i32)
        i32.const 0
        br_table 0 1
      )
    )
    */
    const auto wasm2 =
        from_hex("0061736d010000000105016000017f030201000a0f010d004100027f41000e0100010b0b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "stack underflow");

    /* wat2wasm
    (func (result i32)
      (block (result i32)
        unreachable
        br_table 0 1
      )
    )
    */
    const auto wasm3 =
        from_hex("0061736d010000000105016000017f030201000a0c010a00027f000e0100010b0b");
    EXPECT_NO_THROW(parse(wasm3));

    /* wat2wasm
    (func
      (loop (result i32)
        (block
          i32.const 0
          br_table 0 1
        )
        i32.const 0
      )
      drop
    )
    */
    const auto wasm4 =
        from_hex("0061736d01000000010401600000030201000a13011100037f024041000e0100010b41000b1a0b");
    EXPECT_NO_THROW(parse(wasm4));
}

TEST(validation_stack, return_missing_result)
{
    /* wat2wasm --no-check
    (func (result i32)
      return
    )
    */
    const auto wasm1 = from_hex("0061736d010000000105016000017f030201000a050103000f0b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "stack underflow");

    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      (block return)
    )
    */
    const auto wasm2 = from_hex("0061736d010000000105016000017f030201000a0a010800410002400f0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "stack underflow");

    /* wat2wasm
    (func
      unreachable
      return
    )
    */
    const auto wasm3 = from_hex("0061736d01000000010401600000030201000a06010400000f0b");
    EXPECT_NO_THROW(parse(wasm3));
}
