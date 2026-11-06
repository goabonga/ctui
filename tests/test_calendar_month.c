/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * The full month grid: weeks, cells, hit testing in both layouts, the
 * fallback below the minimum size, and drawing.
 */
#include "check.h"
#include "screen.h"

#include <ctui/calendar_month.h>

static bool rect_eq(ctui_rect a, int y, int x, int h, int w)
{
	return a.y == y && a.x == x && a.height == h && a.width == w;
}

/* The weeks of every month of a range of years, against the cell of the last
 * day. */
static void test_weeks(void)
{
	int six = 0, five = 0;
	for (int ws = 0; ws < 7; ws++) {
		for (int y = 1969; y <= 2102; y++) {
			for (int m = 1; m <= 12; m++) {
				int weeks = ctui_calendar_weeks(y, m, ws);
				int row = -1;
				ctui_calendar_cell(
				    y, m, ws, ctui_date_days_in_month(y, m),
				    &row, NULL);
				if (weeks != 5 && weeks != 6)
					CHECK(weeks == 5 || weeks == 6);
				if (weeks < row + 1 || (weeks == 6 && row != 5))
					CHECK(weeks >= row + 1 &&
					      (weeks != 6 || row == 5));
				six += weeks == 6;
				five += weeks == 5;
			}
		}
	}
	CHECK(six > 0 && five > 0);
	/* February starting on the first day of the week: 28 days fit four
	 * rows, the grid keeps five */
	CHECK(ctui_calendar_weeks(2021, 2, 0) ==
	      5); /* Monday 2021-02-01, week from Monday */
	CHECK(ctui_calendar_weeks(2026, 2, 6) ==
	      5); /* Sunday 2026-02-01, week from Sunday */
	CHECK(ctui_calendar_weeks(2024, 2, 0) == 5); /* leap, Thursday */
	CHECK(ctui_calendar_weeks(2024, 2, 4) ==
	      5); /* leap, starts on the week start: 29 days */
	CHECK(ctui_calendar_weeks(2026, 2, 0) ==
	      5); /* Sunday, week from Monday */
	CHECK(ctui_calendar_weeks(2026, 11, 0) ==
	      6); /* Sunday the 1st, week from Monday: 6 rows */
	CHECK(ctui_calendar_weeks(2026, 11, 6) == 5); /* ... from Sunday: 5 */
	CHECK(ctui_calendar_weeks(2026, 8, 0) ==
	      6); /* Saturday the 1st, 31 days */
	CHECK(ctui_calendar_weeks(2026, 13, 0) == 5 &&
	      ctui_calendar_weeks(2026, 0, 0) == 5); /* invalid */
}

static void test_min(void)
{
	CHECK(ctui_calendar_full_min_height(5) == 12 &&
	      ctui_calendar_full_min_height(6) == 14);
	/* November 2026 from Monday is 6 rows, from Sunday 5 */
	CHECK(ctui_calendar_full_fits(2026, 11, 0, 40, 14));
	CHECK(!ctui_calendar_full_fits(2026, 11, 0, 40, 13));
	CHECK(!ctui_calendar_full_fits(2026, 11, 0, 39, 30));
	CHECK(ctui_calendar_full_fits(2026, 11, 6, 40, 12));
	CHECK(!ctui_calendar_full_fits(2026, 11, 6, 40, 11));
	CHECK(ctui_calendar_full_fits(2026, 2, 0, 200, 60));
	CHECK(!ctui_calendar_full_fits(2026, 2, 0, 0, 0));
}

/* Cells tile the area below the header, no gap, no overlap, sizes within one.
 */
