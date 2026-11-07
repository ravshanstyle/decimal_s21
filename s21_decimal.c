#include "s21_decimal.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//////////////////////////// convertation

static int s21_is_valid_decimal(s21_decimal value) {
  uint32_t info = (uint32_t)value.bits[3];
  int scale = (info & 0x00ff0000) >> 16;
  int result = 1;

  if ((info & 0x7F00FFFF) || scale > 28) {
    result = 0;
  }

  return result;
}

static int s21_get_scale(s21_decimal value) {
  return (int)(((uint32_t)value.bits[3] & 0x00ff0000) >> 16);
}

static int s21_get_sign(s21_decimal value) {
  return (((uint32_t)value.bits[3] & 0x80000000) == 0x80000000);
}

static uint32_t s21_div_by_10(uint32_t word[3]) {
  uint64_t remainder = 0;

  for (int k = 2; k >= 0; k--) {
    uint64_t current = (remainder << 32) | word[k];
    word[k] = (uint32_t)(current / 10);
    remainder = current % 10;
  }

  return (uint32_t)remainder;
}

int s21_from_int_to_decimal(int src, s21_decimal *dst) {
  if (dst == NULL) {
    return 1;
  }

  for (int i = 0; i < 4; i++) {
    dst->bits[i] = 0;
  }

  uint32_t value = (uint32_t)src;

  if (src < 0) {
    dst->bits[3] = (int)0x80000000u;
    value = 0u - value;
  }

  dst->bits[0] = (int)value;

  return 0;
}

int s21_from_decimal_to_int(s21_decimal src, int *dst) {
  if (dst == NULL) {
    return 1;
  }
  *dst = 0;
  if (!s21_is_valid_decimal(src)) {
    return 1;
  }

  uint32_t word[3] = {(uint32_t)src.bits[0], (uint32_t)src.bits[1],
                      (uint32_t)src.bits[2]};
  int minus_sign = s21_get_sign(src);
  int scale = s21_get_scale(src);

  for (int s = 0; s < scale; s++) {
    s21_div_by_10(word);
  }

  if (word[1] != 0 || word[2] != 0) {
    return 1;
  }

  if (minus_sign) {
    if (word[0] > 2147483648u) {
      return 1;
    }
    *dst = (int)(-(int64_t)word[0]);
  } else {
    if (word[0] > (uint32_t)INT_MAX) {
      return 1;
    }
    *dst = (int)word[0];
  }

  return 0;
}

int s21_from_float_to_decimal(float src, s21_decimal *dst) {
  if (dst == NULL) {
    return 1;
  }

  for (int g = 0; g < 4; g++) {
    dst->bits[g] = 0;
  }

  if (isnan(src) || isinf(src)) {
    return 1;
  }
  if (fabsf(src) >= ldexpf(1.0f, 96)) {
    return 1;
  }

  uint64_t acc_digit = 0;
  int acc_exp = 0;
  char my_float[32];
  int i = 0;
  int minus_sign = 0;
  int sign_exp = 0;

  snprintf(my_float, sizeof(my_float), "%.6e", src);

  if (my_float[i] == '-') {
    minus_sign = 1;
    i++;
  }

  if (isdigit((unsigned char)my_float[i])) {
    acc_digit = my_float[i] - '0';
    i++;
  }

  if (my_float[i] == '.') {
    i++;
  }
  while (isdigit((unsigned char)my_float[i])) {
    acc_digit = acc_digit * 10 + (my_float[i] - '0');
    i++;
  }

  if (my_float[i] == 'e') {
    i++;
  }
  if (my_float[i] == '-') {
    sign_exp = 1;
    i++;
  } else if (my_float[i] == '+') {
    i++;
  }
  while (isdigit((unsigned char)my_float[i])) {
    acc_exp = acc_exp * 10 + (my_float[i] - '0');
    i++;
  }
  if (sign_exp) {
    acc_exp = -acc_exp;
  }

  if (acc_digit != 0 && acc_exp < -28) {
    return 1;
  }

  int power = acc_exp - 6;
  int scale = 0;
  uint32_t word[3] = {0, 0, 0};

  if (power < 0) {
    scale = -power;

    if (scale > 28) {
      int drop = scale - 28;
      uint64_t divisor = 1;

      for (int w = 0; w < drop; w++) {
        divisor *= 10;
      }
      uint64_t remainder = acc_digit % divisor;
      uint64_t half = divisor / 2;

      acc_digit /= divisor;

      if ((remainder > half) || ((remainder == half) && (acc_digit % 2))) {
        acc_digit++;
      }
      scale = 28;
    }

    while (scale > 0 && acc_digit != 0 && (acc_digit % 10) == 0) {
      acc_digit /= 10;
      scale--;
    }
    if (acc_digit == 0) {
      scale = 0;
    }

    word[0] = (uint32_t)acc_digit;
    word[1] = (uint32_t)(acc_digit >> 32);
  } else {
    word[0] = (uint32_t)acc_digit;
    word[1] = (uint32_t)(acc_digit >> 32);

    for (int a = 0; a < power; a++) {
      uint64_t carry = 0;

      for (int k = 0; k < 3; k++) {
        uint64_t current = (uint64_t)word[k] * 10 + carry;
        word[k] = (uint32_t)current;
        carry = current >> 32;
      }
      if (carry != 0) {
        for (int g = 0; g < 4; g++) {
          dst->bits[g] = 0;
        }
        return 1;
      }
    }
  }

  dst->bits[0] = (int)word[0];
  dst->bits[1] = (int)word[1];
  dst->bits[2] = (int)word[2];
  dst->bits[3] = scale << 16;
  if (minus_sign) {
    dst->bits[3] = (int)((uint32_t)dst->bits[3] | 0x80000000u);
  }

  return 0;
}

