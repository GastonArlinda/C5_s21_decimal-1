#include "s21_decimal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Суть этой функции в том что она перекидывает лишние
// биты в более старший int. Используется повсеместно
int getoverflow(work_decimal *dec) {
  int overflow = 0;

  // Проверяем все int в нашем расширенном decimal на то что в них
  // записано больше чем 32 бита (предел стандартного 32битного
  // int), если больше то перекидываем всё лишнее на int вверх
  for (int i = 0; i < 7; i++) {
    dec->bits[i] += overflow;
    overflow = (dec->bits[i] >> 32);
    dec->bits[i] &= MAX4BITE;
  }
  int result = 0;

  // Если нам не хватило даже наших дополнительных int то это
  // переполнение и мы возвращаем единицу
  if (overflow) {
    result = 1;
  }
  return result;
}

// смещаем нашу воображаемую точку влево
// умножая все int на 10 и увеличиваем scale
int pointleft(work_decimal *dec) {
  work_decimal temp_dec = *dec;
  for (int i = 0; i < 7; i++) {
    temp_dec.bits[i] *= 10;
  }
  temp_dec.scale++;
  int overflowed = 0;

  // так как мы в процессе увеличиваем весь decimal,
  // нужно проверить это дело на переполнение
  if (getoverflow(&temp_dec)) {
    overflowed = 1;
  } else {
    *dec = temp_dec;
  }
  return overflowed;
}

// смещаем нашу воображаемую точку вправо
// деля все int на 10, уменьшая scale
int pointright(work_decimal *dec) {
  long int remainder = 0;
  for (int i = 6; i >= 0; i--) {
    dec->bits[i] +=
        remainder << 32;  // тут мы уже записывает остаток в конец меньшего int
    remainder = dec->bits[i] % 10;  // остаток от будущего деления
    dec->bits[i] /= 10;
  }
  dec->scale--;
  return remainder;
}

// функция подготовки work_decimal к нашему
// нормальному s21_decimal
int normalize(work_decimal *dec, const int summ, const int sign) {
  int trash = 0;
  int error = 0;
  int last = 0;
  work_decimal temp_dec = *dec;

  for (int i = 6; i > 2; i--) {
    // перетягиваем все int из work_decimal чтобы они уместились в
    // обычный s21_decimal, если нам позволяет scale, то есть
    // должны остаться лишь три низших int, иначе переполнение
    while (temp_dec.bits[i] != 0 && temp_dec.scale > 0) {
      last = pointright(&temp_dec);
      trash += last;
    }
    if (((last > 5 || last == 5) && trash > 5) ||
        (last == 5 && trash == 5 &&
         (temp_dec.bits[0] % 10) % 2 == 1)) {  // банковское округление
      if (!sign) {
        temp_dec.bits[0]++;
        getoverflow(&temp_dec);
      }
    }
  }

  // если нормализируем из какой-нибудь функции сложения, то если все биты
  // заполенены и есть еще мусор, значит случилось переполнение
  if (summ && trash && temp_dec.bits[0] == 0xffffffff &&
      temp_dec.bits[1] == 0xffffffff && temp_dec.bits[2] == 0xffffffff) {
    error = 1;
  }

  for (int i = 3; i < 7; i++) {
    // проверяем свободны ли int старше 3-го, если
    // нет то это переполнение
    if (temp_dec.bits[i] != 0 && temp_dec.scale == 0) {
      error = 1;
      i = 7;
    }
  }

  // никакого переполнения нету, всё хорошо, можно переписывать
  // work_decimal
  if (!error) {
    *dec = temp_dec;
  }

  return error;
}

// создаем расширенный work_decimal с бОльшим количеством int (и
// сами int больше) на основе обычного s21_decimal
work_decimal decimal_to_work(s21_decimal dec) {
  work_decimal result;
  result.bits[0] = dec.bits[0] & MAX4BITE;
  result.bits[1] = dec.bits[1] & MAX4BITE;
  result.bits[2] = dec.bits[2] & MAX4BITE;
  result.bits[3] = 0;
  result.bits[4] = 0;
  result.bits[5] = 0;
  result.bits[6] = 0;
  result.scale = (dec.bits[3] & SC) >> 16;
  return result;
}

// обратная история
s21_decimal work_to_decimal(work_decimal dec) {
  s21_decimal result;
  result.bits[0] = dec.bits[0] & MAX4BITE;
  result.bits[1] = dec.bits[1] & MAX4BITE;
  result.bits[2] = dec.bits[2] & MAX4BITE;
  result.bits[3] = 0;
  result.bits[3] |= (dec.scale << 16);
  return result;
}

// преобразуем оба децимала на один scale, для
// удобства вычислений. Так как нам удобнее
// домнажать на 10, мы это и делаем здесь
void point_to_normal(work_decimal *dec1_work, work_decimal *dec2_work) {
  if (dec1_work->scale < dec2_work->scale) {
    // домножаем первый, если он меньше
    while (dec1_work->scale != dec2_work->scale && !pointleft(dec1_work))
      ;
  } else if (dec2_work->scale < dec1_work->scale) {
    // домножаем второй, если он меньше
    while (dec1_work->scale != dec2_work->scale && !pointleft(dec2_work))
      ;
  }
}

int s21_is_equal(s21_decimal dec1, s21_decimal dec2) {
  int res = 1;

  // если все биты равны нулю, но знак разный то
  // децималы всё равно равны, по условию
  if ((dec1.bits[3] & MINUS) != (dec2.bits[3] & MINUS) &&
      ((dec1.bits[0] == dec2.bits[0]) && (dec1.bits[1] == dec2.bits[1]) &&
       (dec1.bits[2] == dec2.bits[2]) && (dec1.bits[0] == 0) &&
       (dec1.bits[1] == 0) && (dec1.bits[2] == 0))) {
    res = 1;

    // иначе если знак разный то они не равны
  } else if ((dec1.bits[3] & MINUS) != (dec2.bits[3] & MINUS)) {
    res = 0;

    // иначе нужно привести их к одному scale и сравнить
  } else {
    work_decimal dec1_work, dec2_work;

    // переводит наши децималы в расширенные, чтобы
    // можно было удобно домножить на 10 и сравнить
    dec1_work = decimal_to_work(dec1);
    dec2_work = decimal_to_work(dec2);

    // домножаем один из расширенных децималов, если нужно
    point_to_normal(&dec1_work, &dec2_work);
    for (int i = 2; i >= 0; i--) {
      // если различий в трех низших int нету, значит
      // они равны
      if (dec1_work.bits[i] != dec2_work.bits[i]) {
        res = 0;
      }
    }
  }
  return res;
}

