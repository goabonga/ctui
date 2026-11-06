/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * helpview.c - key-binding help overlay (helpview.h).
 */
#include <ctui/helpview.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define WHEEL_STEP 3
#define MAX_HEIGHT 30
#define MAX_WIDTH 100
#define COLUMN_GAP 2

void ctui_helpview_init(ctui_helpview *h, const ctui_help_group *groups,
			size_t group_count)
{
	*h = (ctui_helpview){.groups = groups, .group_count = group_count};
}

size_t ctui_helpview_lines(const ctui_helpview *h)
{
	size_t n = 0;
	for (size_t g = 0; g < h->group_count; g++)
		n += (g ? 1 : 0) + (h->groups[g].title ? 1 : 0) +
		     h->groups[g].count;
	return n;
}

bool ctui_helpview_geometry(const ctui_helpview *h, int lines, int cols,
			    ctui_rect *box, int *rows, int *columns)
{
	int width = cols - 4 < MAX_WIDTH ? cols - 4 : MAX_WIDTH;
	int ncols = width - 4 >= CTUI_HELP_TWO_COLUMNS_MIN ? 2 : 1;
	size_t total = ctui_helpview_lines(h);
	int need = (int)((total + (size_t)ncols - 1) / (size_t)ncols);
	int height = need + 3 < MAX_HEIGHT ? need + 3 : MAX_HEIGHT;
	if (height < 5)
		height = 5;
	if (!ctui_dialog_box(lines, cols, width, height, box) ||
	    box->height < 5 || box->width < 20)
		return false;
	*rows = box->height - 3;
	*columns = ncols;
	return true;
}

static size_t max_top(const ctui_helpview *h, int rows, int columns)
{
	size_t total = ctui_helpview_lines(h),
	       capacity = (size_t)rows * (size_t)columns;
	return total > capacity ? total - capacity : 0;
}

ctui_dialog_result ctui_helpview_key(ctui_helpview *h, int key, int lines,
				     int cols)
{
	ctui_rect box;
	int rows = 1, columns = 1;
	if (!ctui_helpview_geometry(h, lines, cols, &box, &rows, &columns)) {
		rows = 1;
		columns = 1;
	}
	size_t capacity = (size_t)rows * (size_t)columns,
	       page = capacity > 1 ? capacity - 1 : 1;
	switch (key) {
	case 27:
	case 3:
	case 'q':
	case '?':
	case '\n':
	case KEY_ENTER:
		return CTUI_DIALOG_CANCELLED;
	case KEY_HOME:
	case 'g':
		h->top = 0;
		break;
	case KEY_END:
	case 'G':
		h->top = max_top(h, rows, columns);
		break;
	case KEY_DOWN:
	case 'j':
		h->top++;
		break;
	case KEY_UP:
	case 'k':
		h->top = h->top ? h->top - 1 : 0;
		break;
	case KEY_NPAGE:
	case ' ':
		h->top += page;
		break;
	case KEY_PPAGE:
	case 'b':
		h->top = h->top > page ? h->top - page : 0;
		break;
	default:
		break;
	}
	if (h->top > max_top(h, rows, columns))
		h->top = max_top(h, rows, columns);
	return CTUI_DIALOG_OPEN;
}

ctui_dialog_result ctui_helpview_click(const ctui_helpview *h, int lines,
				       int cols, int y, int x)
{
	ctui_rect box;
	int rows, columns;
	if (ctui_helpview_geometry(h, lines, cols, &box, &rows, &columns) &&
	    ctui_frame_close_hit(box.y, box.x, box.width, y, x))
		return CTUI_DIALOG_CANCELLED;
	return CTUI_DIALOG_OPEN;
}

