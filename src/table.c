/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * table.c - sortable table with header, selection and scrolling (table.h).
 */
#include <ctui/table.h>

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

void ctui_table_init(ctui_table *table, const ctui_table_column *columns,
		     size_t column_count, size_t rows, ctui_table_cell_fn cell,
		     void *ctx)
{
	*table = (ctui_table){.columns = columns,
			      .column_count = column_count,
			      .rows = rows,
			      .cell = cell,
			      .ctx = ctx,
			      .sort_col = -1,
			      .sort_asc = true};
}

size_t ctui_table_visible(const ctui_table *table, int height)
{
	(void)table;
	return height > 1 ? (size_t)(height - 1) : 0;
}

static void clamp(ctui_table *table, int height)
{
	size_t visible = ctui_table_visible(table, height);
	size_t max = table->rows > visible ? table->rows - visible : 0;
	if (table->first > max)
		table->first = max;
}

void ctui_table_set_rows(ctui_table *table, size_t rows)
{
	table->rows = rows;
	if (table->selected >= rows)
		table->selected = rows ? rows - 1 : 0;
	if (table->first >= rows)
		table->first = rows ? rows - 1 : 0;
}

static size_t column_total(const ctui_table *table)
{
	return table->column_count < CTUI_TABLE_MAX_COLUMNS
		   ? table->column_count
		   : CTUI_TABLE_MAX_COLUMNS;
}

size_t ctui_table_layout(const ctui_table_column *columns, size_t column_count,
			 int width, ctui_table_span *out)
{
	size_t n = column_count < CTUI_TABLE_MAX_COLUMNS
		       ? column_count
		       : CTUI_TABLE_MAX_COLUMNS;
	long fixed = 0, weights = 0;
	size_t last_flex = n;
	for (size_t i = 0; i < n; i++) {
		if (columns[i].width > 0) {
			fixed += columns[i].width;
		} else {
			weights += columns[i].flex > 0 ? columns[i].flex : 1;
			last_flex = i;
		}
	}
	long rest = (long)width - (long)(n ? n - 1 : 0) - fixed;
	if (rest < 0)
		rest = 0;
	long left = rest;
	long x = 0;
	size_t visible = 0;
	for (size_t i = 0; i < n; i++) {
		long w = columns[i].width;
		if (w <= 0) {
			w = rest * (columns[i].flex > 0 ? columns[i].flex : 1) /
			    weights;
			if (i == last_flex)
				w = left; /* the rounding remainder goes to the
					     last flexible column */
			left -= w;
		}
		long shown = w < width - x ? w : width - x;
		out[i].x = (int)(x < width ? x : width);
		out[i].width = shown > 0 ? (int)shown : 0;
		if (out[i].width > 0)
			visible++;
		x += w + 1;
	}
	return visible;
}

bool ctui_table_has_scrollbar(const ctui_table *table, int width, int height)
{
	size_t visible = ctui_table_visible(table, height);
	return width > 4 && visible > 0 && table->rows > visible;
}

/* Layout inside the table, leaving the scrollbar column out. */
static size_t layout(const ctui_table *table, int width, int height,
		     ctui_table_span *spans)
{
	int body =
	    ctui_table_has_scrollbar(table, width, height) ? width - 1 : width;
	return ctui_table_layout(table->columns, table->column_count, body,
				 spans);
}

void ctui_table_scroll(ctui_table *table, int height)
{
	size_t visible = ctui_table_visible(table, height);
	if (visible == 0)
		return;
	if (table->selected < table->first)
		table->first = table->selected;
	else if (table->selected >= table->first + visible)
		table->first = table->selected - visible + 1;
	clamp(table, height);
}

bool ctui_table_select(ctui_table *table, size_t row, int height)
{
	if (row >= table->rows)
		return false;
	bool changed = row != table->selected;
	table->selected = row;
	ctui_table_scroll(table, height);
	return changed;
}

