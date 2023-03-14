#pragma once

#include "common.h"

struct result_and_carry_u8 {
  u8 result;
  bool carry;
};

static inline struct result_and_carry_u8
carrying_add_u8(const u8 lhs, const u8 rhs, const bool carry) {
  const u8 sum = lhs + rhs + carry;
  const bool carry_out = (sum < lhs || sum < rhs || sum < (carry * 0xFF));
  return (struct result_and_carry_u8){sum, carry_out};
}

static inline struct result_and_carry_u8
carrying_sub_u8(const u8 lhs, const u8 rhs, const bool carry) {
  const u8 dif = lhs - rhs - carry;
  const bool carry_out = (dif > lhs && dif > rhs && dif > (carry * 0xFF));
  return (struct result_and_carry_u8){dif, carry_out};
}

static inline struct result_and_carry_u8
carrying_bcd_add_u8(const u8 lhs, const u8 rhs, const bool carry_in) {
  u8 result_lo = (lhs & 0x0F) + (rhs & 0x0F) + carry_in;
  const u8 carry = (result_lo > 0x09);
  result_lo %= 0x0A;
  u8 result_hi = ((lhs & 0xF0)) + ((rhs & 0xF0)) + carry;
  const bool carry_out = (result_hi > 0x90);
  result_hi %= 0xA0;
  const u8 result = result_lo | result_hi;
  return (struct result_and_carry_u8){result, carry_out};
}

static inline struct result_and_carry_u8
carrying_bcd_sub_u8(const u8 lhs, const u8 rhs, const bool carry_in) {
  TODO();
}