int s21_from_decimal_to_float(s21_decimal src, float *dst) {
  if (dst == NULL) {
    return 1;
  }
  *dst = 0.0f;
  if (!s21_is_valid_decimal(src)) {
    return 1;
  }

  uint32_t word[3] = {(uint32_t)src.bits[0], (uint32_t)src.bits[1],
                      (uint32_t)src.bits[2]};
  int minus_sign = s21_get_sign(src);
  int scale = s21_get_scale(src);

  char digits[32];
  int pos = 31;

  digits[pos] = '\0';
  while (word[0] != 0 || word[1] != 0 || word[2] != 0) {
    pos--;
    digits[pos] = (char)('0' + s21_div_by_10(word));
  }
  if (pos == 31) {
    pos--;
    digits[pos] = '0';
  }

  char my_float[64];

  snprintf(my_float, sizeof(my_float), "%s%se-%d", minus_sign ? "-" : "",
           &digits[pos], scale);
  *dst = strtof(my_float, NULL);

  return 0;
}

///////                                                      other

static void s21_add_one(uint32_t word[3]) {
  for (int k = 0; k < 3; k++) {
    word[k]++;
    if (word[k] != 0) {
      break;
    }
  }
}

static void s21_store_integer(uint32_t word[3], int minus_sign,
                              s21_decimal *result) {
  result->bits[0] = (int)word[0];
  result->bits[1] = (int)word[1];
  result->bits[2] = (int)word[2];
  result->bits[3] = 0;
  if (minus_sign) {
    result->bits[3] = (int)0x80000000;
  }
}

int s21_truncate(s21_decimal value, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value)) {
    return 1;
  }

  uint32_t word[3] = {(uint32_t)value.bits[0], (uint32_t)value.bits[1],
                      (uint32_t)value.bits[2]};
  uint32_t info = (uint32_t)value.bits[3];
  int minus_sign = (info & 0x80000000) == 0x80000000;
  int scale = (info & 0x00ff0000) >> 16;

  for (int s = 0; s < scale; s++) {
    s21_div_by_10(word);
  }

  s21_store_integer(word, minus_sign, result);

  return 0;
}

int s21_floor(s21_decimal value, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value)) {
    return 1;
  }

  uint32_t word[3] = {(uint32_t)value.bits[0], (uint32_t)value.bits[1],
                      (uint32_t)value.bits[2]};
  uint32_t info = (uint32_t)value.bits[3];
  int minus_sign = (info & 0x80000000) == 0x80000000;
  int scale = (info & 0x00ff0000) >> 16;
  int has_fraction = 0;

  for (int s = 0; s < scale; s++) {
    if (s21_div_by_10(word) != 0) {
      has_fraction = 1;
    }
  }

  if (minus_sign && has_fraction) {
    s21_add_one(word);
  }

  s21_store_integer(word, minus_sign, result);

  return 0;
}

