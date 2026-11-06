/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <curses.h>
#include <stdint.h>
#include <stdlib.h>

#include "screen.h"
#include <ctui/logview.h>

#define W 60
#define H 5

static void fill(ctui_logview *v, int n)
{
	for (int i = 0; i < n; i++) {
		char msg[32];
		snprintf(msg, sizeof msg, "message %d", i);
		ctui_logview_push(v, i, (ctui_log_level)(i % CTUI_LOG_LEVELS),
				  "src", msg);
	}
}

static void test_ring(void)
{
	ctui_logview v;
	CHECK(ctui_logview_init(&v, 4));
	fill(&v, 3);
	CHECK(v.count == 3 && v.dropped == 0);
	fill(&v, 10); /* 13 pushed in all */
	CHECK(v.count == 4 && v.dropped == 9 && v.next_seq == 13);
	CHECK(ctui_logview_at(&v, 0)->seq == 9 &&
	      ctui_logview_at(&v, 3)->seq == 12);
	CHECK(ctui_logview_at(&v, 4) == NULL);
	CHECK_STR(ctui_logview_at(&v, 3)->message, "message 9");
	/* the text is copied */
	char buf[16] = "temporary";
	ctui_logview_push(&v, 0, CTUI_LOG_INFO, buf, buf);
	memset(buf, 'x', sizeof buf - 1);
	CHECK_STR(ctui_logview_at(&v, 3)->message, "temporary");
	CHECK_STR(ctui_logview_at(&v, 3)->source, "temporary");
	ctui_logview_clear(&v);
	CHECK(v.count == 0 && ctui_logview_visible(&v) == 0);
	fill(&v, 1);
	CHECK(ctui_logview_at(&v, 0)->seq == 14);
	ctui_logview_free(&v);
}

static void test_zero_capacity(void)
{
	ctui_logview v;
	CHECK(ctui_logview_init(&v, 0));
	CHECK(!ctui_logview_push(&v, 0, CTUI_LOG_INFO, "a", "b"));
	CHECK(v.count == 0 && v.dropped == 1 && ctui_logview_at(&v, 0) == NULL);
	CHECK(ctui_logview_visible(&v) == 0 && ctui_logview_top(&v, W, H) == 0);
	size_t i;
	CHECK(!ctui_logview_hit(&v, 0, 0, W, H, 0, 0, &i));
	CHECK(!ctui_logview_scroll(&v, W, H, -3) &&
	      ctui_logview_selected(&v) == NULL);
	ctui_logview_free(&v);
	CHECK(!ctui_logview_init(&v, CTUI_LOG_CAPACITY_MAX + 1U));
}

static void test_sanitize(void)
{
	ctui_logview v;
	ctui_logview_init(&v, 4);
	ctui_logview_push(&v, 0, CTUI_LOG_INFO, "so\x1b[31mrce",
			  "a\x1b[2Jb\nc\td\x07\xff e\xc3\xa9");
	const ctui_log_record *r = ctui_logview_at(&v, 0);
	CHECK_STR(r->message, "a?[2Jb c d?? e\xc3\xa9");
	CHECK_STR(r->source, "so?[31mrce");
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	ctui_logview_push(&v, 0, (ctui_log_level)99, NULL, NULL);
	CHECK(ctui_logview_at(&v, 1)->level == CTUI_LOG_ERROR &&
	      ctui_logview_at(&v, 1)->message[0] == 0);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	ctui_logview_push(&v, 0, (ctui_log_level)-4, "", "x");
	CHECK(ctui_logview_at(&v, 2)->level == CTUI_LOG_TRACE);
	char longsrc[200];
	memset(longsrc, 's', sizeof longsrc - 1);
	longsrc[sizeof longsrc - 1] = 0;
	ctui_logview_push(&v, 0, CTUI_LOG_INFO, longsrc, "m");
	CHECK(strlen(ctui_logview_at(&v, 3)->source) ==
	      CTUI_LOG_SOURCE_MAX - 1);
	ctui_logview_free(&v);
}