int s21_is_less(s21_decimal dec1, s21_decimal dec2) {
  int res = 1;

  // если децималы равны или второй отрицательный а первый нет
  if ((dec1.bits[3] & MINUS) < (dec2.bits[3] & MINUS) ||
      s21_is_equal(dec1, dec2)) {
    res = 0;

    // если первый децимал отрицательный, а
    // второй нет, то первый меньше
  } else if ((dec1.bits[3] & MINUS) > (dec2.bits[3] & MINUS)) {
    res = 1;
  } else {
    work_decimal dec1_work, dec2_work;
    dec1_work = decimal_to_work(dec1);
    dec2_work = decimal_to_work(dec2);
    point_to_normal(&dec1_work, &dec2_work);  // приводим к одному scale
    for (int i = 2; i >= 0; i--) {
      // если в верхнем int первый децимал меньше то
      // значит и весь первый децимал меньше второго
      if (dec1_work.bits[i] < dec2_work.bits[i]) {
        res = 1;
        i = -1;

        // если в верхнем int первый децимал больше то
        // значит и весь первый децимал больше второго
      } else if (dec1_work.bits[i] > dec2_work.bits[i]) {
        res = 0;
        i = -1;
      }
    }

    // если оба децимала были отрицательными но
    // инвертируем результат
    if (dec1.bits[3] & MINUS) {
      res ? (res = 0) : (res = 1);
    }
  }
  return res;
}

// просто используем функции написанные выше и
// комбинируем их если надо
int s21_is_less_or_equal(s21_decimal dec1, s21_decimal dec2) {
  int res = 0;
  if (s21_is_equal(dec1, dec2) || s21_is_less(dec1, dec2)) {
    res = 1;
  }
  return res;
}

int s21_is_greater(s21_decimal dec1, s21_decimal dec2) {
  int res = 1;
  if (s21_is_less_or_equal(dec1, dec2)) {
    res = 0;
  }
  return res;
}

int s21_is_greater_or_equal(s21_decimal dec1, s21_decimal dec2) {
  int res = 1;
  if (s21_is_less(dec1, dec2)) {
    res = 0;
  }
  return res;
}

int s21_is_not_equal(s21_decimal dec1, s21_decimal dec2) {
  int res = 1;
  if (s21_is_equal(dec1, dec2)) {
    res = 0;
  }
  return res;
}

int s21_add(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  int res = 0;

  // сразу переводим наши децималы в расширенные
  work_decimal dec1_work = decimal_to_work(value_1),
               dec2_work = decimal_to_work(value_2);
  work_decimal temp_result = {{0, 0, 0, 0, 0, 0, 0}, 0};
  point_to_normal(&dec1_work, &dec2_work);  // выравниваем scale

  // числа одинаковые по знакам
  if ((value_1.bits[3] & MINUS) == (value_2.bits[3] & MINUS)) {
    // просто складываем все int и переводим
    // лишние биты в int повыше если нужно
    for (int i = 0; i < 6; i++) {
      temp_result.bits[i] += dec1_work.bits[i] + dec2_work.bits[i];
      getoverflow(&temp_result);
    }

    // в результатирующий расширенный децимал складываем scale
    temp_result.scale = dec1_work.scale;

    // нормализуем для последующей конвертации в обычный
    // децимал. Если есть переполнение то с помощью знака
    // понимаем в какую сторону переполнение
    if (normalize(&temp_result, 1, value_1.bits[3] & MINUS)) {
      if (value_1.bits[3] & MINUS) {
        res = 2;
      } else {
        res = 1;
      }
    } else {
      // складываем всё в наш результирующий обычный децимал
      *result = work_to_decimal(temp_result);
      // знак
      if (value_1.bits[3] & MINUS) {
        result->bits[3] |= MINUS;
      }
      result->bits[3] |= (temp_result.scale << 16) & SC;  // scale
    }
  } else {  // числа разные по знакам

    // заранее запоминаем какой знак у какого децимала
    int first_minus = (value_1.bits[3] & MINUS);
    int second_minus = (value_2.bits[3] & MINUS);

    // делаем оба децимала положительными
    value_1.bits[3] &= ~MINUS;
    value_2.bits[3] &= ~MINUS;
    for (int i = 0; i < 6; i++) {
      // вычитаем из большего децимала меньший
      if (s21_is_less(value_1, value_2)) {
        // вычитаем с помощью инвертирования int меньшего децимала
        temp_result.bits[i] += dec2_work.bits[i] + (~dec1_work.bits[i] + 1);
      } else {
        temp_result.bits[i] += dec1_work.bits[i] + (~dec2_work.bits[i] + 1);
      }
      getoverflow(&temp_result);
    }

    // передаем scale результатирующему расширенному децималу
    temp_result.scale = dec1_work.scale;
    int res_sign = 0x00000000;

    // решаем какой будет в итоге знак у результата
    if ((s21_is_less(value_1, value_2) && second_minus) ||
        (s21_is_greater(value_1, value_2) && first_minus)) {
      res_sign |= MINUS;
    }
    normalize(&temp_result, 0, res_sign);
    *result = work_to_decimal(temp_result);  // передаем все данные результату
    result->bits[3] |= res_sign;
    result->bits[3] |= (temp_result.scale << 16) & SC;
  }
  return res;
}

// просто инвертируем знак для второго децимала и
// складываем с помощью нашего s21_add
int s21_sub(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  value_2.bits[3] = (value_2.bits[3] & MINUS) ? value_2.bits[3] & ~MINUS
                                              : value_2.bits[3] | MINUS;
  return s21_add(value_1, value_2, result);
}

big_decimal decimal_to_big(s21_decimal value_1) {
  big_decimal result;
  result.decimals[0] = value_1;
  result.decimals[1] = (s21_decimal){{0, 0, 0, 0}};
  return result;
}

int s21_decimal_get_not_zero_bit(s21_decimal decimal) {
  int result = -1;
  for (int i = 128 - 1; i >= 0; i--) {
    if ((!!(decimal.bits[i / 32] & (1U << i % 32)))) {
      result = i;
      break;
    }
  }

  return result;
}

int s21_int128_binary_equal_zero(s21_decimal decimal) {
  return decimal.bits[0] == 0 && decimal.bits[1] == 0 && decimal.bits[2] == 0 &&
         decimal.bits[3] == 0;
}

