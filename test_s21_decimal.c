#include <check.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "s21_decimal.h"

#define BIG "79228162514264337593543950335"
#define NEG_BIG "-79228162514264337593543950335"
#define BIG_M1 "79228162514264337593543950334"
#define EPS     \
  "0.000000000" \
  "000000000"   \
  "000000000"   \
  "1"

static s21_decimal dec(const char *str) {
  s21_decimal result = {{0, 0, 0, 0}};
  s21_decimal ten = {{10, 0, 0, 0}};
  s21_decimal digit = {{0, 0, 0, 0}};
  int minus_sign = 0;
  int scale = 0;
  int after_point = 0;

  for (const char *p = str; *p != '\0'; p++) {
    if (*p == '-') {
      minus_sign = 1;
    } else if (*p == '.') {
      after_point = 1;
    } else {
      s21_mul(result, ten, &result);
      s21_from_int_to_decimal(*p - '0', &digit);
      s21_add(result, digit, &result);
      if (after_point) {
        scale++;
      }
    }
  }

  result.bits[3] = scale << 16;
  if (minus_sign) {
    result.bits[3] = (int)((unsigned)result.bits[3] | 0x80000000u);
  }

  return result;
}

static int is_zero_decimal(s21_decimal value) {
  return (value.bits[0] == 0 && value.bits[1] == 0 && value.bits[2] == 0 &&
          value.bits[3] == 0);
}

START_TEST(from_int_basic) {
  s21_decimal d = {{1, 1, 1, 1}};

  ck_assert_int_eq(s21_from_int_to_decimal(0, &d), 0);
  ck_assert(is_zero_decimal(d));

  ck_assert_int_eq(s21_from_int_to_decimal(1, &d), 0);
  ck_assert_int_eq(d.bits[0], 1);
  ck_assert_int_eq(d.bits[3], 0);

  ck_assert_int_eq(s21_from_int_to_decimal(-67889, &d), 0);
  ck_assert_int_eq(d.bits[0], 67889);
  ck_assert_uint_eq((unsigned)d.bits[3], 0x80000000u);
  ck_assert_int_eq(d.bits[1], 0);
  ck_assert_int_eq(d.bits[2], 0);
}
END_TEST

START_TEST(from_int_limits) {
  s21_decimal d = {{0}};

  ck_assert_int_eq(s21_from_int_to_decimal(INT_MAX, &d), 0);
  ck_assert_int_eq(d.bits[0], INT_MAX);
  ck_assert_int_eq(d.bits[3], 0);

  ck_assert_int_eq(s21_from_int_to_decimal(INT_MIN, &d), 0);
  ck_assert_uint_eq((unsigned)d.bits[0], 2147483648u);
  ck_assert_uint_eq((unsigned)d.bits[3], 0x80000000u);
}
END_TEST

START_TEST(from_int_null) {
  ck_assert_int_eq(s21_from_int_to_decimal(5, NULL), 1);
}
END_TEST

START_TEST(to_int_basic) {
  int n = 99;

  ck_assert_int_eq(s21_from_decimal_to_int(dec("0"), &n), 0);
  ck_assert_int_eq(n, 0);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("1.9"), &n), 0);
  ck_assert_int_eq(n, 1);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("-1.9"), &n), 0);
  ck_assert_int_eq(n, -1);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("0.9"), &n), 0);
  ck_assert_int_eq(n, 0);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("-67889"), &n), 0);
  ck_assert_int_eq(n, -67889);
  ck_assert_int_eq(s21_from_decimal_to_int(dec(EPS), &n), 0);
  ck_assert_int_eq(n, 0);
}
END_TEST

START_TEST(to_int_limits) {
  int n = 0;

  ck_assert_int_eq(s21_from_decimal_to_int(dec("2147483647"), &n), 0);
  ck_assert_int_eq(n, INT_MAX);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("-2147483648"), &n), 0);
  ck_assert_int_eq(n, INT_MIN);

  ck_assert_int_eq(s21_from_decimal_to_int(dec("2147483648"), &n), 1);
  ck_assert_int_eq(n, 0);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("-2147483649"), &n), 1);
  ck_assert_int_eq(n, 0);
  ck_assert_int_eq(s21_from_decimal_to_int(dec(BIG), &n), 1);
}
END_TEST