static void test_huge(void)
{
	ctui_logview v;
	ctui_logview_init(&v, 3);
	char *big = malloc(100000);
	memset(big, 'a', 99999);
	big[99999] = 0;
	for (int i = 0; i < 200; i += 7)
		big[i] = ' ';
	CHECK(ctui_logview_push(&v, 0, CTUI_LOG_INFO, "s", big));
	CHECK(strlen(ctui_logview_at(&v, 0)->message) == CTUI_LOG_MESSAGE_MAX);
	free(big);
	CHECK(ctui_logview_row_count(&v, ctui_logview_at(&v, 0), W) == 1);
	v.wrap = true;
	size_t rows = ctui_logview_row_count(&v, ctui_logview_at(&v, 0), W);
	CHECK(rows > 50 && rows < CTUI_LOG_MESSAGE_MAX);
	/* a record taller than the window is still reachable */
	CHECK(ctui_logview_last_top(&v, W, H) == 0);
	screen_open(24, 80);
	screen_clear();
	ctui_logview_draw(&v, 0, 0, W, H);
	ctui_logview_free(&v);
}

static void test_levels_and_filter(void)
{
	ctui_logview v;
	ctui_logview_init(&v, 20);
	fill(&v, 10); /* levels cycle trace..error */
	CHECK(ctui_logview_visible(&v) == 10);
	ctui_logview_set_level(&v, CTUI_LOG_TRACE, false);
	CHECK(ctui_logview_visible(&v) == 8 &&
	      !ctui_logview_level_on(&v, CTUI_LOG_TRACE));
	CHECK(ctui_logview_visible_at(&v, 0)->level == CTUI_LOG_DEBUG);
	CHECK(ctui_logview_key(&v, 'e', W, H) &&
	      !ctui_logview_level_on(&v, CTUI_LOG_ERROR));
	CHECK(ctui_logview_visible(&v) == 6);
	ctui_logview_key(&v, 't', W, H);
	ctui_logview_key(&v, 'e', W, H);
	CHECK(ctui_logview_visible(&v) == 10);
	ctui_logview_set_filter(&v, "MESSAGE 7");
	CHECK(ctui_logview_visible(&v) == 1 &&
	      ctui_logview_visible_at(&v, 0)->seq == 7);
	ctui_logview_set_filter(&v, "SRC"); /* matches the source as well */
	CHECK(ctui_logview_visible(&v) == 10);
	ctui_logview_set_filter(&v, "nothing");
	CHECK(ctui_logview_visible(&v) == 0);
	ctui_logview_set_filter(&v, NULL);
	CHECK(ctui_logview_visible(&v) == 10);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	ctui_logview_set_level(&v, (ctui_log_level)42, false); /* ignored */
	CHECK(ctui_logview_visible(&v) == 10);
	CHECK_STR(ctui_log_level_name(CTUI_LOG_WARN), "WARN");
	CHECK(ctui_log_level_pair(CTUI_LOG_ERROR) !=
	      ctui_log_level_pair(CTUI_LOG_INFO));
	ctui_logview_free(&v);
}

static void test_find(void)
{
	CHECK(ctui_log_find("Hello World", "world", 0) == 6);
	CHECK(ctui_log_find("Hello World", "o", 5) == 7);
	CHECK(ctui_log_find("Hello", "", 0) == SIZE_MAX);
	CHECK(ctui_log_find("Hi", "hello", 0) == SIZE_MAX);
	CHECK(ctui_log_find("caf\xc3\xa9", "caf\xc3\xa9", 0) == 0);
	CHECK(ctui_log_find("caf\xc3\xa9", "\xa9", 0) ==
	      SIZE_MAX); /* not inside a character */
	CHECK(ctui_log_find("caf\xc3\xa9", "caf\xc3", 0) == SIZE_MAX);
	char t[16];
	ctui_log_format_time(3661, t, sizeof t);
	CHECK_STR(t, "01:01:01");
	ctui_log_format_time(86400 + 59, t, sizeof t);
	CHECK_STR(t, "00:00:59");
	ctui_log_format_time(-5, t, sizeof t);
	CHECK_STR(t, "00:00:00");
}