ctui_table_event ctui_table_key(ctui_table *table, int key, int height)
{
	if (table->rows == 0)
		return CTUI_TABLE_NONE;
	size_t page = ctui_table_visible(table, height);
	size_t before = table->selected, target;
	if (page == 0)
		page = 1;
	switch (key) {
	case '\n':
	case KEY_ENTER:
		return CTUI_TABLE_ACTIVATED;
	case KEY_DOWN:
	case 'j':
		target = before + 1 < table->rows ? before + 1 : before;
		break;
	case KEY_UP:
	case 'k':
		target = before > 0 ? before - 1 : 0;
		break;
	case KEY_NPAGE:
		target = before + page < table->rows ? before + page
						     : table->rows - 1;
		break;
	case KEY_PPAGE:
		target = before > page ? before - page : 0;
		break;
	case KEY_HOME:
		target = 0;
		break;
	case KEY_END:
		target = table->rows - 1;
		break;
	default:
		return CTUI_TABLE_NONE;
	}
	ctui_table_select(table, target, height);
	return table->selected != before ? CTUI_TABLE_MOVED : CTUI_TABLE_NONE;
}

ctui_table_part ctui_table_hit(const ctui_table *table, int width, int height,
			       int y, int x, size_t *row, size_t *col)
{
	if (y < 0 || y >= height || x < 0 || x >= width)
		return CTUI_TABLE_OUTSIDE;
	if (y >= 1 && x == width - 1 &&
	    ctui_table_has_scrollbar(table, width, height))
		return CTUI_TABLE_SCROLLBAR;
	ctui_table_span spans[CTUI_TABLE_MAX_COLUMNS];
	layout(table, width, height, spans);
	size_t n = column_total(table), found = n;
	for (size_t i = 0; i < n; i++)
		if (spans[i].width > 0 && x >= spans[i].x &&
		    x <= spans[i].x + spans[i].width)
			found =
			    i; /* the separator after a column belongs to it */
	if (found == n)
		return CTUI_TABLE_OUTSIDE;
	if (y == 0) {
		if (col)
			*col = found;
		return CTUI_TABLE_HEADER;
	}
	size_t r = table->first + (size_t)(y - 1);
	if (r >= table->rows)
		return CTUI_TABLE_OUTSIDE;
	if (col)
		*col = found;
	if (row)
		*row = r;
	return CTUI_TABLE_CELL;
}

void ctui_table_toggle_sort(ctui_table *table, size_t col)
{
	if (table->sort_col == (int)col) {
		table->sort_asc = !table->sort_asc;
	} else {
		table->sort_col = (int)col;
		table->sort_asc = true;
	}
}

/* Put the scrollbar's state in table-relative coordinates for an event. */
static void sync_bar(ctui_table *table, int width, int height)
{
	ctui_scrollbar_set(&table->bar, table->rows,
			   ctui_table_visible(table, height), table->first);
	ctui_scrollbar_place(&table->bar, 1, width - 1, height - 1);
}

static void bar_moved(ctui_table *table, int height)
{
	table->first = table->bar.offset;
	clamp(table, height);
}

ctui_table_event ctui_table_press(ctui_table *table, int width, int height,
				  int y, int x)
{
	size_t row = 0, col = 0;
	switch (ctui_table_hit(table, width, height, y, x, &row, &col)) {
	case CTUI_TABLE_SCROLLBAR:
		sync_bar(table, width, height);
		if (!ctui_scrollbar_press(&table->bar, y, x))
			return CTUI_TABLE_NONE;
		bar_moved(table, height);
		return CTUI_TABLE_SCROLLED;
	case CTUI_TABLE_HEADER:
		ctui_table_toggle_sort(table, col);
		return CTUI_TABLE_SORTED;
	case CTUI_TABLE_CELL:
		return ctui_table_select(table, row, height) ? CTUI_TABLE_MOVED
							     : CTUI_TABLE_NONE;
	case CTUI_TABLE_OUTSIDE:
		break;
	}
	return CTUI_TABLE_NONE;
}

