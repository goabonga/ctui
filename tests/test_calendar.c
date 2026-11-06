/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/calendar.h>
#include <ctui/theme.h>

static void test_dates(void)
{
	CHECK(ctui_date_leap(2024) && !ctui_date_leap(2023));
	CHECK(!ctui_date_leap(1900) && ctui_date_leap(2000));
	CHECK(ctui_date_days_in_month(2023, 2) == 28 &&
	      ctui_date_days_in_month(2024, 2) == 29);
	CHECK(ctui_date_days_in_month(2026, 11) == 30 &&
	      ctui_date_days_in_month(2026, 12) == 31);
	CHECK(ctui_date_days_in_month(2026, 0) == 0 &&
	      ctui_date_days_in_month(2026, 13) == 0);

	CHECK(ctui_date_weekday(1970, 1, 1) == 3);  /* Thursday */
	CHECK(ctui_date_weekday(2026, 11, 1) == 6); /* Sunday */
	CHECK(ctui_date_weekday(2000, 2, 29) == 1); /* Tuesday */
	CHECK(ctui_date_weekday(2024, 2, 30) == -1 &&
	      ctui_date_weekday(2024, 1, 0) == -1);

	CHECK(ctui_date_to_days(1970, 1, 1) == 0 &&
	      ctui_date_to_days(1970, 1, 2) == 1);
	CHECK(ctui_date_to_days(1969, 12, 31) == -1);
	CHECK(ctui_date_to_days(2000, 3, 1) - ctui_date_to_days(2000, 2, 28) ==
	      2);
	/* round trip over a long span, including 1900 and 2100 which are not
	 * leap */
	long start = ctui_date_to_days(1899, 12, 1),
	     end = ctui_date_to_days(2101, 1, 31);
	bool ok = true;
	for (long d = start; d <= end; d++) {
		ctui_date t = ctui_date_from_days(d);
		ok = ok && t.day >= 1 &&
		     t.day <= ctui_date_days_in_month(t.year, t.month) &&
		     ctui_date_to_days(t.year, t.month, t.day) == d;
	}
	CHECK(ok);
	ctui_date t = ctui_date_from_days(ctui_date_to_days(2024, 2, 29) + 1);
	CHECK(t.year == 2024 && t.month == 3 && t.day == 1);
}

static void test_cell_and_hit(void)
{
	int row, col;
	/* November 2026 starts on a Sunday */
	CHECK(ctui_calendar_cell(2026, 11, 0, 1, &row, &col) && row == 0 &&
	      col == 6);
	CHECK(ctui_calendar_cell(2026, 11, 0, 2, &row, &col) && row == 1 &&
	      col == 0);
	CHECK(ctui_calendar_cell(2026, 11, 0, 30, &row, &col) && row == 5 &&
	      col == 0);
	/* weeks starting on Sunday */
	CHECK(ctui_calendar_cell(2026, 11, 6, 1, &row, &col) && row == 0 &&
	      col == 0);
	CHECK(ctui_calendar_cell(2026, 11, 6, 30, &row, &col) && row == 4 &&
	      col == 1);
	CHECK(!ctui_calendar_cell(2026, 11, 0, 31, &row, &col) &&
	      !ctui_calendar_cell(2026, 11, 0, 0, NULL, NULL));
	/* a 31-day month starting on Sunday needs the sixth row (Monday start)
	 */
	CHECK(ctui_calendar_cell(2026, 8, 0, 31, &row, &col) && row == 5 &&
	      col == 0);

	CHECK(ctui_calendar_width() == 20 && ctui_calendar_height() == 8);
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	int day = 0;
	CHECK(ctui_calendar_hit(&c, 0, 0, NULL) == CTUI_CALENDAR_PREV);
	CHECK(ctui_calendar_hit(&c, 0, 2, NULL) == CTUI_CALENDAR_PREV);
	CHECK(ctui_calendar_hit(&c, 0, 3, NULL) == CTUI_CALENDAR_TITLE);
	CHECK(ctui_calendar_hit(&c, 0, 19, NULL) == CTUI_CALENDAR_NEXT);
	CHECK(ctui_calendar_hit(&c, 1, 5, NULL) == CTUI_CALENDAR_WEEKDAYS);
	/* day 1 is a Sunday: row 0, column 6 -> x 18..19 */
	CHECK(ctui_calendar_hit(&c, 2, 18, &day) == CTUI_CALENDAR_DAY &&
	      day == 1);
	CHECK(ctui_calendar_hit(&c, 2, 19, &day) == CTUI_CALENDAR_DAY &&
	      day == 1);
	CHECK(ctui_calendar_hit(&c, 2, 0, NULL) ==
	      CTUI_CALENDAR_OUTSIDE); /* before the 1st */
	CHECK(ctui_calendar_hit(&c, 3, 0, &day) == CTUI_CALENDAR_DAY &&
	      day == 2);
	CHECK(ctui_calendar_hit(&c, 3, 2, NULL) ==
	      CTUI_CALENDAR_OUTSIDE); /* gap */
	CHECK(ctui_calendar_hit(&c, 7, 0, &day) == CTUI_CALENDAR_DAY &&
	      day == 30);
	CHECK(ctui_calendar_hit(&c, 7, 3, NULL) ==
	      CTUI_CALENDAR_OUTSIDE); /* after the 30th */
	CHECK(ctui_calendar_hit(&c, 8, 0, NULL) == CTUI_CALENDAR_OUTSIDE);
	CHECK(ctui_calendar_hit(&c, 3, 20, NULL) == CTUI_CALENDAR_OUTSIDE);
	CHECK(ctui_calendar_hit(&c, -1, 0, NULL) == CTUI_CALENDAR_OUTSIDE);
	c.week_start = 6;
	CHECK(ctui_calendar_hit(&c, 2, 0, &day) == CTUI_CALENDAR_DAY &&
	      day == 1);
}