s21_decimal s21_int128_binary_and(s21_decimal decimal1, s21_decimal decimal2) {
  s21_decimal result = (s21_decimal){{0, 0, 0, 0}};
  result.bits[0] = decimal1.bits[0] & decimal2.bits[0];
  result.bits[1] = decimal1.bits[1] & decimal2.bits[1];
  result.bits[2] = decimal1.bits[2] & decimal2.bits[2];
  result.bits[3] = decimal1.bits[3] & decimal2.bits[3];

  return result;
}

int s21_decimal_is_set_bit(s21_decimal decimal, int index) {
  return !!(decimal.bits[index / 32] & (1U << index % 32));
}

int s21_is_set_bit(int number, int index) { return !!(number & (1U << index)); }

int s21_set_bit(int number, int index) { return number | (1U << index); }

s21_decimal s21_int128_binary_shift_left_one(s21_decimal decimal) {
  s21_decimal result = (s21_decimal){{0, 0, 0, 0}};

  int b0 = s21_is_set_bit(decimal.bits[0], 32 - 1);
  unsigned int result0 = decimal.bits[0];
  result0 = result0 << 1;
  result.bits[0] = result0;

  int b1 = s21_is_set_bit(decimal.bits[1], 32 - 1);
  unsigned int result1 = decimal.bits[1];
  result1 = result1 << 1;
  result.bits[1] = result1;

  int b2 = s21_is_set_bit(decimal.bits[2], 32 - 1);
  unsigned int result2 = decimal.bits[2];
  result2 = result2 << 1;
  result.bits[2] = result2;

  unsigned int result3 = decimal.bits[3];
  result3 = result3 << 1;
  result.bits[3] = result3;

  if (b0) {
    result.bits[1] = s21_set_bit(result.bits[1], 0);
  }

  if (b1) {
    result.bits[2] = s21_set_bit(result.bits[2], 0);
  }

  if (b2) {
    result.bits[3] = s21_set_bit(result.bits[3], 0);
  }

  return result;
}

s21_decimal s21_decimal_set_bit(s21_decimal decimal, int index) {
  decimal.bits[index / 32] = s21_set_bit(decimal.bits[index / 32], index % 32);
  return decimal;
}

big_decimal big_decimal_binary_shift_left(big_decimal decimal, int shift) {
  big_decimal result = decimal;
  while (shift > 0) {
    int b0 = s21_decimal_is_set_bit(result.decimals[0], 128 - 1);
    result.decimals[0] = s21_int128_binary_shift_left_one(result.decimals[0]);
    result.decimals[1] = s21_int128_binary_shift_left_one(result.decimals[1]);
    if (b0) {
      result.decimals[1] = s21_decimal_set_bit(result.decimals[1], 0);
    }
    --shift;
  }

  return result;
}

s21_decimal s21_int128_binary_xor(s21_decimal decimal1, s21_decimal decimal2) {
  s21_decimal result = (s21_decimal){{0, 0, 0, 0}};
  result.bits[0] = decimal1.bits[0] ^ decimal2.bits[0];
  result.bits[1] = decimal1.bits[1] ^ decimal2.bits[1];
  result.bits[2] = decimal1.bits[2] ^ decimal2.bits[2];
  result.bits[3] = decimal1.bits[3] ^ decimal2.bits[3];

  return result;
}

big_decimal big_decimal_binary_addition(big_decimal decimal1,
                                        big_decimal decimal2) {
  big_decimal result = decimal1;
  big_decimal tmp = decimal2;

  while (!s21_int128_binary_equal_zero(tmp.decimals[0]) ||
         !s21_int128_binary_equal_zero(tmp.decimals[1])) {
    big_decimal overflow_bits;
    overflow_bits.decimals[0] =
        s21_int128_binary_and(result.decimals[0], tmp.decimals[0]);
    overflow_bits.decimals[1] =
        s21_int128_binary_and(result.decimals[1], tmp.decimals[1]);

    overflow_bits = big_decimal_binary_shift_left(overflow_bits, 1);
    result.decimals[0] =
        s21_int128_binary_xor(result.decimals[0], tmp.decimals[0]);
    result.decimals[1] =
        s21_int128_binary_xor(result.decimals[1], tmp.decimals[1]);

    tmp = overflow_bits;
  }

  return result;
}

big_decimal big_decimal_binary_multiplication(big_decimal decimal1,
                                              s21_decimal decimal2) {
  big_decimal int256_result = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
  big_decimal int256_tmp1 = decimal1;

  int max_bit = s21_decimal_get_not_zero_bit(decimal2);

  for (int i = 0; i <= max_bit; i++) {
    if (s21_decimal_is_set_bit(decimal2, i) != 0) {
      int256_result = big_decimal_binary_addition(int256_result, int256_tmp1);
    }

    int256_tmp1 = big_decimal_binary_shift_left(int256_tmp1, 1);
  }

  return int256_result;
}

big_decimal binary_mul(s21_decimal value_1, s21_decimal value_2) {
  big_decimal result = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
  big_decimal temp = decimal_to_big(value_1);

  int max_bit = s21_decimal_get_not_zero_bit(value_2);

  for (int i = 0; i <= max_bit; i++) {
    if (s21_decimal_is_set_bit(value_2, i) != 0) {
      result = big_decimal_binary_addition(result, temp);
    }
    temp = big_decimal_binary_shift_left(temp, 1);
  }

  return result;
}

int s21_int128_binary_compare(s21_decimal d1, s21_decimal d2) {
  int result = 0;

  for (int i = 128 - 1; i >= 0; i--) {
    int b1 = !!(d1.bits[i / 32] & (1U << i % 32));
    int b2 = !!(d2.bits[i / 32] & (1U << i % 32));

    if (b1 == 0 && b2 != 0) {
      result = -1;
    }

    if (b1 != 0 && b2 == 0) {
      result = 1;
    }

    if (result != 0) {
      break;
    }
  }

  return result;
}

int big_decimal_binary_compare(big_decimal d1, big_decimal d2) {
  int compare = s21_int128_binary_compare(d1.decimals[1], d2.decimals[1]);

  if (compare == 0) {
    compare = s21_int128_binary_compare(d1.decimals[0], d2.decimals[0]);
  }

  return compare;
}

s21_decimal s21_int128_binary_not(s21_decimal decimal) {
  s21_decimal result = (s21_decimal){{0, 0, 0, 0}};
  result.bits[0] = ~decimal.bits[0];
  result.bits[1] = ~decimal.bits[1];
  result.bits[2] = ~decimal.bits[2];
  result.bits[3] = ~decimal.bits[3];

  return result;
}