START_TEST(to_int_scale_brings_value_into_range) {
  int n = 0;
  s21_decimal d = {{0, 1, 0, 0x00010000}};

  ck_assert_int_eq(s21_from_decimal_to_int(d, &n), 0);
  ck_assert_int_eq(n, 429496729);

  s21_decimal e = {{0, 1, 0, 0}};
  ck_assert_int_eq(s21_from_decimal_to_int(e, &n), 1);
}
END_TEST

START_TEST(to_int_invalid) {
  int n = 5;
  s21_decimal bad_scale = {{1, 0, 0, 0x001D0000}};
  s21_decimal bad_bits = {{1, 0, 0, 0x00000001}};
  s21_decimal bad_bits2 = {{1, 0, 0, 0x01000000}};

  ck_assert_int_eq(s21_from_decimal_to_int(bad_scale, &n), 1);
  ck_assert_int_eq(n, 0);
  ck_assert_int_eq(s21_from_decimal_to_int(bad_bits, &n), 1);
  ck_assert_int_eq(s21_from_decimal_to_int(bad_bits2, &n), 1);
  ck_assert_int_eq(s21_from_decimal_to_int(dec("1"), NULL), 1);
}
END_TEST

typedef struct {
  float src;
  const char *expected;
} float_case;

START_TEST(from_float_values) {
  const float_case cases[] = {
      {0.0f, "0"},
      {1.0f, "1"},
      {-12.5f, "-12.5"},
      {0.5f, "0.5"},
      {0.1f, "0.1"},
      {1234567.0f, "1234567"},
      {1e10f, "10000000000"},
      {3.14159274f, "3.141593"},
      {1e-28f, EPS},
      {1.5e28f,
       "15"
       "000000000"
       "000000000"
       "000000000"},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    s21_decimal d = {{7, 7, 7, 7}};
    int code = s21_from_float_to_decimal(cases[i].src, &d);

    ck_assert_msg(code == 0, "case %zu: code %d", i, code);
    ck_assert_msg(s21_is_equal(d, dec(cases[i].expected)), "case %zu differs",
                  i);
  }
}
END_TEST

START_TEST(from_float_bits) {
  s21_decimal d = {{0}};

  ck_assert_int_eq(s21_from_float_to_decimal(-12.5f, &d), 0);
  ck_assert_int_eq(d.bits[0], 125);
  ck_assert_int_eq(d.bits[1], 0);
  ck_assert_int_eq(d.bits[2], 0);
  ck_assert_uint_eq((unsigned)d.bits[3], 0x80010000u);

  ck_assert_int_eq(s21_from_float_to_decimal(1e10f, &d), 0);
  ck_assert_uint_eq((unsigned)d.bits[0], 1410065408u);
  ck_assert_int_eq(d.bits[1], 2);
  ck_assert_int_eq(d.bits[3], 0);

  ck_assert_int_eq(s21_from_float_to_decimal(1e-28f, &d), 0);
  ck_assert_int_eq(d.bits[0], 1);
  ck_assert_int_eq(d.bits[3], 28 << 16);
}
END_TEST

START_TEST(from_float_errors) {
  const float bad[] = {NAN,   INFINITY, -INFINITY, 1e30f, -1e30f,
                       8e28f, 1e-29f,   -1e-29f,   1e-35f};

  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    s21_decimal d = {{7, 7, 7, 7}};

    ck_assert_msg(s21_from_float_to_decimal(bad[i], &d) == 1, "case %zu", i);
    ck_assert_msg(is_zero_decimal(d), "case %zu: dst not zero", i);
  }

  ck_assert_int_eq(s21_from_float_to_decimal(1.0f, NULL), 1);
}
END_TEST

START_TEST(from_float_negative_zero) {
  s21_decimal d = {{7, 7, 7, 7}};

  ck_assert_int_eq(s21_from_float_to_decimal(-0.0f, &d), 0);
  ck_assert_int_eq(s21_is_equal(d, dec("0")), 1);
}
END_TEST