static void test_init_and_show(void)
{
	ctui_calendar c;
	ctui_calendar_init(&c, 2024, 2, 31);
	CHECK(c.year == 2024 && c.month == 2 && c.day == 29 &&
	      c.week_start == 0 && c.marked == 0);
	ctui_calendar_init(&c, 2024, 13, 5);
	CHECK(c.year == 1970 && c.month == 1 && c.day == 1);
	ctui_calendar_init(&c, 2024, 1, 31);
	CHECK(ctui_calendar_show(&c, 2024, 2) && c.day == 29);
	CHECK(!ctui_calendar_show(&c, 2024, 2));
	CHECK(!ctui_calendar_show(&c, 2024, 0) && c.month == 2);
	CHECK(ctui_calendar_show(&c, 20000, 5) && c.year == 9999 &&
	      c.month == 5);
	CHECK(ctui_calendar_show(&c, -4, 5) && c.year == 1);
	ctui_calendar_set_today(&c, 2026, 11, 1);
	CHECK(c.today.year == 2026 && c.today.month == 11 && c.today.day == 1);
	ctui_calendar_set_today(&c, 2026, 2, 30);
	CHECK(c.today.year == 0);
}

static void test_keys(void)
{
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	CHECK(ctui_calendar_key(&c, KEY_RIGHT) == CTUI_CALENDAR_MOVED &&
	      c.day == 16);
	CHECK(ctui_calendar_key(&c, 'h') == CTUI_CALENDAR_MOVED && c.day == 15);
	CHECK(ctui_calendar_key(&c, KEY_DOWN) == CTUI_CALENDAR_MOVED &&
	      c.day == 22);
	CHECK(ctui_calendar_key(&c, 'k') == CTUI_CALENDAR_MOVED && c.day == 15);
	/* crossing a month boundary changes the month */
	CHECK(ctui_calendar_key(&c, KEY_DOWN) == CTUI_CALENDAR_MOVED &&
	      c.day == 22);
	CHECK(ctui_calendar_key(&c, KEY_DOWN) == CTUI_CALENDAR_MOVED &&
	      c.day == 29);
	CHECK(ctui_calendar_key(&c, KEY_DOWN) == CTUI_CALENDAR_MONTH &&
	      c.month == 12 && c.day == 6);
	CHECK(ctui_calendar_key(&c, KEY_UP) == CTUI_CALENDAR_MONTH &&
	      c.month == 11 && c.day == 29);
	CHECK(ctui_calendar_goto(&c, 2026, 12, 29) == CTUI_CALENDAR_MONTH);
	CHECK(ctui_calendar_key(&c, KEY_RIGHT) == CTUI_CALENDAR_MOVED &&
	      ctui_calendar_key(&c, 'l') == CTUI_CALENDAR_MOVED);
	CHECK(c.day == 31);
	CHECK(ctui_calendar_key(&c, KEY_RIGHT) == CTUI_CALENDAR_MONTH &&
	      c.year == 2027 && c.month == 1 && c.day == 1);
	CHECK(ctui_calendar_key(&c, KEY_LEFT) == CTUI_CALENDAR_MONTH &&
	      c.year == 2026 && c.month == 12 && c.day == 31);
	/* months keep the day when they can */
	CHECK(ctui_calendar_key(&c, KEY_PPAGE) == CTUI_CALENDAR_MONTH &&
	      c.month == 11 && c.day == 30);
	CHECK(ctui_calendar_key(&c, KEY_PPAGE) == CTUI_CALENDAR_MONTH &&
	      c.month == 10 && c.day == 30);
	ctui_calendar_init(&c, 2024, 1, 31);
	CHECK(ctui_calendar_key(&c, KEY_NPAGE) == CTUI_CALENDAR_MONTH &&
	      c.month == 2 && c.day == 29);
	/* Home needs today */
	CHECK(ctui_calendar_key(&c, KEY_HOME) == CTUI_CALENDAR_NONE);
	ctui_calendar_set_today(&c, 2026, 11, 1);
	CHECK(ctui_calendar_key(&c, KEY_HOME) == CTUI_CALENDAR_MONTH &&
	      c.year == 2026 && c.month == 11 && c.day == 1);
	CHECK(ctui_calendar_key(&c, KEY_HOME) == CTUI_CALENDAR_NONE);
	CHECK(ctui_calendar_key(&c, '\n') == CTUI_CALENDAR_ACTIVATED);
	CHECK(ctui_calendar_key(&c, KEY_ENTER) == CTUI_CALENDAR_ACTIVATED);
	CHECK(ctui_calendar_key(&c, 'x') == CTUI_CALENDAR_NONE);
	/* the limits of the supported years */
	ctui_calendar_init(&c, 1, 1, 1);
	CHECK(ctui_calendar_key(&c, KEY_LEFT) == CTUI_CALENDAR_NONE &&
	      ctui_calendar_key(&c, KEY_PPAGE) == CTUI_CALENDAR_NONE);
	CHECK(c.year == 1 && c.month == 1 && c.day == 1);
	ctui_calendar_init(&c, 9999, 12, 31);
	CHECK(ctui_calendar_key(&c, KEY_RIGHT) == CTUI_CALENDAR_NONE &&
	      ctui_calendar_key(&c, KEY_NPAGE) == CTUI_CALENDAR_NONE);
	CHECK(c.year == 9999 && c.month == 12 && c.day == 31);
}