big_decimal big_decimal_binary_subtraction(big_decimal decimal1,
                                           big_decimal decimal2) {
  big_decimal result;
  decimal2.decimals[0] = s21_int128_binary_not(decimal2.decimals[0]);
  decimal2.decimals[1] = s21_int128_binary_not(decimal2.decimals[1]);

  big_decimal one = decimal_to_big((s21_decimal){{1, 0, 0, 0}});

  decimal2 = big_decimal_binary_addition(decimal2, one);
  result = big_decimal_binary_addition(decimal1, decimal2);

  return result;
}

s21_decimal s21_int128_binary_shift_right_one(s21_decimal decimal) {
  s21_decimal result = (s21_decimal){{0, 0, 0, 0}};

  int b3 = s21_is_set_bit(decimal.bits[3], 0);
  unsigned int result3 = decimal.bits[3];
  result3 = result3 >> 1;
  result.bits[3] = result3;

  int b2 = s21_is_set_bit(decimal.bits[2], 0);
  unsigned int result2 = decimal.bits[2];
  result2 = result2 >> 1;
  result.bits[2] = result2;

  int b1 = s21_is_set_bit(decimal.bits[1], 0);
  unsigned int result1 = decimal.bits[1];
  result1 = result1 >> 1;
  result.bits[1] = result1;

  unsigned int result0 = decimal.bits[0];
  result0 = result0 >> 1;
  result.bits[0] = result0;

  if (b3) {
    result.bits[2] = s21_set_bit(result.bits[2], 32 - 1);
  }

  if (b2) {
    result.bits[1] = s21_set_bit(result.bits[1], 32 - 1);
  }

  if (b1) {
    result.bits[0] = s21_set_bit(result.bits[0], 32 - 1);
  }

  return result;
}

big_decimal big_decimal_binary_shift_right(big_decimal decimal, int shift) {
  big_decimal result = decimal;
  while (shift > 0) {
    int b1 = s21_decimal_is_set_bit(result.decimals[1], 0);
    result.decimals[0] = s21_int128_binary_shift_right_one(result.decimals[0]);
    result.decimals[1] = s21_int128_binary_shift_right_one(result.decimals[1]);
    if (b1) {
      result.decimals[0] = s21_decimal_set_bit(result.decimals[0], 128 - 1);
    }
    --shift;
  }

  return result;
}

big_decimal big_decimal_binary_division(big_decimal decimal1,
                                        big_decimal decimal2,
                                        big_decimal *remainder) {
  big_decimal result;

  // Рассчитываемый в алгоритме частичный остаток при расчетах
  big_decimal partial_remainder = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
  // Рассчитываемое в алгоритме частное
  big_decimal quotient = decimal_to_big((s21_decimal){{0, 0, 0, 0}});

  if (s21_int128_binary_equal_zero(decimal1.decimals[0]) &&
      s21_int128_binary_equal_zero(decimal1.decimals[1])) {
    // Сначала заполняем результаты для частного случая, когда делимое равно 0
    partial_remainder = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
    quotient = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
  } else if (big_decimal_binary_compare(decimal1, decimal2) == -1) {
    // Затем заполняем результаты для частного случая, когда делимое меньше
    // делителя
    quotient = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
    partial_remainder = decimal1;
  } else {
    // В остальных случаях используем деление двоичных чисел методом c
    // восстановлением остатка.

    // Рассчитываем предварительный сдвиг делителя
    int left1 = s21_decimal_get_not_zero_bit(decimal1.decimals[1]);
    if (left1 == -1) {
      left1 = s21_decimal_get_not_zero_bit(decimal1.decimals[0]);
    } else {
      left1 = 128 + left1;
    }

    int left2 = s21_decimal_get_not_zero_bit(decimal2.decimals[1]);
    if (left2 == -1) {
      left2 = s21_decimal_get_not_zero_bit(decimal2.decimals[0]);
    } else {
      left2 = 128 + left2;
    }

    int shift = left1 - left2;

    // Сдвинутый делитель
    big_decimal shifted_divisor =
        big_decimal_binary_shift_left(decimal2, shift);
    // Делимое для промежуточных расчетов, на первом этапе равно decimal1
    big_decimal dividend = decimal1;

    // Флаг необходимости проводить вычитание
    // На первой итерации всегда требуется вычитать
    int need_subtraction = 1;

    // Повторяем действия k+1 раз
    while (shift >= 0) {
      // Определяем необходимое действие (Прибавлять или вычитать Сдвинутый
      // делитель)
      if (need_subtraction == 1) {
        partial_remainder =
            big_decimal_binary_subtraction(dividend, shifted_divisor);
      } else {
        partial_remainder =
            big_decimal_binary_addition(dividend, shifted_divisor);
      }

      // Сдвигаем влево на 1 частное и записываем в младший бит результата 1,
      // если старший бит частичного остатка равен 1
      quotient = big_decimal_binary_shift_left(quotient, 1);
      if (s21_decimal_is_set_bit(partial_remainder.decimals[1], 128 - 1) == 0) {
        quotient.decimals[0] = s21_decimal_set_bit(quotient.decimals[0], 0);
      }

      // Рассчитываем делимое для следующей итерации цикла (сдвиг влево на 1
      // частичного остатка)
      dividend = big_decimal_binary_shift_left(partial_remainder, 1);

      // Если старший бит частичного остатка равен 0, то на следующей итерации
      // необходимо проводить вычитание (Шаг 5 алгоритма)
      if (s21_decimal_is_set_bit(partial_remainder.decimals[1], 128 - 1) == 0) {
        need_subtraction = 1;
      } else {
        need_subtraction = 0;
      }
      --shift;
    }
    // Определяем, требуется ли коррекция остатка
    if (s21_decimal_is_set_bit(partial_remainder.decimals[1], 128 - 1)) {
      partial_remainder =
          big_decimal_binary_addition(partial_remainder, shifted_divisor);
    }
    // Возвращаем на место частичный остаток
    partial_remainder =
        big_decimal_binary_shift_right(partial_remainder, left1 - left2);
  }

  // Заполняем переменные результата (частное и остаток)
  result = quotient;
  if (remainder != NULL) {
    *remainder = partial_remainder;
  }

  return result;
}