int s21_round(s21_decimal value, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value)) {
    return 1;
  }

  uint32_t word[3] = {(uint32_t)value.bits[0], (uint32_t)value.bits[1],
                      (uint32_t)value.bits[2]};
  uint32_t info = (uint32_t)value.bits[3];
  int minus_sign = (info & 0x80000000) == 0x80000000;
  int scale = (info & 0x00ff0000) >> 16;
  uint32_t last_digit = 0;

  for (int s = 0; s < scale; s++) {
    last_digit = s21_div_by_10(word);
  }

  if (scale > 0 && last_digit >= 5) {
    s21_add_one(word);
  }

  s21_store_integer(word, minus_sign, result);

  return 0;
}

int s21_negate(s21_decimal value, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value)) {
    return 1;
  }

  *result = value;
  result->bits[3] = (int)((uint32_t)value.bits[3] ^ 0x80000000);

  return 0;
}

///// <>

static void s21_to_wide(s21_decimal value, uint32_t wide[6]) {
  for (int k = 0; k < 6; k++) {
    wide[k] = 0;
  }
  for (int k = 0; k < 3; k++) {
    wide[k] = (uint32_t)value.bits[k];
  }
}

static void s21_wide_mul_by_10(uint32_t wide[6]) {
  uint64_t carry = 0;

  for (int k = 0; k < 6; k++) {
    uint64_t current = (uint64_t)wide[k] * 10 + carry;
    wide[k] = (uint32_t)current;
    carry = current >> 32;
  }
}

static int s21_wide_is_zero(const uint32_t wide[6]) {
  int result = 1;

  for (int k = 0; k < 6; k++) {
    if (wide[k] != 0) {
      result = 0;
    }
  }

  return result;
}

static int s21_wide_compare(const uint32_t first[6], const uint32_t second[6]) {
  int result = 0;

  for (int k = 5; k >= 0 && result == 0; k--) {
    if (first[k] < second[k]) {
      result = -1;
    } else if (first[k] > second[k]) {
      result = 1;
    }
  }

  return result;
}

static int s21_compare(s21_decimal value_1, s21_decimal value_2) {
  uint32_t first[6];
  uint32_t second[6];
  int scale_1 = s21_get_scale(value_1);
  int scale_2 = s21_get_scale(value_2);
  int sign_1 = s21_get_sign(value_1);
  int sign_2 = s21_get_sign(value_2);
  int result = 0;

  s21_to_wide(value_1, first);
  s21_to_wide(value_2, second);

  for (int s = scale_1; s < scale_2; s++) {
    s21_wide_mul_by_10(first);
  }
  for (int s = scale_2; s < scale_1; s++) {
    s21_wide_mul_by_10(second);
  }

  if (s21_wide_is_zero(first)) {
    sign_1 = 0;
  }
  if (s21_wide_is_zero(second)) {
    sign_2 = 0;
  }

  if (sign_1 != sign_2) {
    result = sign_1 ? -1 : 1;
  } else {
    result = s21_wide_compare(first, second);
    if (sign_1) {
      result = -result;
    }
  }

  return result;
}

int s21_is_less(s21_decimal value_1, s21_decimal value_2) {
  return (s21_compare(value_1, value_2) < 0);
}

int s21_is_less_or_equal(s21_decimal value_1, s21_decimal value_2) {
  return (s21_compare(value_1, value_2) <= 0);
}

int s21_is_greater(s21_decimal value_1, s21_decimal value_2) {
  return (s21_compare(value_1, value_2) > 0);
}

int s21_is_greater_or_equal(s21_decimal value_1, s21_decimal value_2) {
  return (s21_compare(value_1, value_2) >= 0);
}

int s21_is_equal(s21_decimal value_1, s21_decimal value_2) {
  return (s21_compare(value_1, value_2) == 0);
}

int s21_is_not_equal(s21_decimal value_1, s21_decimal value_2) {
  return (s21_compare(value_1, value_2) != 0);
}

