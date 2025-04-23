// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "exceptions.hpp"
#include <cstdint>
#include <limits>
#include <utility>

static_assert((int8_t{-1} >> 1) == int8_t{-1},
    "signed LEB128 decoder assumes arithmetic shift of negative values");

namespace fizzy
{
template <typename T>
inline std::pair<T, const uint8_t*> leb128u_decode(const uint8_t* pos, const uint8_t* end)
{
    static_assert(!std::numeric_limits<T>::is_signed);

    T result = 0;
    int result_shift = 0;

    for (; result_shift < std::numeric_limits<T>::digits; ++pos, result_shift += 7)
    {
        if (pos == end)
            throw parser_error{"unexpected EOF"};

        result |= static_cast<T>((static_cast<T>(*pos) & 0x7F) << result_shift);
        if ((*pos & 0x80) == 0)
        {
            if (*pos != (result >> result_shift))
                throw parser_error{"invalid LEB128 encoding: unused bits set"};

            return {result, pos + 1};
        }
    }

    throw parser_error{"invalid LEB128 encoding: too many bytes"};
}

template <typename T>
inline std::pair<T, const uint8_t*> leb128s_decode(const uint8_t* pos, const uint8_t* end)
{
    static_assert(std::numeric_limits<T>::is_signed);

    using T_unsigned = typename std::make_unsigned<T>::type;
    T_unsigned result = 0;
    size_t result_shift = 0;

    for (; result_shift < std::numeric_limits<T_unsigned>::digits; ++pos, result_shift += 7)
    {
        if (pos == end)
            throw parser_error{"unexpected EOF"};

        result |= static_cast<T_unsigned>((static_cast<T_unsigned>(*pos) & 0x7F) << result_shift);
        if ((*pos & 0x80) == 0)
        {
            if (result_shift + 7 < sizeof(T_unsigned) * 8)
            {
                // non-terminal byte of the encoding, extend the sign bit, if needed
                if ((*pos & 0x40) != 0)
                {
                    constexpr auto all_ones = std::numeric_limits<T_unsigned>::max();
                    const auto mask = static_cast<T_unsigned>(all_ones << (result_shift + 7));
                    result |= mask;
                }
            }
            else
            {
                // terminal byte of the encoding, need to check unused bits
                const auto expected =
                    (static_cast<uint8_t>(static_cast<T>(result) >> result_shift) & 0x7F);

                if (*pos != expected)
                {
                    throw parser_error{
                        "invalid LEB128 encoding: unused bits not equal to sign bit"};
                }
            }

            return {static_cast<T>(result), pos + 1};
        }
    }

    throw parser_error{"invalid LEB128 encoding: too many bytes"};
}

}  // namespace fizzy
