/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * panel.c - sectioned reading panels (panel.h).
 */
#include <ctui/panel.h>

#include <curses.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/gauge.h>
#include <ctui/text.h>

#define COLUMN_GAP 3

static char *dup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = malloc(n);
	if (p)
		memcpy(p, s, n);
	return p;
}

void ctui_panel_init(ctui_panel *panel)
{
	memset(panel, 0, sizeof *panel);
}

void ctui_panel_free(ctui_panel *panel)
{
	for (size_t i = 0; i < panel->count; i++)
		free(panel->lines[i].text);
	free(panel->lines);
	free(panel->pending);
	ctui_panel_init(panel);
}

bool ctui_panel_is_empty(const ctui_panel *panel)
{
	return panel->count == 0;
}

static ctui_panel_line *push(ctui_panel *panel, ctui_line_kind kind,
			     const char *text)
{
	if (panel->count == panel->cap) {
		size_t cap = panel->cap ? panel->cap * 2 : 32;
		ctui_panel_line *lines =
		    realloc(panel->lines, cap * sizeof *lines);
		if (!lines)
			return NULL;
		panel->lines = lines;
		panel->cap = cap;
	}
	char *copy = dup(text);
	if (!copy)
		return NULL;
	ctui_panel_line *line = &panel->lines[panel->count++];
	*line = (ctui_panel_line){
	    .kind = kind, .gauge_col = -1, .gauge_fraction = 0, .text = copy};
	return line;
}

static void flush(ctui_panel *panel)
{
	if (!panel->pending)
		return;
	if (panel->count)
		push(panel, CTUI_LINE_BLANK, "");
	push(panel, CTUI_LINE_HEADING, panel->pending);
	push(panel, CTUI_LINE_RULE, "");
	free(panel->pending);
	panel->pending = NULL;
}

void ctui_panel_section(ctui_panel *panel, const char *title)
{
	free(panel->pending);
	panel->pending = dup(title);
}

void ctui_panel_add_line(ctui_panel *panel, const char *text)
{
	flush(panel);
	push(panel, CTUI_LINE_TEXT, text);
}

/* Append s padded to width columns (left- or right-aligned). */
static void pad(char *dst, size_t cap, const char *s, int width, bool right)
{
	size_t len = strlen(dst);
	int cols = (int)ctui_utf8_columns(s);
	int fill = cols < width ? width - cols : 0;
	if (right)
		for (int i = 0; i < fill && len + 1 < cap; i++)
			dst[len++] = ' ';
	for (const char *p = s; *p && len + 1 < cap; p++)
		dst[len++] = *p;
	if (!right)
		for (int i = 0; i < fill && len + 1 < cap; i++)
			dst[len++] = ' ';
	dst[len] = '\0';
}

void ctui_panel_row(ctui_panel *panel, const char *label, const char *value,
		    double gauge, const char *note)
{
	if (!value)
		return;
	char text[CTUI_TEXT_MAX] = "  ";
	pad(text, sizeof text, label, CTUI_PANEL_LABEL_WIDTH, false);
	strncat(text, " ", sizeof text - strlen(text) - 1);
	pad(text, sizeof text, value, CTUI_PANEL_VALUE_WIDTH, true);
	int gauge_col = -1;
	bool has_gauge = isfinite(gauge) && gauge >= 0;
	if (has_gauge) {
		strncat(text, "  ", sizeof text - strlen(text) - 1);
		gauge_col = (int)ctui_utf8_columns(text);
		pad(text, sizeof text, "", CTUI_PANEL_GAUGE_WIDTH, false);
	}
	if (note && *note) {
		strncat(text, "  ", sizeof text - strlen(text) - 1);
		strncat(text, note, sizeof text - strlen(text) - 1);
	}
	flush(panel);
	ctui_panel_line *line = push(panel, CTUI_LINE_TEXT, text);
	if (line && has_gauge) {
		line->gauge_col = gauge_col;
		line->gauge_fraction = gauge > 1 ? 1 : gauge;
	}
}

void ctui_panel_finish(ctui_panel *panel, const char *empty)
{
	if (panel->count == 0 && empty && *empty)
		push(panel, CTUI_LINE_TEXT, empty);
	free(panel->pending);
	panel->pending = NULL;
}