static void test_follow(void)
{
	ctui_logview v;
	ctui_logview_init(&v, 100);
	fill(&v, 3);
	CHECK(v.follow && ctui_logview_top(&v, W, H) == 0 &&
	      ctui_logview_last_top(&v, W, H) == 0);
	fill(&v, 17); /* 20 records, 5 rows */
	CHECK(ctui_logview_top(&v, W, H) == 15);
	CHECK(!ctui_logview_scroll(&v, W, H, 3) &&
	      v.follow); /* down: still following */
	CHECK(ctui_logview_scroll(&v, W, H, -2) && !v.follow);
	CHECK(ctui_logview_top(&v, W, H) == 13);
	fill(&v, 5); /* the window stays on its record */
	CHECK(ctui_logview_top(&v, W, H) == 13);
	CHECK(ctui_logview_scroll(&v, W, H, 100) &&
	      ctui_logview_top(&v, W, H) == 20);
	CHECK(!v.follow); /* reaching the end by scrolling does not follow */
	CHECK(ctui_logview_key(&v, KEY_END, W, H) && v.follow &&
	      ctui_logview_top(&v, W, H) == 20);
	ctui_logview_key(&v, KEY_PPAGE, W, H);
	CHECK(!v.follow && ctui_logview_top(&v, W, H) == 16);
	ctui_logview_key(&v, 'f', W, H);
	CHECK(v.follow);
	ctui_logview_key(&v, 'f', W, H);
	CHECK(!v.follow && ctui_logview_top(&v, W, H) == 20);
	ctui_logview_key(&v, KEY_HOME, W, H);
	CHECK(ctui_logview_top(&v, W, H) == 0);
	ctui_logview_key(&v, KEY_NPAGE, W, H);
	CHECK(ctui_logview_top(&v, W, H) == 4);
	ctui_logview_key(&v, 'j', W, H);
	ctui_logview_key(&v, 'k', W, H);
	ctui_logview_key(&v, KEY_DOWN, W, H);
	ctui_logview_key(&v, KEY_UP, W, H);
	CHECK(ctui_logview_top(&v, W, H) == 4);
	/* the anchor survives old records being dropped */
	ctui_logview_free(&v);
	ctui_logview_init(&v, 10);
	fill(&v, 10);
	ctui_logview_scroll(&v, W, H, -100);
	ctui_logview_scroll(&v, W, H, 3);
	CHECK(!v.follow && ctui_logview_top(&v, W, H) == 3);
	fill(&v, 2);
	CHECK(ctui_logview_top(&v, W, H) == 1);
	fill(&v, 20); /* the anchor is gone: the oldest record is shown */
	CHECK(ctui_logview_top(&v, W, H) == 0);
	/* nothing to scroll: follow stays on */
	ctui_logview_free(&v);
	ctui_logview_init(&v, 10);
	fill(&v, 2);
	CHECK(!ctui_logview_scroll(&v, W, H, -1) && v.follow);
	ctui_logview_free(&v);
}