//////////////////////////////////////////////////////////////////////////////////+++++++++++++++++++++++

static void s21_wide_add(uint32_t first[6], const uint32_t second[6]) {
  uint64_t carry = 0;

  for (int k = 0; k < 6; k++) {
    uint64_t current = (uint64_t)first[k] + second[k] + carry;
    first[k] = (uint32_t)current;
    carry = current >> 32;
  }
}

static void s21_wide_sub(uint32_t first[6], const uint32_t second[6]) {
  uint64_t borrow = 0;

  for (int k = 0; k < 6; k++) {
    uint64_t taken = (uint64_t)second[k] + borrow;
    if ((uint64_t)first[k] >= taken) {
      first[k] = (uint32_t)(first[k] - taken);
      borrow = 0;
    } else {
      first[k] = (uint32_t)(((uint64_t)first[k] + 0x100000000ull) - taken);
      borrow = 1;
    }
  }
}

static void s21_wide_add_small(uint32_t wide[6], uint32_t value) {
  uint64_t carry = value;

  for (int k = 0; k < 6 && carry != 0; k++) {
    uint64_t current = (uint64_t)wide[k] + carry;
    wide[k] = (uint32_t)current;
    carry = current >> 32;
  }
}

static uint32_t s21_wide_div_by_10(uint32_t wide[6]) {
  uint64_t remainder = 0;

  for (int k = 5; k >= 0; k--) {
    uint64_t current = (remainder << 32) | wide[k];
    wide[k] = (uint32_t)(current / 10);
    remainder = current % 10;
  }

  return (uint32_t)remainder;
}

static int s21_wide_fits_96(const uint32_t wide[6]) {
  return (wide[3] == 0 && wide[4] == 0 && wide[5] == 0);
}

static void s21_wide_mul(const uint32_t first[3], const uint32_t second[3], uint32_t out[6]) {
  for (int k = 0; k < 6; k++) {
    out[k] = 0;
  }

  for (int i = 0; i < 3; i++) {
    uint64_t carry = 0;
    for (int j = 0; j < 3; j++) {
      uint64_t current = (uint64_t)first[i] * second[j] + out[i + j] + carry;
      out[i + j] = (uint32_t)current;
      carry = current >> 32;
    }
    out[i + 3] = (uint32_t)carry;
  }
}

static void s21_wide_divmod(const uint32_t number[6], const uint32_t divider[6],
                            uint32_t quotient[6], uint32_t remainder[6]) {
  for (int k = 0; k < 6; k++) {
    quotient[k] = 0;
    remainder[k] = 0;
  }

  for (int bit = 191; bit >= 0; bit--) {
    s21_wide_add(remainder, remainder);
    if ((number[bit / 32] >> (bit % 32)) & 1u) {
      s21_wide_add_small(remainder, 1);
    }
    if (s21_wide_compare(remainder, divider) >= 0) {
      s21_wide_sub(remainder, divider);
      quotient[bit / 32] |= (1u << (bit % 32));
    }
  }
}

static int s21_wide_to_decimal(uint32_t wide[6], int scale, int sign,
                               uint32_t digit, int dropped,
                               s21_decimal *result) {
  int status = 0;
  int done = 0;
  int sticky = 0;

  while (!done) {
    while (scale > 0 && (scale > 28 || !s21_wide_fits_96(wide))) {
      if (digit != 0) {
        sticky = 1;
      }
      digit = s21_wide_div_by_10(wide);
      scale--;
      dropped = 1;
    }

    if (scale > 28 || !s21_wide_fits_96(wide)) {
      status = sign ? 2 : 1;
      done = 1;
    } else if (dropped &&
               (digit > 5 || (digit == 5 && (sticky || (wide[0] & 1u))))) {
      s21_wide_add_small(wide, 1);
      dropped = 0;
      digit = 0;
      sticky = 0;
    } else {
      done = 1;
    }
  }

  if (status == 0) {
    if (s21_wide_is_zero(wide)) {
      sign = 0;
    }

    result->bits[0] = (int)wide[0];
    result->bits[1] = (int)wide[1];
    result->bits[2] = (int)wide[2];
    result->bits[3] = scale << 16;
    if (sign) {
      result->bits[3] = (int)((uint32_t)result->bits[3] | 0x80000000u);
    }
  }

  return status;
}