static void test_goto_and_mouse(void)
{
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	CHECK(ctui_calendar_goto(&c, 2026, 11, 15) == CTUI_CALENDAR_NONE);
	CHECK(ctui_calendar_goto(&c, 2026, 11, 31) == CTUI_CALENDAR_NONE &&
	      c.day == 15);
	CHECK(ctui_calendar_goto(&c, 2027, 2, 3) == CTUI_CALENDAR_MONTH &&
	      c.year == 2027 && c.day == 3);

	ctui_calendar_init(&c, 2026, 11, 15);
	CHECK(ctui_calendar_press(&c, 3, 0) == CTUI_CALENDAR_MOVED &&
	      c.day == 2); /* Monday the 2nd */
	CHECK(ctui_calendar_press(&c, 3, 0) == CTUI_CALENDAR_NONE);
	CHECK(ctui_calendar_press(&c, 2, 0) == CTUI_CALENDAR_NONE &&
	      c.day == 2);
	CHECK(ctui_calendar_press(&c, 1, 3) == CTUI_CALENDAR_NONE);
	CHECK(ctui_calendar_press(&c, 0, 19) == CTUI_CALENDAR_MONTH &&
	      c.month == 12 && c.day == 2);
	CHECK(ctui_calendar_press(&c, 0, 0) == CTUI_CALENDAR_MONTH &&
	      c.month == 11);
	CHECK(ctui_calendar_press(&c, 0, 0) == CTUI_CALENDAR_MONTH &&
	      ctui_calendar_press(&c, 0, 0) == CTUI_CALENDAR_MONTH);
	CHECK(c.month == 9);
	CHECK(ctui_calendar_wheel(&c, -1) == CTUI_CALENDAR_MONTH &&
	      c.month == 8);
	CHECK(ctui_calendar_wheel(&c, 12) == CTUI_CALENDAR_MONTH &&
	      c.year == 2027 && c.month == 8);
	CHECK(ctui_calendar_wheel(&c, 0) == CTUI_CALENDAR_NONE);
	/* the 31st clamps when the month is shorter */
	ctui_calendar_init(&c, 2026, 10, 31);
	CHECK(ctui_calendar_press(&c, 0, 19) == CTUI_CALENDAR_MONTH &&
	      c.month == 11 && c.day == 30);
}