static void test_wrap_heights(void)
{
	ctui_logview v;
	ctui_logview_init(&v, 10);
	ctui_logview_push(&v, 0, CTUI_LOG_INFO, "s", "short");
	ctui_logview_push(
	    &v, 0, CTUI_LOG_INFO, "s",
	    "aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm");
	ctui_logview_layout l = ctui_logview_layout_for(&v, 40);
	CHECK(l.gutter == 0 && l.prefix == 15 && l.body == 39 && l.text == 24);
	CHECK(ctui_logview_row_count(&v, ctui_logview_at(&v, 1), 40) == 1);
	v.wrap = true;
	CHECK(ctui_logview_row_count(&v, ctui_logview_at(&v, 0), 40) == 1);
	CHECK(ctui_logview_row_count(&v, ctui_logview_at(&v, 1), 40) ==
	      3); /* 64 chars in 24 columns */
	CHECK(ctui_logview_row_count(&v, ctui_logview_at(&v, 1), 100) == 1);
	v.gutter = CTUI_LOG_GUTTER_TIME;
	l = ctui_logview_layout_for(&v, 40);
	CHECK(l.gutter == 9 && l.text == 15);
	v.gutter = CTUI_LOG_GUTTER_NUMBER;
	l = ctui_logview_layout_for(&v, 40);
	CHECK(l.gutter == 2 && l.text == 22);
	/* very narrow: one column of text, no crash */
	CHECK(ctui_logview_layout_for(&v, 3).text == 1);
	CHECK(ctui_logview_row_count(&v, ctui_logview_at(&v, 0), 3) == 5);
	/* a tall record limits the last page */
	v.gutter = CTUI_LOG_GUTTER_NONE;
	CHECK(ctui_logview_last_top(&v, 40, 3) ==
	      1); /* only the 3-row record fits */
	CHECK(ctui_logview_last_top(&v, 40, 4) == 0); /* 1 + 3 rows */
	ctui_logview_key(&v, 'r', 40, 4);
	CHECK(!v.wrap);
	ctui_logview_key(&v, 'g', 40, 4);
	CHECK(v.gutter == CTUI_LOG_GUTTER_NUMBER);
	ctui_logview_free(&v);
}

static void test_hit_and_select(void)
{
	screen_open(24, 80);
	ctui_logview v;
	ctui_logview_init(&v, 20);
	ctui_logview_push(&v, 0, CTUI_LOG_INFO, "s", "one");
	ctui_logview_push(
	    &v, 0, CTUI_LOG_INFO, "s",
	    "aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm");
	ctui_logview_push(&v, 0, CTUI_LOG_INFO, "s", "three");
	v.wrap = true;
	size_t i;
	/* widget at (2, 4), 40 x 6: rows 2 | 3 4 5 | 6 */
	CHECK(ctui_logview_hit(&v, 2, 4, 40, 6, 2, 4, &i) && i == 0);
	CHECK(ctui_logview_hit(&v, 2, 4, 40, 6, 3, 10, &i) && i == 1);
	CHECK(ctui_logview_hit(&v, 2, 4, 40, 6, 5, 42, &i) && i == 1);
	CHECK(ctui_logview_hit(&v, 2, 4, 40, 6, 6, 4, &i) && i == 2);
	CHECK(!ctui_logview_hit(&v, 2, 4, 40, 6, 7, 4,
				&i)); /* below the widget */
	CHECK(!ctui_logview_hit(&v, 2, 4, 40, 6, 1, 4, &i)); /* above */
	CHECK(!ctui_logview_hit(&v, 2, 4, 40, 6, 3, 3, &i)); /* left */
	CHECK(!ctui_logview_hit(&v, 2, 4, 40, 6, 3, 43,
				&i)); /* the scrollbar column */
	CHECK(!ctui_logview_hit(&v, 2, 4, 40, 4, 6, 4, &i)); /* clipped away */
	screen_clear();
	ctui_logview_draw(&v, 2, 4, 40, 6);
	CHECK(ctui_logview_selected(&v) == NULL);
	CHECK(ctui_logview_press(&v, 4, 10) &&
	      ctui_logview_selected(&v) == ctui_logview_at(&v, 1));
	CHECK(!ctui_logview_press(&v, 20, 10));
	ctui_logview_select(&v, NULL);
	CHECK(ctui_logview_selected(&v) == NULL);
	/* the selection follows the record and ends with it */
	ctui_logview_select(&v, ctui_logview_at(&v, 0));
	fill(&v, 20);
	CHECK(ctui_logview_selected(&v) == NULL);
	ctui_logview_free(&v);
}