int big_decimal_get_shift_to_decimal(big_decimal value) {
  int cnt = 0;

  // Если value содержит только нули, то расчет не производим, резльтат - 0
  if (!(s21_int128_binary_equal_zero(value.decimals[0]) &&
        s21_int128_binary_equal_zero(value.decimals[1]))) {
    // Максимальное 96битное число
    big_decimal max =
        decimal_to_big((s21_decimal){{0xffffffff, 0xffffffff, 0xffffffff, 0}});
    // Находим число, во сколько раз value больше максимального 96битного числа
    big_decimal quotient = big_decimal_binary_division(value, max, NULL);

    // Перебираем все степени 10, чтобы найти ближайшую большую степень, чем
    // quotient Делаем так, потому что это гораздо быстрее, чем делить на 10 в
    // цикле
    while (1) {
      int compare =
          s21_int128_binary_compare(quotient.decimals[0], all_ten_pows[cnt]);
      if (compare == -1 || compare == 0) {
        break;
      }
      ++cnt;
    }

    // Проверяем, что value впишется в 96 бит, если поделить на 10 в найденной
    // степени
    big_decimal tmp = big_decimal_binary_division(
        value, decimal_to_big(all_ten_pows[cnt]), NULL);
    // Если не помещается, то берем следующую степень
    if (!s21_int128_binary_equal_zero(tmp.decimals[1]) ||
        tmp.decimals[0].bits[3] != 0) {
      ++cnt;
    }
  }

  return cnt;
}

int s21_is_correct_decimal(s21_decimal decimal) {
  int code = 1;

  if ((decimal.bits[3] & FIRST) || (decimal.bits[3] & SECOND)) {
    code = 0;
  } else {
    int scale = (decimal.bits[3] & SC) >> 16;
    if (scale < 0 || scale > 28) {
      code = 0;
    }
  }

  return code;
}

int multiplication(s21_decimal value_1, s21_decimal value_2,
                   s21_decimal *result) {
  int error = 0;
  int scale1 = (value_1.bits[3] & SC) >> 16;  // Вычисляем степени
  int scale2 = (value_2.bits[3] & SC) >> 16;

  value_1.bits[3] = 0;  // Зануляем всё кроме мантиссы
  value_2.bits[3] = 0;
  // Перемножаем множители без степеней как два огромных целых числа
  big_decimal big_dec = binary_mul(value_1, value_2);
  // Рассчитываем, на сколько необходимо поделить результат, чтобы вписать его в
  // 96бит числа decimal
  int shift = big_decimal_get_shift_to_decimal(big_dec);

  int res_scale = scale1 + scale2 - shift;

  if (res_scale < 0) {
    // Анализируем степень результата на корректность
    error = 1;
    *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
  } else {
    // Если результат надо делить более, чем на 10^28, то уменьшаем его, чтобы
    // далее делить на 10^28
    while (shift > 28) {
      big_dec = big_decimal_binary_division(
          big_dec, decimal_to_big((s21_decimal){{10, 0, 0, 0}}), NULL);
      --shift;
    }

    // Если слишком много цифр после запятой получается в результате, то
    // корректируем результат
    if (res_scale > 28) {
      big_decimal tmp = big_dec;
      int tmp_scale = res_scale;
      while (tmp_scale > 28) {
        tmp = big_decimal_binary_division(
            tmp, decimal_to_big((s21_decimal){{10, 0, 0, 0}}), NULL);
        --tmp_scale;
      }
      shift = res_scale - tmp_scale + shift;
      res_scale = tmp_scale;
    }

    big_decimal remainder = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
    big_decimal powerten = decimal_to_big(all_ten_pows[shift]);

    // Уменьшаем результат, чтобы он поместился в 96бит числа decimal
    // remainder - остаток от деления при этом
    big_dec = big_decimal_binary_division(big_dec, powerten, &remainder);
    // Устанавливаем для остатка степень, чтобы выполнить банковское округление
    // чисел decimal
    // s21_decimal_set_power(&remainder.decimals[0], shift);
    remainder.decimals[0].bits[3] += shift << 16;
    // Выполняем банковское округления результата, исходя из остатка от деления
    // remainder
    big_dec.decimals[0] =
        s21_round_banking(big_dec.decimals[0], remainder.decimals[0]);
    // Устанавливаем степень результата
    // s21_decimal_set_power(&big_dec.decimals[0], res_scale);
    big_dec.decimals[0].bits[3] += res_scale << 16;
    // Анализируем результат на корректность (переполнение)
    if (!s21_int128_binary_equal_zero(big_dec.decimals[1]) ||
        !s21_is_correct_decimal(big_dec.decimals[0])) {
      error = 1;
      *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
    } else {
      *result = big_dec.decimals[0];
    }
  }

  return error;
}

int s21_mul(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  int error = 0;

  if (!result) {
    error = 4;
  } else if (!s21_is_correct_decimal(value_1) ||
             !s21_is_correct_decimal(value_2)) {
    // Проверяем, что value_1 и value_2 являются корректными decimal
    error = 4;
    *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
  } else {
    s21_decimal temp_dec = {{0, 0, 0, 0}};

    error = multiplication(value_1, value_2, &temp_dec);

    if ((value_1.bits[3] & MINUS) != (value_2.bits[3] & MINUS)) {
      s21_negate(temp_dec, &temp_dec);
    }

    if (error == 1) {
      if (temp_dec.bits[3] & MINUS) {
        error = 2;
      }
    }

    if (error == 0 && s21_is_not_equal(value_1, (s21_decimal){{0, 0, 0, 0}}) &&
        s21_is_not_equal(value_2, (s21_decimal){{0, 0, 0, 0}}) &&
        s21_is_equal(temp_dec, (s21_decimal){{0, 0, 0, 0}})) {
      error = 2;
    }

    *result = temp_dec;
  }

  return error;
}

void s21_decimal_leveling(s21_decimal value_1, s21_decimal value_2,
                          big_decimal *value_1l, big_decimal *value_2l) {
  int scale1 = (value_1.bits[3] & SC) >> 16;
  int scale2 = (value_2.bits[3] & SC) >> 16;
  s21_decimal tmp1 = value_1;
  s21_decimal tmp2 = value_2;

  tmp1.bits[3] = 0;
  tmp2.bits[3] = 0;

  if (scale1 > scale2) {
    *value_1l = decimal_to_big(tmp1);
    *value_2l = binary_mul(tmp2, all_ten_pows[scale1 - scale2]);
  } else if (scale1 < scale2) {
    *value_1l = binary_mul(tmp1, all_ten_pows[scale2 - scale1]);
    *value_2l = decimal_to_big(tmp2);
  } else {
    *value_1l = decimal_to_big(tmp1);
    *value_2l = decimal_to_big(tmp2);
  }
}

