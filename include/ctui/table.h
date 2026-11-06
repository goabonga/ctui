/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * table.h - a sortable table with a header row, a selected row and
 * vertical scrolling.
 *
 * The data stays with the caller: columns are described by an array and
 * the cells are read through an accessor, so a large table needs no copy.
 * A column is fixed (width > 0) or flexible (width <= 0, sharing what the
 * fixed columns leave in proportion to `flex`, at least 1). Columns are
 * separated by one column holding the box glyph; text that does not fit
 * ends with the ellipsis glyph, and wide characters keep their columns.
 *
 * The first row is the header. A click on it asks for a sort: the table
 * only records the column and the direction (sort_col, sort_asc) and
 * reports CTUI_TABLE_SORTED; ordering the rows is the caller's job, and
 * ctui_table_sort_indices() gives it a stable permutation to read the rows
 * through. A scrollbar appears on the right when the rows do not fit.
 *
 * Coordinates of the event functions are relative to the top-left cell of
 * the table, which is drawn in a width x height area (header included).
 */
#ifndef CTUI_TABLE_H
#define CTUI_TABLE_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/scrollbar.h>
#include <ctui/theme.h>

#define CTUI_TABLE_MAX_COLUMNS 32 /* columns beyond this are ignored */

typedef enum ctui_table_align {
	CTUI_TABLE_LEFT = 0,
	CTUI_TABLE_RIGHT,
	CTUI_TABLE_CENTER
} ctui_table_align;

typedef struct ctui_table_column {
	const char *title;
	int width; /* columns when > 0, else flexible */
	int flex;  /* weight of a flexible column (0 counts as 1) */
	ctui_table_align align;
	bool numeric; /* sorted by value rather than as text */
} ctui_table_column;

/* Text of the cell at (row, col); NULL reads as empty. Must stay valid
 * until the next call. */
typedef const char *(*ctui_table_cell_fn)(void *ctx, size_t row, size_t col);

/* Order two rows: negative, zero or positive like strcmp(). */
typedef int (*ctui_table_compare)(void *ctx, size_t a, size_t b);

typedef struct ctui_table {
	const ctui_table_column *columns;
	size_t column_count;
	size_t rows;
	ctui_table_cell_fn cell;
	void *ctx;
	size_t selected; /* meaningful when rows > 0 */
	size_t first;	 /* first visible row */
	int sort_col;	 /* column of the last sort request, -1 for none */
	bool sort_asc;
	bool zebra;		 /* alternate rows in the dialog tone */
	ctui_scrollbar bar;	 /* drag state */
	const ctui_style *style; /* colours and glyphs of this table, may be
				    NULL; set after init */
} ctui_table;

typedef enum ctui_table_event {
	CTUI_TABLE_NONE = 0,
	CTUI_TABLE_MOVED,     /* the selected row changed */
	CTUI_TABLE_ACTIVATED, /* Enter */
	CTUI_TABLE_SORTED,  /* sort_col / sort_asc changed: reorder the rows */
	CTUI_TABLE_SCROLLED /* the scrollbar took the event */
} ctui_table_event;

typedef enum ctui_table_part {
	CTUI_TABLE_OUTSIDE = 0,
	CTUI_TABLE_HEADER,
	CTUI_TABLE_CELL,
	CTUI_TABLE_SCROLLBAR
} ctui_table_part;

/* Where a column lies: x from the left edge, width in columns (0 when it
 * is cut off entirely). */
typedef struct ctui_table_span {
	int x, width;
} ctui_table_span;

void ctui_table_init(ctui_table *table, const ctui_table_column *columns,
		     size_t column_count, size_t rows, ctui_table_cell_fn cell,
		     void *ctx);

/* Change the number of rows; selection and scroll are clamped. */
void ctui_table_set_rows(ctui_table *table, size_t rows);

/* Pure layout of column_count columns in width columns, one separator
 * between neighbours. Fills out[column_count] and returns the number of
 * columns that have a visible part. */
size_t ctui_table_layout(const ctui_table_column *columns, size_t column_count,
			 int width, ctui_table_span *out);

/* Rows of the body in a table of `height` rows (the header takes one). */
size_t ctui_table_visible(const ctui_table *table, int height);

/* Whether the scrollbar is shown in a width x height table. */
bool ctui_table_has_scrollbar(const ctui_table *table, int width, int height);

/* Move the selection to `row`, scrolling to show it. */
bool ctui_table_select(ctui_table *table, size_t row, int height);

/* Scroll so that the selected row is inside the body. */
void ctui_table_scroll(ctui_table *table, int height);

/* Up/Down/j/k, PgUp/PgDn, Home/End, Enter. */
ctui_table_event ctui_table_key(ctui_table *table, int key, int height);

/* What is at (y, x): the header, a cell, the scrollbar or nothing. For the
 * header and cells, *col is the column (the separator belongs to the column
 * on its left); for cells *row is the row. Either pointer may be NULL. */
ctui_table_part ctui_table_hit(const ctui_table *table, int width, int height,
			       int y, int x, size_t *row, size_t *col);

/* Ask for a sort on `col`: the same column flips the direction, another
 * one starts ascending. */
void ctui_table_toggle_sort(ctui_table *table, size_t col);

/* Left press: the scrollbar, a header (sort request) or a row (selection). */
ctui_table_event ctui_table_press(ctui_table *table, int width, int height,
				  int y, int x);

/* Pointer moved / released after a press on the scrollbar; true when the
 * view changed / a drag ended. */
bool ctui_table_drag(ctui_table *table, int width, int height, int y);
bool ctui_table_release(ctui_table *table);

/* Wheel: scroll the view by `rows` (negative = up) keeping the selection. */
bool ctui_table_wheel(ctui_table *table, int height, int rows);

/* A stable permutation of 0 .. rows-1 ordering the rows by column col,
 * descending when !ascending. cmp orders two rows; with NULL the cells are
 * compared by value (leading number, text without one first) in a numeric
 * column and as text otherwise. Returns a malloc'd array the caller frees,
 * or NULL when out of memory (or col is out of range without a cmp). */
size_t *ctui_table_sort_indices(const ctui_table *table, size_t col,
				bool ascending, ctui_table_compare cmp,
				void *cmp_ctx);

/* Draw the table in a width x height area. */
void ctui_table_draw(const ctui_table *table, int y, int x, int width,
		     int height);

#endif /* CTUI_TABLE_H */