static void test_cells_tile(void)
{
	for (int weeks = 5; weeks <= 6; weeks++) {
		for (int w = 40; w <= 130; w += 3) {
			for (int h = 2 + 2 * weeks; h <= 60; h += 5) {
				int minw = 1000, maxw = 0, minh = 1000,
				    maxh = 0, area = 0;
				for (int r = 0; r < weeks; r++)
					for (int c = 0; c < 7; c++) {
						ctui_rect a =
						    ctui_calendar_full_cell(
							w, h, weeks, r, c);
						area += a.width * a.height;
						minw = a.width < minw ? a.width
								      : minw;
						maxw = a.width > maxw ? a.width
								      : maxw;
						minh = a.height < minh
							   ? a.height
							   : minh;
						maxh = a.height > maxh
							   ? a.height
							   : maxh;
						if (c > 0) {
							ctui_rect l =
							    ctui_calendar_full_cell(
								w, h, weeks, r,
								c - 1);
							if (l.x + l.width !=
								a.x ||
							    l.y != a.y)
								CHECK(
								    l.x + l.width ==
									a.x &&
								    l.y == a.y);
						}
						if (r > 0) {
							ctui_rect u =
							    ctui_calendar_full_cell(
								w, h, weeks,
								r - 1, c);
							if (u.y + u.height !=
								a.y ||
							    u.x != a.x)
								CHECK(
								    u.y + u.height ==
									a.y &&
								    u.x == a.x);
						}
						if (r == 0 && a.y != 2)
							CHECK(a.y == 2);
						if (c == 0 && a.x != 0)
							CHECK(a.x == 0);
						if (r == weeks - 1 &&
						    a.y + a.height != h)
							CHECK(a.y + a.height ==
							      h);
						if (c == 6 &&
						    a.x + a.width != w)
							CHECK(a.x + a.width ==
							      w);
					}
				if (area != w * (h - 2) || maxw - minw > 1 ||
				    maxh - minh > 1 || minh < 2)
					CHECK(area == w * (h - 2) &&
					      maxw - minw <= 1 &&
					      maxh - minh <= 1 && minh >= 2);
			}
		}
	}
	/* known sizes */
	CHECK(rect_eq(ctui_calendar_full_cell(70, 32, 6, 0, 0), 2, 0, 5, 10));
	CHECK(rect_eq(ctui_calendar_full_cell(100, 30, 6, 0, 0), 2, 0, 4, 14));
	CHECK(
	    rect_eq(ctui_calendar_full_cell(100, 30, 6, 5, 6), 25, 85, 5, 15));
	CHECK(
	    rect_eq(ctui_calendar_full_cell(100, 30, 5, 4, 6), 24, 85, 6, 15));
	/* out of range */
	CHECK(rect_eq(ctui_calendar_full_cell(100, 30, 6, 6, 0), 0, 0, 0, 0));
	CHECK(rect_eq(ctui_calendar_full_cell(100, 30, 6, 0, 7), 0, 0, 0, 0));
	CHECK(rect_eq(ctui_calendar_full_cell(100, 30, 6, -1, 0), 0, 0, 0, 0));
	CHECK(rect_eq(ctui_calendar_full_cell(100, 2, 6, 0, 0), 0, 0, 0, 0));
	CHECK(rect_eq(ctui_calendar_full_cell(100, 30, 0, 0, 0), 0, 0, 0, 0));
}

static void test_header_parts(void)
{
	CHECK(rect_eq(ctui_calendar_full_arrow(100, false), 0, 0, 1, 3));
	CHECK(rect_eq(ctui_calendar_full_arrow(100, true), 0, 97, 1, 3));
	CHECK(rect_eq(ctui_calendar_full_title(100), 0, 3, 1, 94));
	int total = 0;
	for (int c = 0; c < 7; c++) {
		ctui_rect r = ctui_calendar_full_weekday(100, c);
		CHECK(r.y == 1 && r.height == 1);
		total += r.width;
		ctui_rect cell = ctui_calendar_full_cell(100, 30, 5, 0, c);
		CHECK(r.x == cell.x &&
		      r.width ==
			  cell.width); /* the name sits over its column */
	}
	CHECK(total == 100);
	CHECK(ctui_calendar_full_weekday(100, 7).width == 0);
}

/* Every pixel of the area, against the day whose cell holds it. */
static void test_hit_all_pixels(void)
{
	static const int SIZES[][2] = {
	    {40, 14}, {41, 15}, {80, 24}, {100, 30}, {133, 41}};
	static const int MONTHS[][2] = {
	    {2026, 11}, {2026, 2}, {2024, 2}, {2021, 2}, {2026, 8}};
	for (size_t s = 0; s < sizeof SIZES / sizeof SIZES[0]; s++) {
		for (size_t k = 0; k < sizeof MONTHS / sizeof MONTHS[0]; k++) {
			for (int ws = 0; ws < 7; ws += 6) {
				int y = MONTHS[k][0], m = MONTHS[k][1];
				int w = SIZES[s][0], h = SIZES[s][1];
				int weeks = ctui_calendar_weeks(y, m, ws),
				    dim = ctui_date_days_in_month(y, m);
				int hits[32] = {0};
				for (int py = -1; py <= h; py++) {
					for (int px = -1; px <= w; px++) {
						int d = 0;
						ctui_calendar_part p =
						    ctui_calendar_full_hit(
							y, m, ws, w, h, py, px,
							&d);
						if (py < 0 || py >= h ||
						    px < 0 || px >= w) {
							CHECK(
							    p ==
							    CTUI_CALENDAR_OUTSIDE);
						} else if (py == 0) {
							ctui_calendar_part want =
							    px < 3
								? CTUI_CALENDAR_PREV
							    : px >= w - 3
								? CTUI_CALENDAR_NEXT
								: CTUI_CALENDAR_TITLE;
							CHECK(p == want);
						} else if (py == 1) {
							CHECK(
							    p ==
							    CTUI_CALENDAR_WEEKDAYS);
						} else if (p ==
							   CTUI_CALENDAR_DAY) {
							hits[d]++;
							int row, col;
							ctui_calendar_cell(
							    y, m, ws, d, &row,
							    &col);
							ctui_rect c =
							    ctui_calendar_full_cell(
								w, h, weeks,
								row, col);
							if (!(py >= c.y &&
							      py <
								  c.y +
								      c.height &&
							      px >= c.x &&
							      px < c.x +
								       c.width))
								CHECK(
								    py >= c.y &&
								    py <
									c.y +
									    c.height &&
								    px >= c.x &&
								    px <
									c.x +
									    c.width);
						} else {
							CHECK(
							    p ==
							    CTUI_CALENDAR_OUTSIDE); /* a cell of the other months */
						}
					}
				}
				for (int d = 1; d <= dim; d++) {
					int row, col;
					ctui_calendar_cell(y, m, ws, d, &row,
							   &col);
					ctui_rect c = ctui_calendar_full_cell(
					    w, h, weeks, row, col);
					if (hits[d] != c.width * c.height)
						CHECK(hits[d] ==
						      c.width * c.height);
				}
			}
		}
	}
}