int s21_div_calc_fractional(big_decimal *res, big_decimal value_2l,
                            big_decimal *remainder) {
  int power = 0;
  big_decimal number = *res;

  big_decimal tmp;
  big_decimal tmp_remainder = *remainder;

  // Производим расчеты пока остаток не будет полностью равен нулю или пока
  // степень не станет максимально допустимой (28)
  while ((!s21_int128_binary_equal_zero((*remainder).decimals[0]) ||
          !s21_int128_binary_equal_zero((*remainder).decimals[1])) &&
         power < 28) {
    // Сохраняем значения полученных числа и остатка перед дальнейшей итерацией
    // расчетов Чтобы вернуть эти значения в случае переполнения
    big_decimal number_stored = number;
    big_decimal remainder_stored = tmp_remainder;

    // Производим расчеты (вжик вжик, см бриф)
    number =
        big_decimal_binary_multiplication(number, (s21_decimal){{10, 0, 0, 0}});
    tmp_remainder = big_decimal_binary_multiplication(
        tmp_remainder, (s21_decimal){{10, 0, 0, 0}});
    tmp = big_decimal_binary_division(tmp_remainder, value_2l, &tmp_remainder);
    number = big_decimal_binary_addition(number, tmp);

    // Возвращаем предварительно сохраненные число и остаток, если произошло
    // переполнение при расчете
    if (!s21_is_correct_decimal(number.decimals[0])) {
      number = number_stored;
      tmp_remainder = remainder_stored;
      break;
    }

    ++power;
  }

  *res = number;
  *remainder = tmp_remainder;

  return power;
}

int s21_div_handle(big_decimal value_2l, big_decimal res, big_decimal remainder,
                   s21_decimal *result) {
  int error = 0;

  // рассчитываем дробную часть нашего результата и получаем в res результат,
  // включая дробную часть после расчетов в remainder останется остаток от
  // деления (который не поместился в дробную часть) power1 - значение степени
  // результата
  int power1 = s21_div_calc_fractional(&res, value_2l, &remainder);

  big_decimal tmp_res = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
  // Переводи остаток, полученный в расчете выше, в decimal, чтобы использовать
  // его для округления power2 - значение степени данного decimal
  int power2 = s21_div_calc_fractional(&tmp_res, value_2l, &remainder);

  // Устанавливаем полученную степень для нашего остатка
  // s21_decimal_set_power(&tmp_res.decimals[0], power2);
  tmp_res.decimals[0].bits[3] |= power2 << 16;

  if (s21_is_equal(tmp_res.decimals[0], (s21_decimal){{5, 0, 0, 0x00010000}})) {
    if (!s21_int128_binary_equal_zero(remainder.decimals[0]) ||
        !s21_int128_binary_equal_zero(remainder.decimals[1])) {
      // Если остаток от деления в виде decimal получился ровно 0.5, но после
      // вычисления остаток от деления не равен 0, то корректируем остаток, т.к.
      // фактически он больше 0.5: 0.5 + 0.0000000000000000000000000001 =
      // 0.5000000000000000000000000001
      s21_add(tmp_res.decimals[0], (s21_decimal){{0x1, 0x0, 0x0, 0x1C0000}},
              &tmp_res.decimals[0]);
    }
  }
  // Выполняем банковское округления результата, исходя из остатка от деления
  res.decimals[0] = s21_round_banking(res.decimals[0], tmp_res.decimals[0]);

  // Устанавливаем степень результата
  // s21_decimal_set_power(&res.decimals[0], power1);
  res.decimals[0].bits[3] |= power1 << 16;
  // Анализируем результат на корректность (переполнение)
  if (!s21_int128_binary_equal_zero(res.decimals[1]) ||
      !s21_is_correct_decimal(res.decimals[0])) {
    error = 1;
    *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
  } else {
    *result = res.decimals[0];
  }

  return error;
}

int s21_div(s21_decimal value_1, s21_decimal value_2, s21_decimal *result) {
  int error = 0;

  if (!result) {
    // Если указатель на decimal является NULL
    error = 4;
  } else if (!s21_is_correct_decimal(value_1) ||
             !s21_is_correct_decimal(value_2)) {
    // Проверяем, что value_1 и value_2 являются корректными decimal
    error = 4;
    *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
  } else if (s21_is_equal(value_2, (s21_decimal){{0, 0, 0, 0}})) {
    // Проверяем случай, когда value_2 является нулем
    error = 3;
    *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
  } else {
    // В остальных случаях считаем частное
    *result = (s21_decimal){{0, 0, 0, 0}};
    int sign1 = value_1.bits[3] & MINUS;
    int sign2 = value_2.bits[3] & MINUS;
    big_decimal value_1l;
    big_decimal value_2l;
    // Выравниваем степени делимого и делителя (знак и степень при этом будут
    // обнулены)
    s21_decimal_leveling(value_1, value_2, &value_1l, &value_2l);

    big_decimal remainder = decimal_to_big((s21_decimal){{0, 0, 0, 0}});
    big_decimal res;

    // Делим выравненное делимое на выравненный делитель, остаток от деления
    // будет записан в remainder
    res = big_decimal_binary_division(value_1l, value_2l, &remainder);

    if (res.decimals[0].bits[3] != 0 ||
        !s21_int128_binary_equal_zero(res.decimals[1])) {
      // Если целый результат деления уже не помещается в decimal
      // (переполнение), то возвращаем соответствующие ошибки
      if (sign1 != sign2) {
        error = 2;
      } else {
        error = 1;
      }
      *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
    } else {
      // В остальных случаях продолжаем вычисления и вызываем вспомогательную
      // функцию для расчетов
      error = s21_div_handle(value_2l, res, remainder, result);

      // Если знаки делимого и делителя отличаются, то необходимо сделать
      // результат отрицательным
      if (sign1 != sign2) {
        result->bits[3] |= MINUS;
      }
      // Корректируем код ответа от вспомогательной функции в случае ошибки и
      // отрицательного результата
      if ((result->bits[3] & MINUS) && error == 1) {
        error = 2;
      }
      // Обрабатываем ситуацию, что результат получился слишком маленький (0 <
      // |x| < 1e-28)
      if (error == 0 &&
          s21_is_not_equal(value_1, (s21_decimal){{0, 0, 0, 0}}) &&
          s21_is_equal(*result, (s21_decimal){{0, 0, 0, 0}})) {
        error = 2;
      }
    }
  }

  return error;
}