int s21_add(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value_1) || !s21_is_valid_decimal(value_2)) {
    return 1;
  }

  uint32_t first[6];
  uint32_t second[6];
  int scale_1 = s21_get_scale(value_1);
  int scale_2 = s21_get_scale(value_2);
  int sign_1 = s21_get_sign(value_1);
  int sign_2 = s21_get_sign(value_2);
  int scale = (scale_1 > scale_2) ? scale_1 : scale_2;
  int sign = sign_1;

  s21_to_wide(value_1, first);
  s21_to_wide(value_2, second);

  for (int s = scale_1; s < scale; s++) {
    s21_wide_mul_by_10(first);
  }
  for (int s = scale_2; s < scale; s++) {
    s21_wide_mul_by_10(second);
  }

  if (sign_1 == sign_2) {
    s21_wide_add(first, second);
  } else if (s21_wide_compare(first, second) >= 0) {
    s21_wide_sub(first, second);
  } else {
    s21_wide_sub(second, first);
    for (int k = 0; k < 6; k++) {
      first[k] = second[k];
    }
    sign = sign_2;
  }

  return s21_wide_to_decimal(first, scale, sign, 0, 0, result);
}

int s21_sub(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  value_2.bits[3] = (int)((uint32_t)value_2.bits[3] ^ 0x80000000u);

  return s21_add(value_1, value_2, result);
}

int s21_mul(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value_1) || !s21_is_valid_decimal(value_2)) {
    return 1;
  }

  uint32_t first[3] = {(uint32_t)value_1.bits[0], (uint32_t)value_1.bits[1],
                       (uint32_t)value_1.bits[2]};
  uint32_t second[3] = {(uint32_t)value_2.bits[0], (uint32_t)value_2.bits[1],
                        (uint32_t)value_2.bits[2]};
  uint32_t product[6];
  int scale = s21_get_scale(value_1) + s21_get_scale(value_2);
  int sign = s21_get_sign(value_1) ^ s21_get_sign(value_2);

  s21_wide_mul(first, second, product);

  return s21_wide_to_decimal(product, scale, sign, 0, 0, result);
}

int s21_div(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  if (result == NULL) {
    return 1;
  }
  if (!s21_is_valid_decimal(value_1) || !s21_is_valid_decimal(value_2)) {
    return 1;
  }

  uint32_t number[6];
  uint32_t divider[6];
  uint32_t quotient[6];
  uint32_t remainder[6];
  uint32_t twice[6];
  int scale_1 = s21_get_scale(value_1);
  int scale_2 = s21_get_scale(value_2);
  int sign = s21_get_sign(value_1) ^ s21_get_sign(value_2);
  int scale = 0;
  uint32_t digit = 0;
  int dropped = 0;

  s21_to_wide(value_1, number);
  s21_to_wide(value_2, divider);

  if (s21_wide_is_zero(divider)) {
    return 3;
  }

  if (scale_1 >= scale_2) {
    scale = scale_1 - scale_2;
  } else {
    for (int s = scale_1; s < scale_2; s++) {
      s21_wide_mul_by_10(number);
    }
  }

  s21_wide_divmod(number, divider, quotient, remainder);

  while (!s21_wide_is_zero(remainder) && scale < 28 &&
         s21_wide_fits_96(quotient)) {
    uint32_t next = 0;

    s21_wide_mul_by_10(quotient);
    s21_wide_mul_by_10(remainder);
    while (s21_wide_compare(remainder, divider) >= 0) {
      s21_wide_sub(remainder, divider);
      next++;
    }
    s21_wide_add_small(quotient, next);
    scale++;
  }

  if (!s21_wide_is_zero(remainder)) {
    int cmp = 0;

    for (int k = 0; k < 6; k++) {
      twice[k] = remainder[k];
    }
    s21_wide_add(twice, twice);
    cmp = s21_wide_compare(twice, divider);

    dropped = 1;
    if (cmp > 0) {
      digit = 6;
    } else if (cmp == 0) {
      digit = 5;
    } else {
      digit = 1;
    }
  }

  return s21_wide_to_decimal(quotient, scale, sign, digit, dropped, result);
}