bool ctui_table_drag(ctui_table *table, int width, int height, int y)
{
	if (!table->bar.dragging)
		return false;
	sync_bar(table, width, height);
	if (!ctui_scrollbar_drag(&table->bar, y))
		return false;
	bar_moved(table, height);
	return true;
}

bool ctui_table_release(ctui_table *table)
{
	return ctui_scrollbar_release(&table->bar);
}

bool ctui_table_wheel(ctui_table *table, int height, int rows)
{
	size_t visible = ctui_table_visible(table, height);
	size_t max = table->rows > visible ? table->rows - visible : 0;
	size_t first = table->first;
	if (rows < 0)
		first = first > (size_t)-rows ? first - (size_t)-rows : 0;
	else
		first = first + (size_t)rows < max ? first + (size_t)rows : max;
	bool changed = first != table->first;
	table->first = first;
	return changed;
}

/* ---- sorting ---- */

typedef struct sort_ctx {
	const ctui_table *table;
	size_t col;
	bool ascending;
	ctui_table_compare cmp;
	void *cmp_ctx;
} sort_ctx;

static const char *cell_text(const ctui_table *table, size_t row, size_t col)
{
	const char *s = table->cell ? table->cell(table->ctx, row, col) : NULL;
	return s ? s : "";
}

static bool number_of(const char *s, double *value)
{
	while (*s == ' ' || *s == '\t')
		s++;
	char *end;
	*value = strtod(s, &end);
	return end != s;
}

static int compare(const sort_ctx *sc, size_t a, size_t b)
{
	int r;
	if (sc->cmp) {
		r = sc->cmp(sc->cmp_ctx, a, b);
	} else {
		/* the text of a cell may be overwritten by the next call: copy
		 * the first */
		char first[CTUI_TEXT_MAX];
		snprintf(first, sizeof first, "%s",
			 cell_text(sc->table, a, sc->col));
		const char *second = cell_text(sc->table, b, sc->col);
		if (sc->table->columns[sc->col].numeric) {
			double da = 0, db = 0;
			bool oka = number_of(first, &da),
			     okb = number_of(second, &db);
			r = oka != okb ? (oka ? 1 : -1) : (da > db) - (da < db);
		} else {
			r = strcmp(first, second);
		}
	}
	return sc->ascending ? r : -r;
}

size_t *ctui_table_sort_indices(const ctui_table *table, size_t col,
				bool ascending, ctui_table_compare cmp,
				void *cmp_ctx)
{
	if (!cmp && col >= table->column_count)
		return NULL;
	size_t n = table->rows;
	size_t *idx = malloc((n ? n : 1) * sizeof *idx);
	size_t *tmp = malloc((n ? n : 1) * sizeof *tmp);
	if (!idx || !tmp) {
		free(idx);
		free(tmp);
		return NULL;
	}
	for (size_t i = 0; i < n; i++)
		idx[i] = i;
	sort_ctx sc = {table, col, ascending, cmp, cmp_ctx};
	/* bottom-up merge sort: stable, ties keep their order */
	for (size_t run = 1; run < n; run *= 2) {
		for (size_t lo = 0; lo < n; lo += 2 * run) {
			size_t mid = lo + run < n ? lo + run : n,
			       hi = lo + 2 * run < n ? lo + 2 * run : n;
			size_t i = lo, j = mid, k = lo;
			while (i < mid && j < hi)
				tmp[k++] = compare(&sc, idx[i], idx[j]) <= 0
					       ? idx[i++]
					       : idx[j++];
			while (i < mid)
				tmp[k++] = idx[i++];
			while (j < hi)
				tmp[k++] = idx[j++];
		}
		memcpy(idx, tmp, n * sizeof *idx);
	}
	free(tmp);
	return idx;
}

/* ---- drawing ---- */

/* Draw text in a cell of w columns: sanitised, aligned, cut with the ellipsis.
 */
