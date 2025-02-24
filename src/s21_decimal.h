#ifndef S21_DECIMAL_H_
#define S21_DECIMAL_H_

#include <stdint.h>

#define MINUS 0x80000000     // minus 10000000 00000000 00000000 00000000
#define SC 0x00ff0000        // scale 00000000 11111111 00000000 00000000
#define MAX4BITE 0xffffffff  // max4b 11111111 11111111 11111111 11111111
#define FIRST 0xFFFF         // FIRST 00000000 00000000 11111111 11111111
#define SECOND 0x7F000000    // FIRST 01111111 00000000 00000000 00000000

#define MAX_FLOAT_TO_CONVERT 79228157791897854723898736640.0f
#define MIN_FLOAT_TO_CONVERT \
  0.00000000000000000000000000010000000031710768509710513471352647538147514756461109f

typedef struct {
  int bits[4];
} s21_decimal;

// Расширенный decimal для того чтобы было удобно выполнять все операции
typedef struct {
  uint64_t bits[7];  // используем в два раза больший int на 64 бита
  uint16_t scale;  // для скейла используем маленький int
} work_decimal;

typedef struct {
  s21_decimal decimals[2];
} big_decimal;

// Work convertors
work_decimal decimal_to_work(s21_decimal dec);
s21_decimal work_to_decimal(work_decimal dec);

// Arithmetic Operators
int s21_add(s21_decimal value_1, s21_decimal value_2, s21_decimal *result);
int s21_sub(s21_decimal value_1, s21_decimal value_2, s21_decimal *result);
int s21_mul(s21_decimal value_1, s21_decimal value_2, s21_decimal *result);
int s21_div(s21_decimal value_1, s21_decimal value_2, s21_decimal *result);

// Comparison Operators
int s21_is_less(s21_decimal dec1, s21_decimal dec2);
int s21_is_less_or_equal(s21_decimal dec1, s21_decimal dec2);
int s21_is_greater(s21_decimal dec1, s21_decimal dec2);
int s21_is_greater_or_equal(s21_decimal dec1, s21_decimal dec2);
int s21_is_equal(s21_decimal dec1, s21_decimal dec2);
int s21_is_not_equal(s21_decimal dec1, s21_decimal dec2);

// Convertors and parsers
int s21_from_int_to_decimal(int src, s21_decimal *dst);
int s21_from_float_to_decimal(float src, s21_decimal *dst);
int s21_from_decimal_to_int(s21_decimal src, int *dst);
int s21_from_decimal_to_float(s21_decimal src, float *dst);

// Another functions
int s21_floor(s21_decimal value, s21_decimal *result);
int s21_round(s21_decimal value, s21_decimal *result);
int s21_truncate(s21_decimal value, s21_decimal *result);
int s21_negate(s21_decimal value, s21_decimal *result);

// Support functions
int work_is_less(work_decimal dec1_work, work_decimal dec2_work);
int work_is_equal(work_decimal dec1_work, work_decimal dec2_work);
int work_sub(work_decimal value_1, work_decimal value_2, work_decimal *result);
int work_mul(work_decimal dec1_work, work_decimal dec2_work,
             work_decimal *temp_dec);

void bit_left(work_decimal *dec1_work, int shift);
void div_upper_loop(work_decimal *temp_dec, const work_decimal *dec2_work,
                    work_decimal *dec1_work, int sign, int *res,
                    work_decimal *dec3_work);
void div_down_loop(work_decimal *temp_dec, const work_decimal *dec2_work,
                   work_decimal *dec1_work, work_decimal *dec3_work, int *scale,
                   int *temp_div, int *is_it_first_loop);

void point_to_normal(work_decimal *dec1_work, work_decimal *dec2_work);

int getoverflow(work_decimal *dec);
int pointleft(work_decimal *dec);
int pointright(work_decimal *dec);
int normalize(work_decimal *dec, const int summ, const int sign);

int multiplication(s21_decimal value_1, s21_decimal value_2,
                   s21_decimal *result);
big_decimal binary_mul(s21_decimal value_1, s21_decimal value_2);
s21_decimal s21_round_banking(s21_decimal integral, s21_decimal fractional);
int s21_int128_binary_compare(s21_decimal d1, s21_decimal d2);
int work_decimal_binary_compare(work_decimal d1, work_decimal d2);

static const s21_decimal all_ten_pows[39] = {
    {{0x1, 0x0, 0x0, 0x0}},
    {{0xA, 0x0, 0x0, 0x0}},
    {{0x64, 0x0, 0x0, 0x0}},
    {{0x3E8, 0x0, 0x0, 0x0}},
    {{0x2710, 0x0, 0x0, 0x0}},
    {{0x186A0, 0x0, 0x0, 0x0}},
    {{0xF4240, 0x0, 0x0, 0x0}},
    {{0x989680, 0x0, 0x0, 0x0}},
    {{0x5F5E100, 0x0, 0x0, 0x0}},
    {{0x3B9ACA00, 0x0, 0x0, 0x0}},
    {{0x540BE400, 0x2, 0x0, 0x0}},
    {{0x4876E800, 0x17, 0x0, 0x0}},
    {{(int)0xD4A51000, 0xE8, 0x0, 0x0}},
    {{0x4E72A000, 0x918, 0x0, 0x0}},
    {{0x107A4000, 0x5AF3, 0x0, 0x0}},
    {{(int)0xA4C68000, 0x38D7E, 0x0, 0x0}},
    {{0x6FC10000, 0x2386F2, 0x0, 0x0}},
    {{0x5D8A0000, 0x1634578, 0x0, 0x0}},
    {{(int)0xA7640000, 0xDE0B6B3, 0x0, 0x0}},
    {{(int)0x89E80000, (int)0x8AC72304, 0x0, 0x0}},
    {{0x63100000, 0x6BC75E2D, 0x5, 0x0}},
    {{(int)0xDEA00000, 0x35C9ADC5, 0x36, 0x0}},
    {{(int)0xB2400000, 0x19E0C9BA, 0x21E, 0x0}},
    {{(int)0xF6800000, 0x2C7E14A, 0x152D, 0x0}},
    {{(int)0xA1000000, 0x1BCECCED, 0xD3C2, 0x0}},
    {{0x4A000000, 0x16140148, 0x84595, 0x0}},
    {{(int)0xE4000000, (int)0xDCC80CD2, 0x52B7D2, 0x0}},
    {{(int)0xE8000000, (int)0x9FD0803C, 0x33B2E3C, 0x0}},
    {{0x10000000, 0x3E250261, 0x204FCE5E, 0x0}},
    {{(int)0xA0000000, 0x6D7217CA, 0x431E0FAE, 0x1}},
    {{0x40000000, 0x4674EDEA, (int)0x9F2C9CD0, 0xC}},
    {{(int)0x80000000, (int)0xC0914B26, 0x37BE2022, 0x7E}},
    {{0x0, (int)0x85ACEF81, 0x2D6D415B, 0x4EE}},
    {{0x0, 0x38C15B0A, (int)0xC6448D93, 0x314D}},
    {{0x0, 0x378D8E64, (int)0xBEAD87C0, 0x1ED09}},
    {{0x0, 0x2B878FE8, 0x72C74D82, 0x134261}},
    {{0x0, (int)0xB34B9F10, 0x7BC90715, 0xC097CE}},
    {{0x0, 0xF436A0, (int)0xD5DA46D9, 0x785EE10}},
    {{0x0, 0x98A2240, 0x5A86C47A, 0x4B3B4CA8}}};

#endif  // S21_DECIMAL_H_