static void test_both_layouts(void)
{
	ctui_calendar cal;
	ctui_calendar_init(&cal, 2026, 11, 1);
	ctui_calendar_set_today(&cal, 2026, 11, 1);
	int day = 0;

	/* no area: the compact grid as before */
	CHECK(!ctui_calendar_is_full(&cal));
	CHECK(ctui_calendar_hit(&cal, 0, 0, NULL) == CTUI_CALENDAR_PREV);
	CHECK(ctui_calendar_hit(&cal, 0, 19, NULL) == CTUI_CALENDAR_NEXT);
	CHECK(ctui_calendar_hit(&cal, 7, 0, &day) == CTUI_CALENDAR_DAY &&
	      day == 30);
	CHECK(ctui_calendar_hit(&cal, 2, 20, NULL) == CTUI_CALENDAR_OUTSIDE);

	/* an area that fits: the full layout, from Monday November 2026 has 6
	 * rows (min 14) */
	CHECK(ctui_calendar_set_area(&cal, 100, 30) &&
	      ctui_calendar_is_full(&cal));
	CHECK(ctui_calendar_hit(&cal, 0, 99, NULL) == CTUI_CALENDAR_NEXT);
	ctui_rect c =
	    ctui_calendar_full_cell(100, 30, 6, 0, 6); /* Sunday the 1st */
	CHECK(ctui_calendar_hit(&cal, c.y + 1, c.x + 3, &day) ==
		  CTUI_CALENDAR_DAY &&
	      day == 1);
	c = ctui_calendar_full_cell(100, 30, 6, 2, 3); /* Thursday the 12th */
	CHECK(ctui_calendar_hit(&cal, c.y + c.height - 1, c.x + c.width - 1,
				&day) == CTUI_CALENDAR_DAY &&
	      day == 12);
	CHECK(ctui_calendar_hit(&cal, 3, 0, &day) ==
	      CTUI_CALENDAR_OUTSIDE); /* before the 1st */

	/* clicks: a day moves the cursor, the arrows change the month, in the
	 * full grid too */
	CHECK(ctui_calendar_press(&cal, c.y + 1, c.x + 1) ==
		  CTUI_CALENDAR_MOVED &&
	      cal.day == 12);
	CHECK(ctui_calendar_press(&cal, 0, 99) == CTUI_CALENDAR_MONTH &&
	      cal.month == 12);
	CHECK(ctui_calendar_press(&cal, 0, 0) == CTUI_CALENDAR_MONTH &&
	      cal.month == 11);
	/* the wheel and the keys do not depend on the layout */
	CHECK(ctui_calendar_wheel(&cal, 1) == CTUI_CALENDAR_MONTH &&
	      cal.month == 12);
	CHECK(ctui_calendar_wheel(&cal, -1) == CTUI_CALENDAR_MONTH &&
	      cal.month == 11);
	CHECK(ctui_calendar_key(&cal, KEY_DOWN) == CTUI_CALENDAR_MOVED &&
	      cal.day == 19);
	CHECK(ctui_calendar_key(&cal, KEY_NPAGE) == CTUI_CALENDAR_MONTH &&
	      cal.month == 12);
	ctui_calendar_key(&cal, KEY_HOME);
	CHECK(cal.month == 11 && cal.day == 1);

	/* selecting: drag over days, right click, Space */
	ctui_rect a =
	    ctui_calendar_full_cell(100, 30, 6, 1, 0); /* Monday the 2nd */
	ctui_rect b =
	    ctui_calendar_full_cell(100, 30, 6, 1, 2); /* Wednesday the 4th */
	CHECK(ctui_calendar_press(&cal, a.y, a.x) == CTUI_CALENDAR_MOVED &&
	      cal.sel.dragging);
	CHECK(ctui_calendar_drag(&cal, b.y + 1, b.x + 2) ==
	      CTUI_CALENDAR_SELECTED);
	ctui_calendar_release(&cal);
	CHECK(cal.sel.count == 3 &&
	      ctui_calendar_selected_mask(&cal) == (7U << 1U));
	ctui_rect e =
	    ctui_calendar_full_cell(100, 30, 6, 3, 4); /* Friday the 20th */
	CHECK(ctui_calendar_toggle_at(&cal, e.y + 1, e.x) ==
		  CTUI_CALENDAR_SELECTED &&
	      cal.sel.count == 4);
	CHECK(ctui_calendar_key(&cal, 27) == CTUI_CALENDAR_SELECTED &&
	      cal.sel.count == 0);
	CHECK(ctui_calendar_key(&cal, 'L') == CTUI_CALENDAR_SELECTED &&
	      cal.sel.count == 2);

	/* below the minimum: the compact grid again, in the same functions */
	CHECK(!ctui_calendar_set_area(&cal, 39, 30) &&
	      !ctui_calendar_is_full(&cal));
	CHECK(ctui_calendar_hit(&cal, 0, 19, NULL) == CTUI_CALENDAR_NEXT);
	CHECK(ctui_calendar_hit(&cal, 0, 25, NULL) == CTUI_CALENDAR_OUTSIDE);
	CHECK(!ctui_calendar_set_area(&cal, 100,
				      13)); /* 6 rows of weeks need 14 */
	CHECK(ctui_calendar_set_area(&cal, 100, 14));
	ctui_calendar_show(
	    &cal, 2026, 12); /* December from Monday: 5 rows, 12 are enough */
	CHECK(ctui_calendar_set_area(&cal, 100, 12));
	ctui_calendar_show(
	    &cal, 2026,
	    11); /* ... November needs 14: the compact grid for this month */
	CHECK(!ctui_calendar_is_full(&cal));
	CHECK(!ctui_calendar_set_area(&cal, 0, 0) && cal.area_width == 0 &&
	      cal.area_height == 0);
	CHECK(!ctui_calendar_set_area(&cal, 100, -3) && cal.area_width == 0);
	CHECK(ctui_calendar_hit(&cal, 7, 19, NULL) != CTUI_CALENDAR_NEXT);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_calendar cal;
	ctui_calendar_init(&cal, 2026, 11, 12);
	ctui_calendar_set_today(&cal, 2026, 11, 1);
	cal.week_start = 0;
	cal.marked = 1U << 4U;
	ctui_calendar_set_area(&cal, 76, 22);
	screen_clear();
	ctui_calendar_draw(&cal, 1, 2);
	CHECK(screen_has_text("November 2026"));
	CHECK(screen_has_text("Mon") && screen_has_text("Sun") &&
	      screen_has_text("<") && screen_has_text(">"));
	/* the number of a day is at the top-left of its cell */
	int sy = 0, sx = 0;
	for (int d = 1; d <= 30; d++) {
		int row, col;
		ctui_calendar_cell(2026, 11, 0, d, &row, &col);
		ctui_rect c = ctui_calendar_full_cell(76, 22, 6, row, col);
		char want[16], got[16] = "";
		snprintf(want, sizeof want, "%d", d);
		mvinnstr(1 + c.y, 2 + c.x, got, (int)strlen(want));
		if (strcmp(got, want) != 0)
			CHECK(strcmp(got, want) == 0);
		sy = 1 + c.y;
		sx = 2 + c.x;
	}
	(void)sy;
	(void)sx;
	/* the cursor day is on the selection tone across its cell, today
	 * underlined */
	ctui_rect c12 = ctui_calendar_full_cell(76, 22, 6, 2, 3);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, true)));
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair;
	mvin_wch(1 + c12.y, 2 + c12.x + c12.width - 3, &cell);
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(pair ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_SELECTION, true)));
	ctui_rect c1 = ctui_calendar_full_cell(76, 22, 6, 0, 6);
	CHECK((mvinch(1 + c1.y, 2 + c1.x) & A_UNDERLINE) != 0);
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_CURSOR, true))); /* the marked 5th */

	/* too small: the compact layout */
	ctui_calendar_set_area(&cal, 30, 30);
	screen_clear();
	ctui_calendar_draw(&cal, 0, 0);
	CHECK(screen_has_text("Mo Tu We") && !screen_has_text("Mon"));
}

int main(void)
{
	RUN(test_weeks);
	RUN(test_min);
	RUN(test_cells_tile);
	RUN(test_header_parts);
	RUN(test_hit_all_pixels);
	RUN(test_both_layouts);
	RUN(test_draw);
	return CHECK_DONE();
}