static void test_draw(void)
{
	screen_open(24, 80);
	screen_clear();
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	ctui_calendar_set_today(&c, 2026, 11, 1);
	c.marked = 1U << 9U; /* the 10th */
	ctui_calendar_draw(&c, 1, 2);
	CHECK(screen_has_text("November 2026"));
	CHECK(screen_has_text("Mo Tu We Th Fr Sa Su"));
	CHECK(screen_has_text(" 2  3  4"));
	CHECK(screen_has_text("30"));
	CHECK(!screen_has_text("31"));

	char row[64] = {0};
	mvinnstr(1, 2, row, 20);
	CHECK(row[0] == '<' && row[19] == '>');
	/* today is underlined, the cursor and the marks use their own colours
	 */
	CHECK((mvinch(1 + 2, 2 + 18) & A_UNDERLINE) != 0);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, true)));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_CURSOR, true)));

	/* names and week start are per instance */
	static const char *const MONTHS[12] = {
	    "Janvier",	 "Fevrier", "Mars",	"Avril",
	    "Mai",	 "Juin",    "Juillet",	"Aout",
	    "Septembre", "Octobre", "Novembre", "Decembre"};
	static const char *const DAYS[7] = {"Lundi",   "Mardi",	   "Mercredi",
					    "Jeudi",   "Vendredi", "Samedi",
					    "Dimanche"};
	c.month_names = MONTHS;
	c.weekday_names = DAYS;
	c.week_start = 6;
	screen_clear();
	ctui_calendar_draw(&c, 0, 0);
	CHECK(screen_has_text("Novembre 2026"));
	CHECK(screen_has_text("Di Lu Ma Me Je Ve Sa"));

	/* a style recolours only this instance */
	ctui_style st = {0};
	st.attr[CTUI_PAIR_SELECTION] = ctui_attr(CTUI_PAIR_ERROR, true);
	c.style = &st;
	screen_clear();
	ctui_calendar_draw(&c, 0, 0);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_ERROR, true)));
	endwin();
}

/* Screen position of a day of November 2026 (the 1st is a Sunday). */
static void nov(int d, int *y, int *x)
{
	int row, col;
	ctui_calendar_cell(2026, 11, 0, d, &row, &col);
	*y = 2 + row;
	*x = col * 3;
}

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