static void put_cell(int y, int x, int w, const char *text,
		     ctui_table_align align, int attr)
{
	ctui_fill(y, x, w, attr);
	char buf[CTUI_TEXT_MAX];
	ctui_sanitize(buf, sizeof buf, text ? text : "", ctui_theme_unicode());
	size_t cols = ctui_utf8_columns(buf);
	if (cols > (size_t)w) {
		const char *ell = ctui_theme_glyphs()->ellipsis;
		size_t ec = ctui_utf8_columns(ell);
		if (ec >= (size_t)w) {
			ctui_put(y, x, w, ell, attr);
			return;
		}
		size_t n = ctui_utf8_prefix_bytes(buf, (size_t)w - ec);
		snprintf(buf + n, sizeof buf - n, "%s", ell);
		cols = ctui_utf8_columns(buf);
	}
	int off = align == CTUI_TABLE_RIGHT    ? w - (int)cols
		  : align == CTUI_TABLE_CENTER ? (w - (int)cols) / 2
					       : 0;
	ctui_put(y, x + off, w - off, buf, attr);
}

/* One row: the cells and the separators between them. */
static void row_draw(const ctui_table *table, const ctui_table_span *spans,
		     int y, int x, int body, size_t r, bool header, int attr,
		     int line)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	size_t n = column_total(table);
	for (size_t c = 0; c < n; c++) {
		if (spans[c].width <= 0)
			continue;
		const char *text;
		char title[CTUI_TEXT_MAX];
		if (header) {
			const ctui_table_column *col = &table->columns[c];
			const char *arrow = !g->unicode
						? (table->sort_asc ? "^" : "v")
					    : table->sort_asc ? "\xe2\x96\xb2"
							      : "\xe2\x96\xbc";
			if (table->sort_col == (int)c)
				snprintf(title, sizeof title, "%s %s",
					 col->title ? col->title : "", arrow);
			else
				snprintf(title, sizeof title, "%s",
					 col->title ? col->title : "");
			text = title;
		} else {
			text = cell_text(table, r, c);
		}
		put_cell(y, x + spans[c].x, spans[c].width, text,
			 table->columns[c].align, attr);
		if (c + 1 < n && spans[c].x + spans[c].width < body)
			ctui_put(y, x + spans[c].x + spans[c].width, 1,
				 g->box_v, line);
	}
}

static void table_draw_impl(const ctui_table *table, int y, int x, int width,
			    int height)
{
	if (height < 1 || width < 1)
		return;
	ctui_table_span spans[CTUI_TABLE_MAX_COLUMNS];
	layout(table, width, height, spans);
	bool bar = ctui_table_has_scrollbar(table, width, height);
	int body = bar ? width - 1 : width;

	int head = (int)((unsigned)ctui_attr(CTUI_PAIR_HEADING, true) |
			 (unsigned)A_UNDERLINE);
	ctui_fill(y, x, body, head);
	row_draw(table, spans, y, x, body, 0, true, head, head);

	size_t visible = ctui_table_visible(table, height);
	for (size_t k = 0; k < visible && table->first + k < table->rows; k++) {
		size_t r = table->first + k;
		bool selected = r == table->selected;
		bool tinted = table->zebra && (r & 1U);
		int attr = selected ? ctui_attr(CTUI_PAIR_SELECTION, true)
			   : tinted ? ctui_attr(CTUI_PAIR_DIALOG, false)
				    : ctui_attr(CTUI_PAIR_BODY, false);
		int line = selected ? attr
				    : ctui_attr(tinted ? CTUI_PAIR_DIALOG_BORDER
						       : CTUI_PAIR_BORDER,
						false);
		ctui_fill(y + 1 + (int)k, x, body, attr);
		row_draw(table, spans, y + 1 + (int)k, x, body, r, false, attr,
			 line);
	}

	if (bar) {
		ctui_scrollbar sb = table->bar;
		ctui_scrollbar_set(&sb, table->rows, visible, table->first);
		ctui_scrollbar_place(&sb, y + 1, x + width - 1, height - 1);
		ctui_scrollbar_draw(&sb, false);
	}
}

void ctui_table_draw(const ctui_table *table, int y, int x, int width,
		     int height)
{
	ctui_style_begin(table->style);
	table_draw_impl(table, y, x, width, height);
	ctui_style_end();
}
