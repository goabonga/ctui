/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_toast.c - toast stack, expiry and geometry.
 */
#include "check.h"
#include <ctui/toast.h>

static void test_push(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	CHECK(t.count == 0);
	ctui_toasts_push(&t, CTUI_TOAST_OK, "saved", 10, 3);
	ctui_toasts_push(&t, CTUI_TOAST_ERROR, "failed", 10, 0);
	CHECK(t.count == 2);
	CHECK_STR(t.items[0].text, "saved");
	CHECK(t.items[0].level == CTUI_TOAST_OK);
	CHECK_NEAR(t.items[0].expires, 13, 1e-9);
	CHECK_NEAR(t.items[1].expires, 0, 1e-9);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, NULL, 10, 1);
	CHECK_STR(t.items[2].text, "");
}

static void test_overflow_drops_the_oldest(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	char text[8];
	for (int i = 0; i < CTUI_TOAST_MAX + 2; i++) {
		text[0] = (char)('a' + i);
		text[1] = '\0';
		ctui_toasts_push(&t, CTUI_TOAST_INFO, text, 0, 5);
	}
	CHECK(t.count == CTUI_TOAST_MAX);
	CHECK_STR(t.items[0].text, "c");
	CHECK_STR(t.items[CTUI_TOAST_MAX - 1].text, "f");
}

static void test_truncation(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	char long_text[300];
	for (int i = 0; i < 299; i++)
		long_text[i] = 'x';
	long_text[299] = '\0';
	ctui_toasts_push(&t, CTUI_TOAST_INFO, long_text, 0, 1);
	CHECK(strlen(t.items[0].text) == CTUI_TOAST_TEXT - 1);

	/* a multi-byte character is never cut in half */
	for (int i = 0; i < 118; i++)
		long_text[i] = 'x';
	memcpy(long_text + 118, "\xc3\xa9\xc3\xa9",
	       5); /* two e-acute and NUL */
	ctui_toasts_push(&t, CTUI_TOAST_INFO, long_text, 0, 1);
	CHECK(strlen(t.items[1].text) == 118);
}

static void test_expire(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "a", 0, 2);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "sticky", 0, 0);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "c", 0, 4);
	CHECK(ctui_toasts_expire(&t, 1) == 0);
	CHECK(ctui_toasts_expire(&t, 2) == 1);
	CHECK(t.count == 2);
	CHECK_STR(t.items[0].text, "sticky");
	CHECK(ctui_toasts_expire(&t, 100) == 1);
	CHECK(t.count == 1);
	CHECK_STR(t.items[0].text, "sticky");
}

static void test_dismiss(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "a", 0, 0);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "b", 0, 0);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "c", 0, 0);
	CHECK(ctui_toasts_dismiss(&t, 1));
	CHECK(t.count == 2);
	CHECK_STR(t.items[1].text, "c");
	CHECK(!ctui_toasts_dismiss(&t, 2));
}

static void test_geometry(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "hello", 0, 0);
	ctui_toasts_push(&t, CTUI_TOAST_WARN,
			 "a much longer message than the first one", 0, 0);
	ctui_rect box;
	CHECK(ctui_toasts_geometry(&t, 1, 24, 80, 0, &box));
	CHECK(box.y == 1 && box.height == 1);
	CHECK(box.width == 5 + 7);
	CHECK(box.x + box.width ==
	      79); /* right-aligned with a one column margin */
	CHECK(ctui_toasts_geometry(&t, 1, 24, 80, 1, &box));
	CHECK(box.y == 2);
	CHECK(!ctui_toasts_geometry(&t, 1, 24, 80, 2, &box));

	/* a narrow screen clips the long one */
	CHECK(ctui_toasts_geometry(&t, 1, 24, 30, 1, &box));
	CHECK(box.width == 28 && box.x == 1);

	/* rows past the bottom do not exist */
	CHECK(!ctui_toasts_geometry(&t, 1, 2, 80, 1, &box));
	CHECK(!ctui_toasts_geometry(&t, 1, 24, 8, 0, &box));
}

static void test_hit(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "hello", 0, 0);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "world", 0, 0);
	CHECK(ctui_toasts_hit(&t, 1, 24, 80, 1, 70) == 0);
	CHECK(ctui_toasts_hit(&t, 1, 24, 80, 2, 70) == 1);
	CHECK(ctui_toasts_hit(&t, 1, 24, 80, 3, 70) == -1);
	CHECK(ctui_toasts_hit(&t, 1, 24, 80, 1, 10) == -1);
}

static void test_limit(void)
{
	ctui_toasts t;
	ctui_toasts_init(&t);
	t.limit = 2;
	const char *names[] = {"a", "b", "c", "d"};
	for (int i = 0; i < 4; i++)
		ctui_toasts_push(&t, CTUI_TOAST_INFO, names[i], 0, 0);
	CHECK(t.count == 2);
	CHECK_STR(t.items[0].text, "c");
	CHECK_STR(t.items[1].text, "d");

	/* lowering the limit drops the oldest on the next push */
	t.limit = 1;
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "e", 0, 0);
	CHECK(t.count == 1 && !strcmp(t.items[0].text, "e"));

	/* more than the capacity is refused */
	t.limit = 1000;
	for (int i = 0; i < CTUI_TOAST_CAPACITY + 5; i++)
		ctui_toasts_push(&t, CTUI_TOAST_INFO, "x", 0, 0);
	CHECK(t.count == CTUI_TOAST_CAPACITY);
}

int main(void)
{
	RUN(test_push);
	RUN(test_overflow_drops_the_oldest);
	RUN(test_truncation);
	RUN(test_expire);
	RUN(test_dismiss);
	RUN(test_geometry);
	RUN(test_hit);
	RUN(test_limit);
	return CHECK_DONE();
}