int s21_from_int_to_decimal(int src, s21_decimal *dst) {
  s21_decimal temp_dec = {{0, 0, 0, 0}};
  temp_dec.bits[3] = (src < 0) ? MINUS : 0;  // вычисляем знак src

  // просто передаем src нашему int, но уже без
  // последнего бита, так как мы его отдельно записали
  // в наш 4 int, если же src отрицательное число, то
  // нам нужно его инвертировать (что поделать, так
  // устроен signed int)
  temp_dec.bits[0] = (src < 0) ? (~src & ~MINUS) + 1 : src & ~MINUS;
  *dst = temp_dec;
  return 0;
}

int s21_from_decimal_to_int(s21_decimal src, int *dst) {
  int res = 0;
  work_decimal dec_work = decimal_to_work(src);
  int scale =
      (((src.bits[3] & SC) >> 16) > 28) ? 28 : ((src.bits[3] & SC) >> 16);

  // отбрасываем все цифры после точки, всю
  // вещественную часть децимал
  for (int i = 0; i < scale; i++) {
    pointright(&dec_work);
  }

  // проверяем не переполнен ли у нас децимал. должен быть занят
  // только самый низший int, чтобы небыло переполнения
  for (int i = 6; i > 0; i--) {
    if (dec_work.bits[i] != 0) {
      res = 1;
      i = 0;
    }
  }
  if (!res) {
    // передаем из нашено int в dst int
    *dst = dec_work.bits[0];

    // умножаем dst на -1 если децимал отрицательный
    *dst *= (src.bits[3] & MINUS) ? -1 : 1;
  }
  return res;
}

int s21_from_decimal_to_float(s21_decimal src, float *dst) {
  double temp_result = 0;  // временный результат
  int offset = 0;

  // проверяем каждый наш бит в децимале. Прибавляем к нашему
  // временному double результату бит переведенный из двоичной
  // системы в десятичную
  for (int i = 0; i < 96; i++) {
    if ((src.bits[i / 32] & (1l << i % 32))) {  // добавил буковку l к 1

      double temp_pow = 1;
      for (int j = 0; j < i; j++) {
        temp_pow *= 2;
      }
      temp_result += temp_pow;
    }
  }

  // вычисляем смещение точки, если оно больше нуля то делим наш
  // результат на 10 пока точка не будет в нужном месте
  if ((offset = (src.bits[3] & ~MINUS) >> 16) > 0) {
    for (int i = offset; i > 0; i--) {
      temp_result /= 10.;
    }
  }
  *dst = (float)temp_result;  // переводим double результат в float
  *dst *= (src.bits[3] & MINUS) ? -1 : 1;  // умножаем на -1 если таков знак
  return 0;
}

s21_decimal s21_decimal_get_from_char(char c) {
  s21_decimal result;

  switch (c) {
    case '0':
      result = (s21_decimal){{0, 0, 0, 0}};
      break;
    case '1':
      result = (s21_decimal){{1, 0, 0, 0}};
      break;
    case '2':
      s21_from_int_to_decimal(2, &result);
      break;
    case '3':
      s21_from_int_to_decimal(3, &result);
      break;
    case '4':
      s21_from_int_to_decimal(4, &result);
      break;
    case '5':
      s21_from_int_to_decimal(5, &result);
      break;
    case '6':
      s21_from_int_to_decimal(6, &result);
      break;
    case '7':
      s21_from_int_to_decimal(7, &result);
      break;
    case '8':
      s21_from_int_to_decimal(8, &result);
      break;
    case '9':
      s21_from_int_to_decimal(9, &result);
      break;
  }

  return result;
}

int s21_get_float_exp_from_string(char *str) {
  int result = 0;
  char *ptr = str;
  while (*ptr) {
    if (*ptr == 'E') {
      ++ptr;
      result = strtol(ptr, NULL, 10);
      break;
    }
    ++ptr;
  }

  return result;
}

s21_decimal s21_float_string_to_decimal(char *str) {
  int digits_counter = 6;
  s21_decimal result = (s21_decimal){{0, 0, 0, 0}};
  char *ptr = str;

  // Получаем в любом случае заново степень float из научной записи, а не
  // передаем полученную ранее в s21_from_float_to_decimal, т.к. она могла
  // измениться из-за округления
  int exp = s21_get_float_exp_from_string(str);

  while (*ptr) {
    if (*ptr == '.') {
      // Точку игнорируем, итоговую степень decimal мы будем отдельно считать
      ++ptr;
      continue;
    } else if (*ptr >= '0' && *ptr <= '9') {
      // Переводим цифры в строке в decimal, начиная с первой, используя
      // арифметику decimal
      s21_decimal tmp = (s21_decimal){{0, 0, 0, 0}};
      s21_mul(s21_decimal_get_from_char(*ptr), all_ten_pows[digits_counter],
              &tmp);
      s21_add(result, tmp, &result);
      --digits_counter;
      ++ptr;
    } else {
      break;
    }
  }
  // Учитываем, что в научной записи степень уже сдвинутая, чтобы далее
  // корректно выставить степень decimal
  exp = exp - 6;

  if (exp > 0) {
    // Для положительной степени производим умножение на 10^exp
    s21_mul(result, all_ten_pows[exp], &result);
  } else if (exp < 0) {
    // Для отрицательной степени производим деление на 10^exp
    if (exp < -28) {
      // Отдельно обрабатываем очень маленькие степени, т.к. мы не сможем
      // поделить больше чем на 10^28
      s21_div(result, all_ten_pows[28], &result);
      exp += 28;
    }
    s21_div(result, all_ten_pows[-exp], &result);
  }

  // Наслаждаемся результатом
  return result;
}