START_TEST(to_float_values) {
  float f = 5.0f;

  ck_assert_int_eq(s21_from_decimal_to_float(dec("0.5"), &f), 0);
  ck_assert_float_eq(f, 0.5f);
  ck_assert_int_eq(s21_from_decimal_to_float(dec("-12.5"), &f), 0);
  ck_assert_float_eq(f, -12.5f);
  ck_assert_int_eq(s21_from_decimal_to_float(dec("1"), &f), 0);
  ck_assert_float_eq(f, 1.0f);
  ck_assert_int_eq(s21_from_decimal_to_float(dec("5.00"), &f), 0);
  ck_assert_float_eq(f, 5.0f);
  ck_assert_int_eq(s21_from_decimal_to_float(dec("0.1"), &f), 0);
  ck_assert_float_eq(f, 0.1f);
  ck_assert_int_eq(s21_from_decimal_to_float(dec("123456789"), &f), 0);
  ck_assert_float_eq(f, 123456789.0f);
}
END_TEST

START_TEST(to_float_limits) {
  float f = 0.0f;

  ck_assert_int_eq(s21_from_decimal_to_float(dec(BIG), &f), 0);
  ck_assert_float_eq(f, ldexpf(1.0f, 96));
  ck_assert_int_eq(s21_from_decimal_to_float(dec(NEG_BIG), &f), 0);
  ck_assert_float_eq(f, -ldexpf(1.0f, 96));
  ck_assert_int_eq(s21_from_decimal_to_float(dec(EPS), &f), 0);
  ck_assert_float_eq(f, 1e-28f);
}
END_TEST

START_TEST(to_float_zero_and_errors) {
  float f = 5.0f;

  ck_assert_int_eq(s21_from_decimal_to_float(dec("0"), &f), 0);
  ck_assert_float_eq(f, 0.0f);
  ck_assert(!signbit(f));

  ck_assert_int_eq(s21_from_decimal_to_float(dec("-0"), &f), 0);
  ck_assert_float_eq(f, 0.0f);
  ck_assert(signbit(f));

  s21_decimal bad_scale = {{1, 0, 0, 0x001D0000}};
  s21_decimal bad_bits = {{1, 0, 0, 0x00000100}};

  f = 5.0f;
  ck_assert_int_eq(s21_from_decimal_to_float(bad_scale, &f), 1);
  ck_assert_float_eq(f, 0.0f);
  ck_assert_int_eq(s21_from_decimal_to_float(bad_bits, &f), 1);
  ck_assert_int_eq(s21_from_decimal_to_float(dec("1"), NULL), 1);
}
END_TEST

START_TEST(float_round_trip) {
  const float values[] = {0.5f,   -12.5f,    1234.5f, 1.0f,
                          0.001f, 123456.7f, -0.25f};

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    s21_decimal d = {{0}};
    float back = 0.0f;

    ck_assert_int_eq(s21_from_float_to_decimal(values[i], &d), 0);
    ck_assert_int_eq(s21_from_decimal_to_float(d, &back), 0);
    ck_assert_msg(back == values[i], "case %zu", i);
  }
}
END_TEST

typedef struct {
  const char *in;
  const char *out;
} unary_case;

static void run_unary(int (*fn)(s21_decimal, s21_decimal *),
                      const unary_case *cases, size_t count) {
  for (size_t i = 0; i < count; i++) {
    s21_decimal r = {{7, 7, 7, 7}};
    int code = fn(dec(cases[i].in), &r);

    ck_assert_msg(code == 0, "case %zu (%s): code %d", i, cases[i].in, code);
    ck_assert_msg(s21_is_equal(r, dec(cases[i].out)), "case %zu (%s) differs",
                  i, cases[i].in);
    ck_assert_msg(((unsigned)r.bits[3] & 0x00FF0000u) == 0 || fn == s21_negate,
                  "case %zu (%s): scale not 0", i, cases[i].in);
  }
}