static void test_select_keys(void)
{
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 5);
	CHECK(ctui_calendar_key(&c, KEY_SRIGHT) == CTUI_CALENDAR_SELECTED &&
	      c.day == 6);
	CHECK(ctui_cal_select_count(&c.sel) == 2 &&
	      ctui_cal_select_contains(&c.sel, day(2026, 11, 5)));
	CHECK(ctui_calendar_key(&c, 'L') == CTUI_CALENDAR_SELECTED &&
	      c.day == 7 && c.sel.count == 3);
	CHECK(ctui_calendar_key(&c, KEY_SLEFT) == CTUI_CALENDAR_SELECTED &&
	      c.day == 6 && c.sel.count == 2);
	CHECK(ctui_calendar_key(&c, 'H') == CTUI_CALENDAR_SELECTED &&
	      c.day == 5 && c.sel.count == 1);
	CHECK(ctui_calendar_key(&c, 'H') == CTUI_CALENDAR_SELECTED &&
	      c.day == 4 && c.sel.count == 2);
	/* by weeks */
	CHECK(ctui_calendar_key(&c, KEY_SF) == CTUI_CALENDAR_SELECTED &&
	      c.day == 11);
	CHECK(c.sel.count == 7 &&
	      ctui_cal_select_first(&c.sel) == day(2026, 11, 5));
	CHECK(ctui_calendar_key(&c, 'K') == CTUI_CALENDAR_SELECTED &&
	      c.day == 4 && c.sel.count == 2);
	CHECK(ctui_calendar_key(&c, KEY_SR) == CTUI_CALENDAR_MONTH &&
	      c.month == 10 && c.day == 28);
	CHECK(c.sel.count == 9);
	CHECK(ctui_calendar_key(&c, 27) == CTUI_CALENDAR_SELECTED &&
	      c.sel.count == 0);
	CHECK(ctui_calendar_key(&c, 27) == CTUI_CALENDAR_NONE);

	/* a plain move keeps the days and forgets the anchor; Space makes holes
	 */
	ctui_calendar_init(&c, 2026, 11, 2);
	CHECK(ctui_calendar_key(&c, ' ') == CTUI_CALENDAR_SELECTED &&
	      c.sel.count == 1);
	CHECK(ctui_calendar_key(&c, KEY_RIGHT) == CTUI_CALENDAR_MOVED &&
	      c.sel.count == 1 && !c.sel.has_anchor);
	CHECK(ctui_calendar_key(&c, KEY_RIGHT) == CTUI_CALENDAR_MOVED);
	CHECK(ctui_calendar_key(&c, ' ') == CTUI_CALENDAR_SELECTED);
	CHECK(c.sel.count == 2 &&
	      ctui_cal_select_contains(&c.sel, day(2026, 11, 2)) &&
	      ctui_cal_select_contains(&c.sel, day(2026, 11, 4)) &&
	      !ctui_cal_select_contiguous(&c.sel));
	CHECK(ctui_calendar_key(&c, ' ') == CTUI_CALENDAR_SELECTED &&
	      c.sel.count == 1); /* toggled off */
	CHECK(ctui_calendar_selected_mask(&c) == 1U << 1U);

	/* across a month end: the month change wins, the range goes on */
	ctui_calendar_init(&c, 2026, 12, 30);
	CHECK(ctui_calendar_key(&c, KEY_SRIGHT) == CTUI_CALENDAR_SELECTED);
	CHECK(ctui_calendar_key(&c, KEY_SRIGHT) == CTUI_CALENDAR_MONTH &&
	      c.year == 2027 && c.month == 1 && c.day == 1);
	CHECK(c.sel.count == 3 &&
	      ctui_cal_select_first(&c.sel) == day(2026, 12, 30) &&
	      ctui_cal_select_last(&c.sel) == day(2027, 1, 1));
	CHECK(ctui_calendar_selected_mask(&c) == 1U);
	CHECK(ctui_calendar_key(&c, KEY_PPAGE) == CTUI_CALENDAR_MONTH &&
	      ctui_calendar_selected_mask(&c) == 3U << 29U);

	/* refused when the range would be too long, or beyond year 9999 */
	ctui_calendar_init(&c, 2026, 1, 1);
	for (int i = 0; i < CTUI_CAL_SELECT_MAX - 1; i++)
		ctui_calendar_key(&c, 'L');
	CHECK(c.sel.count == CTUI_CAL_SELECT_MAX);
	long before = day(c.year, c.month, c.day);
	CHECK(ctui_calendar_key(&c, 'L') == CTUI_CALENDAR_NONE &&
	      day(c.year, c.month, c.day) == before);
	ctui_calendar_init(&c, 9999, 12, 31);
	CHECK(ctui_calendar_key(&c, 'L') == CTUI_CALENDAR_NONE &&
	      c.sel.count == 0);
	ctui_calendar_init(&c, 1, 1, 1);
	CHECK(ctui_calendar_key(&c, KEY_SLEFT) == CTUI_CALENDAR_NONE &&
	      c.sel.count == 0);
}