void ctui_helpview_wheel(ctui_helpview *h, int lines, int cols, int dir)
{
	ctui_rect box;
	int rows, columns;
	if (!ctui_helpview_geometry(h, lines, cols, &box, &rows, &columns))
		return;
	size_t max = max_top(h, rows, columns);
	if (dir > 0)
		h->top = h->top + WHEEL_STEP < max ? h->top + WHEEL_STEP : max;
	else if (dir < 0)
		h->top = h->top > WHEEL_STEP ? h->top - WHEEL_STEP : 0;
}

/* What line k of the content is: a heading, an entry or a blank. */
typedef struct line {
	const char *heading;
	const ctui_help_entry *entry;
} line;

static line line_at(const ctui_helpview *h, size_t k)
{
	for (size_t g = 0; g < h->group_count; g++) {
		const ctui_help_group *group = &h->groups[g];
		if (g) {
			if (k == 0)
				return (line){0};
			k--;
		}
		if (group->title) {
			if (k == 0)
				return (line){.heading = group->title};
			k--;
		}
		if (k < group->count)
			return (line){.entry = &group->entries[k]};
		k -= group->count;
	}
	return (line){0};
}

static void helpview_draw_impl(const ctui_helpview *h)
{
	ctui_rect box;
	int rows, columns;
	if (!ctui_helpview_geometry(h, LINES, COLS, &box, &rows, &columns))
		return;
	ctui_labels l = ctui_labels_resolve(h->labels);
	ctui_frame_draw(box.y, box.x, box.height, box.width, true, true,
			h->title ? h->title : l.help_title);
	ctui_frame_close_draw(box.y, box.x, box.width, true);
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false),
	    bold = ctui_attr(CTUI_PAIR_DIALOG, true);
	int inner_w = box.width - 4;
	int colw = (inner_w - (columns - 1) * COLUMN_GAP) / columns;

	/* the keys column is as wide as the longest keys text, within reason */
	int kw = 0;
	for (size_t g = 0; g < h->group_count; g++)
		for (size_t i = 0; i < h->groups[g].count; i++) {
			int w = (int)ctui_utf8_columns(
			    h->groups[g].entries[i].keys);
			if (w > kw)
				kw = w;
		}
	if (kw > colw * 2 / 5)
		kw = colw * 2 / 5;

	size_t total = ctui_helpview_lines(h);
	size_t capacity = (size_t)rows * (size_t)columns;
	size_t top = h->top > max_top(h, rows, columns)
			 ? max_top(h, rows, columns)
			 : h->top;
	for (int c = 0; c < columns; c++)
		for (int r = 0; r < rows; r++) {
			size_t k = top + (size_t)c * (size_t)rows + (size_t)r;
			if (k >= total)
				continue;
			line ln = line_at(h, k);
			int y = box.y + 1 + r,
			    x = box.x + 2 + c * (colw + COLUMN_GAP);
			if (ln.heading)
				ctui_put(y, x, colw, ln.heading,
					 (int)((unsigned)bold |
					       (unsigned)A_UNDERLINE));
			else if (ln.entry) {
				ctui_put(y, x, kw, ln.entry->keys, bold);
				ctui_put(y, x + kw + 2, colw - kw - 2,
					 ln.entry->description, dialog);
			}
		}
	ctui_put(box.y + box.height - 2, box.x + 2, inner_w, l.help_hint,
		 (int)((unsigned)dialog | (unsigned)A_DIM));
	if (total > capacity && box.width > 20) {
		char pos[64];
		size_t last = top + capacity < total ? top + capacity : total;
		snprintf(pos, sizeof pos, " %zu-%zu/%zu ", top + 1, last,
			 total);
		ctui_put(box.y + box.height - 1,
			 box.x + box.width - 2 - (int)strlen(pos),
			 (int)strlen(pos), pos,
			 ctui_attr(CTUI_PAIR_DIALOG_BORDER, false));
	}
}

void ctui_helpview_draw(const ctui_helpview *h)
{
	ctui_style_begin(h->style);
	helpview_draw_impl(h);
	ctui_style_end();
}