START_TEST(truncate_values) {
  const unary_case cases[] = {
      {"2.5", "2"},
      {"-2.5", "-2"},
      {"0.9", "0"},
      {"-0.9", "0"},
      {"5", "5"},
      {"123.456", "123"},
      {"5.00", "5"},
      {BIG, BIG},
      {NEG_BIG, NEG_BIG},
      {EPS, "0"},
      {"7922816251426433759354395033.5", "7922816251426433759354395033"},
  };

  run_unary(s21_truncate, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(floor_values) {
  const unary_case cases[] = {
      {"2.3", "2"},   {"-2.3", "-3"},     {"-2", "-2"}, {"0.5", "0"},
      {"-0.5", "-1"}, {"-" EPS, "-1"},    {"2.0", "2"}, {"-2.0", "-2"},
      {BIG, BIG},     {NEG_BIG, NEG_BIG},
  };

  run_unary(s21_floor, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(round_values) {
  const unary_case cases[] = {
      {"2.5", "3"},  {"-2.5", "-3"}, {"2.4", "2"},   {"-2.4", "-2"},
      {"0.49", "0"}, {"0.5", "1"},   {"-0.5", "-1"}, {"2.49999", "2"},
      {"7", "7"},    {EPS, "0"},     {"1.5", "2"},   {"-1.5", "-2"},
  };

  run_unary(s21_round, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(negate_values) {
  const unary_case cases[] = {
      {"2.5", "-2.5"}, {"-2.5", "2.5"}, {"0", "0"},
      {BIG, NEG_BIG},  {NEG_BIG, BIG},  {"1.00", "-1"},
  };

  run_unary(s21_negate, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(negate_keeps_scale_and_mantissa) {
  s21_decimal r = {{0}};
  s21_decimal d = {{23, 0, 0, (int)0x80010000u}};

  ck_assert_int_eq(s21_negate(d, &r), 0);
  ck_assert_int_eq(r.bits[0], 23);
  ck_assert_uint_eq((unsigned)r.bits[3], 0x00010000u);
}
END_TEST

START_TEST(other_functions_errors) {
  s21_decimal r = {{0}};
  s21_decimal bad = {{1, 0, 0, 0x001D0000}};
  s21_decimal bad2 = {{1, 0, 0, 0x00000002}};
  int (*fns[])(s21_decimal, s21_decimal *) = {s21_truncate, s21_floor,
                                              s21_round, s21_negate};

  for (size_t i = 0; i < 4; i++) {
    ck_assert_int_eq(fns[i](dec("1.5"), NULL), 1);
    ck_assert_int_eq(fns[i](bad, &r), 1);
    ck_assert_int_eq(fns[i](bad2, &r), 1);
  }
}
END_TEST

static void check_cmp(const char *a, const char *b, int order) {
  s21_decimal x = dec(a);
  s21_decimal y = dec(b);

  ck_assert_msg(s21_is_less(x, y) == (order < 0), "less: %s %s", a, b);
  ck_assert_msg(s21_is_less_or_equal(x, y) == (order <= 0),
                "less_or_equal: %s %s", a, b);
  ck_assert_msg(s21_is_greater(x, y) == (order > 0), "greater: %s %s", a, b);
  ck_assert_msg(s21_is_greater_or_equal(x, y) == (order >= 0),
                "greater_or_equal: %s %s", a, b);
  ck_assert_msg(s21_is_equal(x, y) == (order == 0), "equal: %s %s", a, b);
  ck_assert_msg(s21_is_not_equal(x, y) == (order != 0), "not_equal: %s %s", a,
                b);
}

START_TEST(compare_table) {
  check_cmp("1", "2", -1);
  check_cmp("2", "1", 1);
  check_cmp("2.5", "2.50", 0);
  check_cmp("-0.5", "-0.50", 0);
  check_cmp("-3", "2.5", -1);
  check_cmp("2.5", "-3", 1);
  check_cmp("-1", "-2", 1);
  check_cmp("-2", "-1", -1);
  check_cmp("0", "-0", 0);
  check_cmp("0.0", "0", 0);
  check_cmp("-0.000", "0.0", 0);
  check_cmp(BIG, NEG_BIG, 1);
  check_cmp(NEG_BIG, BIG, -1);
  check_cmp(BIG, BIG_M1, 1);
  check_cmp(BIG, BIG, 0);
  check_cmp(EPS, "0", 1);
  check_cmp("-" EPS, "0", -1);
  check_cmp(EPS, "0.1", -1);
  check_cmp("0.5", "0.49999999999999999999999999", 1);
  check_cmp("100", "99.99", 1);
}
END_TEST

START_TEST(compare_zero_with_different_scales) {
  s21_decimal z1 = {{0, 0, 0, 0}};
  s21_decimal z2 = {{0, 0, 0, (int)0x801C0000u}};

  ck_assert_int_eq(s21_is_equal(z1, z2), 1);
  ck_assert_int_eq(s21_is_less(z1, z2), 0);
  ck_assert_int_eq(s21_is_greater(z2, z1), 0);
}
END_TEST

typedef struct {
  const char *a;
  const char *b;
  int code;
  const char *expected;
} op_case;

static void run_op(int (*op)(s21_decimal, s21_decimal, s21_decimal *),
                   const op_case *cases, size_t count) {
  for (size_t i = 0; i < count; i++) {
    s21_decimal r = {{0}};
    int code = op(dec(cases[i].a), dec(cases[i].b), &r);

    ck_assert_msg(code == cases[i].code,
                  "case %zu (%s, %s): code %d, expected %d", i, cases[i].a,
                  cases[i].b, code, cases[i].code);
    if (cases[i].code == 0 && cases[i].expected != NULL) {
      ck_assert_msg(s21_is_equal(r, dec(cases[i].expected)),
                    "case %zu (%s, %s) differs", i, cases[i].a, cases[i].b);
    }
  }
}

START_TEST(add_table) {
  const op_case cases[] = {
      {"2.5", "0.6", 0, "3.1"},
      {"-2.5", "0.6", 0, "-1.9"},
      {"2.5", "-0.6", 0, "1.9"},
      {"-2.5", "-0.6", 0, "-3.1"},
      {"0.5", "0.5", 0, "1"},
      {"-5", "5", 0, "0"},
      {"0", "0", 0, "0"},
      {"100", "0.001", 0, "100.001"},
      {EPS, EPS, 0,
       "0.000000000"
       "000000000"
       "000000000"
       "2"},
      {BIG, "1", 1, NULL},
      {NEG_BIG, "-1", 2, NULL},
      {BIG, BIG, 1, NULL},
      {BIG, "0.4", 0, BIG},
      {BIG, "0.5", 1, NULL},
      {BIG_M1, "0.5", 0, BIG_M1},
      {BIG, NEG_BIG, 0, "0"},
  };

  run_op(s21_add, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(sub_table) {
  const op_case cases[] = {
      {"2.5", "0.6", 0, "1.9"}, {"0.6", "2.5", 0, "-1.9"},
      {"1", "1", 0, "0"},       {"-2", "-3", 0, "1"},
      {"-2", "3", 0, "-5"},     {BIG, "0.6", 0, BIG_M1},
      {BIG, "0.5", 0, BIG_M1},  {NEG_BIG, "1", 2, NULL},
      {BIG, "-1", 1, NULL},     {"0", BIG, 0, NEG_BIG},
  };

  run_op(s21_sub, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(mul_table) {
  const op_case cases[] = {
      {"2.5", "0.6", 0, "1.5"},
      {"-2", "3", 0, "-6"},
      {"-2", "-3", 0, "6"},
      {"0", "5", 0, "0"},
      {"0.1", "0.1", 0, "0.01"},
      {"1234.5678", "100", 0, "123456.78"},
      {BIG, "1", 0, BIG},
      {BIG, "0.1", 0, "7922816251426433759354395033.5"},
      {BIG, "2", 1, NULL},
      {NEG_BIG, "2", 2, NULL},
      {BIG, "-2", 2, NULL},
      {EPS, EPS, 0, "0"},
  };

  run_op(s21_mul, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(div_table) {
  const op_case cases[] = {
      {"1", "3", 0,
       "0.3333333333"
       "3333333333"
       "33333333"},
      {"2", "3", 0,
       "0.6666666666"
       "6666666666"
       "66666667"},
      {"1", "4", 0, "0.25"},
      {"10", "4", 0, "2.5"},
      {"6", "3", 0, "2"},
      {"-6", "3", 0, "-2"},
      {"6", "-3", 0, "-2"},
      {"-6", "-3", 0, "2"},
      {"0", "5", 0, "0"},
      {"1", "0.5", 0, "2"},
      {"0.3", "0.1", 0, "3"},
      {BIG, "1", 0, BIG},
      {BIG, "0.1", 1, NULL},
      {NEG_BIG, "0.1", 2, NULL},
      {"1", "0", 3, NULL},
      {"0", "0", 3, NULL},
      {"-1", "0.00", 3, NULL},
  };

  run_op(s21_div, cases, sizeof(cases) / sizeof(cases[0]));
}
END_TEST

START_TEST(arithmetic_errors) {
  s21_decimal r = {{0}};
  s21_decimal bad = {{1, 0, 0, 0x001D0000}};
  s21_decimal bad2 = {{1, 0, 0, 0x00000004}};
  int (*ops[])(s21_decimal, s21_decimal, s21_decimal *) = {s21_add, s21_sub,
                                                           s21_mul, s21_div};

  for (size_t i = 0; i < 4; i++) {
    ck_assert_int_eq(ops[i](dec("1"), dec("1"), NULL), 1);
    ck_assert_int_eq(ops[i](bad, dec("1"), &r), 1);
    ck_assert_int_eq(ops[i](dec("1"), bad, &r), 1);
    ck_assert_int_eq(ops[i](bad2, dec("1"), &r), 1);
  }
}
END_TEST

START_TEST(arithmetic_zero_sign) {
  s21_decimal r = {{0}};

  ck_assert_int_eq(s21_sub(dec("5"), dec("5"), &r), 0);
  ck_assert_int_eq(r.bits[0], 0);
  ck_assert_uint_eq((unsigned)r.bits[3] & 0x80000000u, 0u);

  ck_assert_int_eq(s21_mul(dec("-5"), dec("0"), &r), 0);
  ck_assert_uint_eq((unsigned)r.bits[3] & 0x80000000u, 0u);
}
END_TEST

START_TEST(arithmetic_properties) {
  const char *values[] = {"1.25", "-7.5", "1000000", "0.0001", "-33.333"};

  for (size_t i = 0; i < 5; i++) {
    for (size_t j = 0; j < 5; j++) {
      s21_decimal a = dec(values[i]);
      s21_decimal b = dec(values[j]);
      s21_decimal sum = {{0}};
      s21_decimal back = {{0}};
      s21_decimal prod = {{0}};

      ck_assert_int_eq(s21_add(a, b, &sum), 0);
      ck_assert_int_eq(s21_sub(sum, b, &back), 0);
      ck_assert_int_eq(s21_is_equal(back, a), 1);

      ck_assert_int_eq(s21_mul(a, b, &prod), 0);
      ck_assert_int_eq(s21_div(prod, b, &back), 0);
      ck_assert_int_eq(s21_is_equal(back, a), 1);
    }
  }
}
END_TEST

static Suite *s21_decimal_suite(void) {
  Suite *suite = suite_create("s21_decimal");
  TCase *tc_int = tcase_create("int");
  TCase *tc_float = tcase_create("float");
  TCase *tc_other = tcase_create("other");
  TCase *tc_cmp = tcase_create("compare");
  TCase *tc_arith = tcase_create("arithmetic");

  tcase_add_test(tc_int, from_int_basic);
  tcase_add_test(tc_int, from_int_limits);
  tcase_add_test(tc_int, from_int_null);
  tcase_add_test(tc_int, to_int_basic);
  tcase_add_test(tc_int, to_int_limits);
  tcase_add_test(tc_int, to_int_scale_brings_value_into_range);
  tcase_add_test(tc_int, to_int_invalid);

  tcase_add_test(tc_float, from_float_values);
  tcase_add_test(tc_float, from_float_bits);
  tcase_add_test(tc_float, from_float_errors);
  tcase_add_test(tc_float, from_float_negative_zero);
  tcase_add_test(tc_float, to_float_values);
  tcase_add_test(tc_float, to_float_limits);
  tcase_add_test(tc_float, to_float_zero_and_errors);
  tcase_add_test(tc_float, float_round_trip);

  tcase_add_test(tc_other, truncate_values);
  tcase_add_test(tc_other, floor_values);
  tcase_add_test(tc_other, round_values);
  tcase_add_test(tc_other, negate_values);
  tcase_add_test(tc_other, negate_keeps_scale_and_mantissa);
  tcase_add_test(tc_other, other_functions_errors);

  tcase_add_test(tc_cmp, compare_table);
  tcase_add_test(tc_cmp, compare_zero_with_different_scales);

  tcase_add_test(tc_arith, add_table);
  tcase_add_test(tc_arith, sub_table);
  tcase_add_test(tc_arith, mul_table);
  tcase_add_test(tc_arith, div_table);
  tcase_add_test(tc_arith, arithmetic_errors);
  tcase_add_test(tc_arith, arithmetic_zero_sign);
  tcase_add_test(tc_arith, arithmetic_properties);

  suite_add_tcase(suite, tc_int);
  suite_add_tcase(suite, tc_float);
  suite_add_tcase(suite, tc_other);
  suite_add_tcase(suite, tc_cmp);
  suite_add_tcase(suite, tc_arith);

  return suite;
}

int main(void) {
  SRunner *runner = srunner_create(s21_decimal_suite());
  int failed = 0;

  srunner_run_all(runner, CK_NORMAL);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