static void test_pointer(void)
{
	screen_open(24, 80);
	ctui_logview v;
	ctui_logview_init(&v, 100);
	fill(&v, 50);
	CHECK(!ctui_logview_wheel(&v, -3)); /* never drawn */
	screen_clear();
	ctui_logview_draw(&v, 0, 0, W, H);
	CHECK(v.bar.total == 50 && ctui_scrollbar_needed(&v.bar));
	CHECK(ctui_logview_top(&v, W, H) == 45);
	CHECK(ctui_logview_wheel(&v, -3) && !v.follow &&
	      ctui_logview_top(&v, W, H) == 42);
	ctui_logview_draw(&v, 0, 0, W, H);
	CHECK(v.bar.offset == 42);
	/* press the track at its top: a page up; drag the thumb to the top */
	CHECK(ctui_logview_press(&v, 0, W - 1));
	CHECK(ctui_logview_top(&v, W, H) < 42);
	ctui_logview_release(&v);
	ctui_logview_free(&v);
	/* drag from the bottom to the top */
	ctui_logview_init(&v, 100);
	fill(&v, 50);
	ctui_logview_draw(&v, 0, 0, W, H);
	CHECK(
	    ctui_logview_press(&v, H - 1, W - 1)); /* thumb is at the bottom */
	CHECK(v.bar.dragging);
	CHECK(ctui_logview_drag(&v, 0));
	CHECK(!v.follow && ctui_logview_top(&v, W, H) == 0);
	CHECK(ctui_logview_release(&v) && !ctui_logview_drag(&v, 3));
	ctui_logview_free(&v);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_logview v;
	ctui_logview_init(&v, 100);
	ctui_logview_push(&v, 3661, CTUI_LOG_ERROR, "net",
			  "connection LOST to host");
	ctui_logview_push(&v, 3662, CTUI_LOG_WARN, "disk",
			  "a very long message that does not fit in the "
			  "columns that are left for it");
	screen_clear();
	ctui_logview_draw(&v, 0, 0, 50, 4);
	CHECK(screen_has_text("ERROR") && screen_has_text("net") &&
	      screen_has_text("connection LOST"));
	CHECK(screen_has_color(
	    ctui_attr(ctui_log_level_pair(CTUI_LOG_ERROR), true)));
	CHECK(screen_has_color(
	    ctui_attr(ctui_log_level_pair(CTUI_LOG_WARN), false)));
	CHECK(!screen_has_text("columns that are left")); /* cut */
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_PLAYHEAD, false)));
	ctui_logview_set_filter(&v, "lost");
	screen_clear();
	ctui_logview_draw(&v, 0, 0, 50, 4);
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_PLAYHEAD, false))); /* the match */
	CHECK(!screen_has_text("WARN"));
	ctui_logview_set_filter(&v, NULL);
	v.gutter = CTUI_LOG_GUTTER_TIME;
	screen_clear();
	ctui_logview_draw(&v, 0, 0, 50, 4);
	CHECK(screen_has_text("01:01:01") && screen_has_text("01:01:02"));
	v.gutter = CTUI_LOG_GUTTER_NONE;
	v.wrap = true;
	screen_clear();
	ctui_logview_draw(&v, 0, 0, 50, 4);
	CHECK(screen_has_text("columns"));
	ctui_logview_select(&v, ctui_logview_at(&v, 0));
	screen_clear();
	ctui_logview_draw(&v, 0, 0, 50, 4);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, false)));
	/* tiny widgets draw nothing and do not crash */
	ctui_logview_draw(&v, 0, 0, 1, 4);
	ctui_logview_draw(&v, 0, 0, 5, 1);
	ctui_logview_free(&v);
}

int main(void)
{
	RUN(test_ring);
	RUN(test_zero_capacity);
	RUN(test_sanitize);
	RUN(test_huge);
	RUN(test_levels_and_filter);
	RUN(test_find);
	RUN(test_follow);
	RUN(test_wrap_heights);
	RUN(test_hit_and_select);
	RUN(test_pointer);
	RUN(test_draw);
	return CHECK_DONE();
}
