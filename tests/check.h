/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * check.h - the tiny test harness shared by every tests/test_*.c program.
 *
 *   #include "check.h"
 *   static void test_something(void) { CHECK(1 + 1 == 2); CHECK_STR(a, "x"); }
 *   int main(void) { RUN(test_something); return CHECK_DONE(); }
 *
 * A failed CHECK prints file:line and the expression and the test goes on;
 * CHECK_DONE() prints a summary and returns the process exit status.
 */
#ifndef CTUI_TESTS_CHECK_H
#define CTUI_TESTS_CHECK_H

#include <math.h>
#include <stdio.h>
#include <string.h>

static int check_failures_;
static int check_count_;

#define CHECK(expr)                                                            \
	do {                                                                   \
		check_count_++;                                                \
		if (!(expr)) {                                                 \
			check_failures_++;                                     \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, \
				__LINE__, #expr);                              \
		}                                                              \
	} while (0)

#define CHECK_STR(got, want)                                                   \
	do {                                                                   \
		const char *check_g_ = (got), *check_w_ = (want);              \
		check_count_++;                                                \
		if (!check_g_ || !check_w_ ||                                  \
		    strcmp(check_g_, check_w_) != 0) {                         \
			check_failures_++;                                     \
			fprintf(stderr,                                        \
				"%s:%d: CHECK_STR failed: %s\n  got:  "        \
				"\"%s\"\n  want: \"%s\"\n",                    \
				__FILE__, __LINE__, #got,                      \
				check_g_ ? check_g_ : "(null)",                \
				check_w_ ? check_w_ : "(null)");               \
		}                                                              \
	} while (0)

#define CHECK_NEAR(got, want, eps)                                             \
	do {                                                                   \
		double check_g_ = (double)(got), check_w_ = (double)(want);    \
		check_count_++;                                                \
		if (!(fabs(check_g_ - check_w_) <= (eps))) {                   \
			check_failures_++;                                     \
			fprintf(stderr,                                        \
				"%s:%d: CHECK_NEAR failed: %s = %.17g, want "  \
				"%.17g\n",                                     \
				__FILE__, __LINE__, #got, check_g_, check_w_); \
		}                                                              \
	} while (0)

#define RUN(fn)                                                                \
	do {                                                                   \
		int check_before_ = check_failures_;                           \
		fn();                                                          \
		fprintf(stderr, "%s %s\n",                                     \
			check_failures_ == check_before_ ? "ok  " : "FAIL",    \
			#fn);                                                  \
	} while (0)

#define CHECK_DONE()                                                           \
	(fprintf(stderr, "%d checks, %d failed\n", check_count_,               \
		 check_failures_),                                             \
	 check_failures_ ? 1 : 0)

#endif /* CTUI_TESTS_CHECK_H */