int s21_from_float_to_decimal(float src, s21_decimal *dst) {
  int error = 0;
  // printf("%f%d", src, dst->bits[0]);

  if (!dst) {
    // Если указатель на decimal является NULL
    error = 1;
  } else if (src == 0.0) {
    // Отдельно обрабатываем 0.0, чтобы не выполнять лишних вычислений
    error = 0;
    *dst = (s21_decimal){{0, 0, 0, 0}};
    if (signbit(src) != 0) {
      // Добавляем знак для -0.0
      dst->bits[3] |= MINUS;
    }
  } else if (isinf(src) || isnan(src)) {
    // Отдельно обрабатываем +inf, -inf, +nan, и -nan
    error = 1;
    *dst = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
    if (signbit(src) != 0) {
      // Добавляем знак для -nan и -inf
      dst->bits[3] |= MINUS;
    }
  } else if (fabsf(src) > MAX_FLOAT_TO_CONVERT) {
    // MAX_FLOAT_TO_CONVERT - максимальное число, которое можно сконвертировать
    // в decimal
    error = 1;
    *dst = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
    if (signbit(src) != 0) {
      // Добавляем знак для для отрицательного числа
      dst->bits[3] |= MINUS;
    }
  } else if (fabsf(src) < MIN_FLOAT_TO_CONVERT) {
    // MIN_FLOAT_TO_CONVERT - минимальное число, которое можно сконвертировать в
    // decimal
    error = 1;
    *dst = (s21_decimal){{0, 0, 0, 0}};
  } else {
    *dst = (s21_decimal){{0, 0, 0, 0}};
    s21_decimal result;
    char flt[64];

    // Приводим float в научную запись - одна цифра до запятой и 6 цифр после
    // запятой В итоге мы получаем 7 значащих цифр, которые нам и нужно
    // перевести по заданию из float в decimal
    sprintf(flt, "%.6E", fabsf(src));
    // Получаем степень float из научной записи.

    int exp = s21_get_float_exp_from_string(flt);

    // Анализируем полученную степень.
    if (exp <= -23) {
      // Если степень слишком маленькая, то не все значащие цифры поместятся в
      // decimal Поэтому корректируем точность и заново приводим float в научную
      // запись уже с новой точностью
      int float_precision = exp + 28;
      sprintf(flt, "%.*E", float_precision, fabsf(src));
    }

    // Переводим строку с научной нотацией в decimal
    result = s21_float_string_to_decimal(flt);

    // Определяем знак результата, исходя из знака числа src (типа float)
    if (signbit(src) != 0) {
      result.bits[3] |= MINUS;
    }

    *dst = result;
  }

  return error;
}

// округление но в сторону минус бесконечности
int s21_floor(s21_decimal value, s21_decimal *result) {
  work_decimal dec_work = decimal_to_work(value);

  int trash = 0;
  int scale =
      (((value.bits[3] & SC) >> 16) > 28) ? 28 : ((value.bits[3] & SC) >> 16);

  // убираем всю вещественную часть, но запоминаем последнюю цифру
  // и весь остаток в сумме
  for (int i = 0; i < scale; i++) {
    int last = pointright(&dec_work);
    trash += last;
  }

  // если общий остаток больше нуля и число отрицательное то
  // прибавляем единичку, чтобы сделать наше отрицательное
  // число ближе к минус бесконечности
  if (trash > 0 && value.bits[3] & MINUS) {
    dec_work.bits[0]++;
    getoverflow(&dec_work);
  }
  *result = work_to_decimal(dec_work);
  result->bits[3] = value.bits[3] & MINUS;
  return 0;
}

s21_decimal s21_round_banking(s21_decimal integral, s21_decimal fractional) {
  s21_decimal zerofive = {{5, 0, 0, 0x00010000}};
  s21_decimal result;

  if (s21_is_equal(fractional, zerofive)) {
    // Если дробная часть ровно 0.5
    if ((integral.bits[0] & 1) != 1) {
      // Если целая часть четная, то оставляем её
      result = integral;
    } else {
      // Если целая часть нечетная, то увеличиваем её на 1
      if (s21_is_equal(integral, (s21_decimal){{0xffffffff, 0xffffffff,
                                                0xffffffff, 0}})) {
        result.bits[3] = 1;  // Обозначаем переполнение
        // printf("errrrr 1\n");
      } else {
        s21_add(integral, (s21_decimal){{1, 0, 0, 0}}, &result);
        // printf("added 1\n");
      }
    }
  } else if (s21_is_greater(fractional, zerofive)) {
    // Если дробная часть > 0.5, то увеличиваем целую часть на 1
    if (s21_is_equal(integral,
                     (s21_decimal){{0xffffffff, 0xffffffff, 0xffffffff, 0}})) {
      result.bits[3] = 1;  // Обозначаем переполнение
      // printf("errrrr 2\n");
    } else {
      s21_add(integral, (s21_decimal){{1, 0, 0, 0}}, &result);
      // printf("added 1\n");
    }
  } else {
    // Если дробная часть < 0.5, то оставляем целую часть без изменений
    result = integral;
  }

  return result;
}

// для округления используем банковского округление,
// то есть округление в сторону чётных
int s21_round(s21_decimal value, s21_decimal *result) {
  int error = 0;
  int scale = (value.bits[3] & SC) >> 16;
  if (!result) {
    // Если указатель на decimal является NULL
    error = 1;
  } else if (scale > 28 || scale < 0 || (value.bits[3] & FIRST) ||
             (value.bits[3] & SECOND)) {
    // Проверяем, что value является корректными decimal
    error = 1;
    *result = (s21_decimal){{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF}};
  } else {
    // В остальных случаях округляем
    *result = (s21_decimal){{0, 0, 0, 0}};
    int sign = value.bits[3] & MINUS;
    s21_decimal fractional;
    s21_decimal value_unsigned_truncated;
    // Убираем знак

    s21_decimal value_unsigned = value;
    value_unsigned.bits[3] = value.bits[3] & ~MINUS;
    // Убираем дробную часть числа
    s21_truncate(value_unsigned, &value_unsigned_truncated);

    // Считаем убранную дробную часть числа
    s21_sub(value_unsigned, value_unsigned_truncated, &fractional);

    // Производим округление, исходя из дробной части числа
    value_unsigned_truncated =
        s21_round_banking(value_unsigned_truncated, fractional);

    *result = value_unsigned_truncated;
    // Возвращаем знак
    result->bits[3] |= sign;
  }

  return error;
}

// просто делаем s21_floor но без знака. Знак потом
// возвращаем. Так получается что мы в любом случае
// просто отбрасываем вещественную часть
int s21_truncate(s21_decimal value, s21_decimal *result) {
  int sign = value.bits[3] & MINUS;
  value.bits[3] &= ~MINUS;
  s21_floor(value, result);
  result->bits[3] |= sign;
  return 0;
}

// инвертируем знаковый бит в децимал
int s21_negate(s21_decimal value, s21_decimal *result) {
  value.bits[3] =
      (value.bits[3] & MINUS) ? value.bits[3] & ~MINUS : value.bits[3] | MINUS;
  *result = value;
  return 0;
}