void ctui_panel_layout_free(ctui_panel_layout *layout)
{
	free(layout->column);
	free(layout->row);
	memset(layout, 0, sizeof *layout);
}

void ctui_panel_arrange(const ctui_panel *panel, int width, bool allow_columns,
			ctui_panel_layout *layout)
{
	memset(layout, 0, sizeof *layout);
	size_t n = panel->count;
	layout->count = n;
	layout->column = calloc(n ? n : 1, sizeof(int));
	layout->row = calloc(n ? n : 1, sizeof(int));
	if (!layout->column || !layout->row) {
		ctui_panel_layout_free(layout);
		return;
	}
	bool two =
	    allow_columns && width >= CTUI_PANEL_TWO_COLUMNS_MIN && n > 0;
	size_t split = n; /* first line of the second column */
	if (two) {
		/* Break on the section boundary closest to half the lines. */
		size_t best = n, best_gap = n;
		for (size_t i = 1; i < n; i++) {
			if (panel->lines[i].kind != CTUI_LINE_BLANK)
				continue;
			size_t gap = i > n - i ? i - (n - i) : (n - i) - i;
			if (gap < best_gap) {
				best_gap = gap;
				best = i;
			}
		}
		split = best;
	}
	layout->columns = two && split < n ? 2 : 1;
	layout->width = layout->columns == 2 ? (width - COLUMN_GAP) / 2 : width;
	int rows[2] = {0, 0};
	for (size_t i = 0; i < n; i++) {
		int col = layout->columns == 2 && i >= split ? 1 : 0;
		layout->column[i] = col;
		if (panel->lines[i].kind == CTUI_LINE_BLANK && rows[col] == 0) {
			layout->row[i] =
			    -1; /* no blank line at the top of a column */
			continue;
		}
		layout->row[i] = rows[col]++;
	}
	layout->rows = (size_t)(rows[0] > rows[1] ? rows[0] : rows[1]);
}

static void panel_draw_impl(const ctui_panel *panel,
			    const ctui_panel_layout *layout, int y, int x,
			    int rows, size_t scroll)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	for (size_t i = 0; i < panel->count && i < layout->count; i++) {
		int row = layout->row[i];
		if (row < 0 || (size_t)row < scroll ||
		    (size_t)row >= scroll + (size_t)rows)
			continue;
		const ctui_panel_line *line = &panel->lines[i];
		int py = y + row - (int)scroll;
		int px = x + layout->column[i] * (layout->width + COLUMN_GAP);
		switch (line->kind) {
		case CTUI_LINE_HEADING:
			ctui_put(py, px, layout->width, line->text,
				 ctui_attr(CTUI_PAIR_HEADING, true));
			break;
		case CTUI_LINE_RULE: {
			int w = layout->width < CTUI_PANEL_RULE_WIDTH
				    ? layout->width
				    : CTUI_PANEL_RULE_WIDTH;
			for (int k = 0; k < w; k++)
				ctui_put(py, px + k, 1, g->rule,
					 ctui_attr(CTUI_PAIR_MUTED, false));
			break;
		}
		case CTUI_LINE_TEXT:
			ctui_put(py, px, layout->width, line->text,
				 ctui_attr(CTUI_PAIR_BODY, false));
			if (line->gauge_col >= 0 &&
			    line->gauge_col < layout->width) {
				int gw = layout->width - line->gauge_col;
				if (gw > CTUI_PANEL_GAUGE_WIDTH)
					gw = CTUI_PANEL_GAUGE_WIDTH;
				ctui_gauge_draw(py, px + line->gauge_col, gw,
						true, line->gauge_fraction,
						1.0);
			}
			break;
		case CTUI_LINE_BLANK:
			break;
		}
	}
}

void ctui_panel_draw(const ctui_panel *panel, const ctui_panel_layout *layout,
		     int y, int x, int rows, size_t scroll)
{
	ctui_style_begin(panel->style);
	panel_draw_impl(panel, layout, y, x, rows, scroll);
	ctui_style_end();
}
