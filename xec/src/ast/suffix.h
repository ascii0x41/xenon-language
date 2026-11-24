#pragma once

#include <string_view>

namespace xenon::ast {

    enum class NumericSuffix {
        NONE,
        I8, I16, I32, I64,
        U8, U16, U32, U64,
        SIZE,
        F32, F64,
    };

    [[nodiscard]]
    inline NumericSuffix parse_integer_suffix(std::string_view suf) noexcept {
        if (suf == "i8")   return NumericSuffix::I8;
        if (suf == "i16")  return NumericSuffix::I16;
        if (suf == "i32")  return NumericSuffix::I32;
        if (suf == "i64")  return NumericSuffix::I64;
        if (suf == "u8")   return NumericSuffix::U8;
        if (suf == "u16")  return NumericSuffix::U16;
        if (suf == "u32")  return NumericSuffix::U32;
        if (suf == "u64")  return NumericSuffix::U64;
        if (suf == "size") return NumericSuffix::SIZE;
        return NumericSuffix::NONE;
    }

    [[nodiscard]]
    inline NumericSuffix parse_float_suffix(std::string_view suf) noexcept {
        if (suf == "f32") return NumericSuffix::F32;
        if (suf == "f64") return NumericSuffix::F64;
        if (suf == "f")   return NumericSuffix::F32;  // shorthand for f32
        if (suf == "d")   return NumericSuffix::F64;  // shorthand for f64
        return NumericSuffix::NONE;
    }

} // namespace xenon::ast