static void test_select_mouse(void)
{
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	int y, x, y2, x2;
	nov(3, &y, &x);
	nov(5, &y2, &x2);
	/* press then drag over days selects the range, release ends it */
	CHECK(ctui_calendar_press(&c, y, x) == CTUI_CALENDAR_MOVED &&
	      c.day == 3 && c.sel.count == 0);
	CHECK(ctui_calendar_drag(&c, y2, x2) == CTUI_CALENDAR_SELECTED &&
	      c.day == 5 && c.sel.count == 3);
	CHECK(ctui_calendar_drag(&c, y2, x2) == CTUI_CALENDAR_NONE);
	CHECK(ctui_calendar_drag(&c, 1, 3) == CTUI_CALENDAR_NONE &&
	      c.sel.count == 3); /* not a day */
	CHECK(ctui_calendar_release(&c) == CTUI_CALENDAR_NONE &&
	      !c.sel.dragging);
	CHECK(ctui_calendar_drag(&c, y, x) == CTUI_CALENDAR_NONE &&
	      c.sel.count == 3); /* no drag any more */
	/* a plain click clears the selection and moves the cursor */
	CHECK(ctui_calendar_press(&c, y, x) == CTUI_CALENDAR_MOVED &&
	      c.day == 3 && c.sel.count == 0);
	ctui_calendar_release(&c);
	nov(5, &y, &x);
	CHECK(ctui_calendar_press(&c, y, x) == CTUI_CALENDAR_MOVED);
	ctui_calendar_release(&c);
	/* clicking the day the cursor is on still clears */
	ctui_cal_select_toggle(&c.sel, day(2026, 11, 9));
	CHECK(ctui_calendar_press(&c, y, x) == CTUI_CALENDAR_SELECTED &&
	      c.sel.count == 0);
	ctui_calendar_release(&c);
	/* the toggle (right button in the demo) makes a discontiguous selection
	 */
	nov(9, &y, &x);
	CHECK(ctui_calendar_toggle_at(&c, y, x) == CTUI_CALENDAR_SELECTED &&
	      c.day == 9 && c.sel.count == 1);
	nov(12, &y, &x);
	CHECK(ctui_calendar_toggle_at(&c, y, x) == CTUI_CALENDAR_SELECTED &&
	      c.sel.count == 2);
	CHECK(ctui_calendar_toggle_at(&c, y, x) == CTUI_CALENDAR_SELECTED &&
	      c.sel.count == 1);
	CHECK(ctui_calendar_toggle_at(&c, 1, 3) == CTUI_CALENDAR_NONE &&
	      c.sel.count == 1);
	CHECK(ctui_calendar_selected_mask(&c) == 1U << 8U);
	/* the month arrows keep the selection */
	CHECK(ctui_calendar_press(&c, 0, 19) == CTUI_CALENDAR_MONTH &&
	      c.sel.count == 1);
}

static void test_spanned_draw(void)
{
	screen_open(24, 80);
	screen_clear();
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	ctui_calendar_draw(&c, 0, 0);
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_YELLOW, true)));
	c.spanned = 7U << 2U; /* the 3rd to the 5th */
	c.marked = 1U << 3U;  /* the 4th is marked as well: the mark wins */
	c.sel.count = 0;
	screen_clear();
	ctui_calendar_draw(&c, 0, 0);
	int y, x;
	nov(3, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_GAUGE_YELLOW, true)));
	nov(4, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR, true)));
	nov(5, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_GAUGE_YELLOW, true)));
	nov(6, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) !=
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_GAUGE_YELLOW, true)));
	/* the cursor and the selection come first */
	c.day = 3;
	ctui_cal_select_add(&c.sel, day(2026, 11, 5));
	screen_clear();
	ctui_calendar_draw(&c, 0, 0);
	nov(3, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_SELECTION, true)));
	nov(5, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));
	endwin();
}

static void test_select_draw(void)
{
	screen_open(24, 80);
	screen_clear();
	ctui_calendar c;
	ctui_calendar_init(&c, 2026, 11, 15);
	ctui_calendar_draw(&c, 0, 0);
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));
	ctui_cal_select_set_range(&c.sel, day(2026, 11, 3),
				  day(2026, 11, 5)); /* Tue to Thu, one row */
	screen_clear();
	ctui_calendar_draw(&c, 0, 0);
	int y, x;
	nov(4, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));
	/* the gap between two selected days of a row is joined */
	CHECK(PAIR_NUMBER(mvinch(y, x + 2) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR_SELECTED, false)));
	nov(5, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x + 2) & A_COLOR) !=
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR_SELECTED, false)));
	/* the cursor day keeps its own style inside the selection */
	c.day = 4;
	screen_clear();
	ctui_calendar_draw(&c, 0, 0);
	nov(4, &y, &x);
	CHECK(PAIR_NUMBER(mvinch(y, x) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_SELECTION, true)));
	endwin();
}

int main(void)
{
	RUN(test_dates);
	RUN(test_cell_and_hit);
	RUN(test_init_and_show);
	RUN(test_keys);
	RUN(test_goto_and_mouse);
	RUN(test_draw);
	RUN(test_select_keys);
	RUN(test_select_mouse);
	RUN(test_select_draw);
	RUN(test_spanned_draw);
	return CHECK_DONE();
}
