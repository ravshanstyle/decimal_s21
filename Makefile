CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror
CHECK = $(shell pkg-config --cflags --libs check)
LIBS = $(CHECK) -lm

all: s21_decimal.a

s21_decimal.o: s21_decimal.c s21_decimal.h
	$(CC) $(CFLAGS) -c s21_decimal.c -o s21_decimal.o

s21_decimal.a: s21_decimal.o
	ar rcs s21_decimal.a s21_decimal.o
	ranlib s21_decimal.a

test: s21_decimal.a test_s21_decimal.c
	$(CC) $(CFLAGS) test_s21_decimal.c s21_decimal.a -o test_runner $(LIBS)
	./test_runner

gcov_report:
	$(CC) $(CFLAGS) --coverage s21_decimal.c test_s21_decimal.c -o gcov_runner $(LIBS)
	./gcov_runner
	lcov -t "s21_decimal" -o report.info -c -d .
	genhtml -o report report.info

clean:
	rm -f *.o *.a *.gcda *.gcno *.gcov report.info test_runner gcov_runner
	rm -rf report

.PHONY: all test gcov_report clean
