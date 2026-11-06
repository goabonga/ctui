/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * treemap.c - squarified treemap of weighted items (treemap.h).
 */
#include <ctui/treemap.h>

#include <curses.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctui/text.h>

#define PALETTE_COLORS 6 /* the icon colours before gray */

void ctui_treemap_init(ctui_treemap *t, const ctui_treemap_item *items,
		       size_t count)
{
	*t = (ctui_treemap){
	    .items = items, .count = count, .show_weights = true};
}

bool ctui_treemap_weight_ok(double weight)
{
	return isfinite(weight) && weight > 0;
}

typedef struct entry {
	double w; /* weight relative to the largest, in (0, 1] */
	size_t i;
} entry;

/* Decreasing weight, then position. */
static int by_weight(const void *pa, const void *pb)
{
	const entry *a = pa, *b = pb;
	if (a->w != b->w)
		return a->w > b->w ? -1 : 1;
	return a->i < b->i ? -1 : a->i > b->i;
}

static double max_d(double a, double b)
{
	return a > b ? a : b;
}

/* Worst aspect ratio of a row of total area s, largest piece amax and
 * smallest amin laid along a side of the given length. */
static double worst(double side, double s, double amax, double amin)
{
	return max_d(side * side * amax / (s * s),
		     s * s / (side * side * amin));
}

/* Where the exact edge `v` (relative to the area) lands on the cells. */
static int cell(double v, int size)
{
	long long c = llround(v);
	return c < 0 ? 0 : c > size ? size : (int)c;
}

size_t ctui_treemap_squarify(const ctui_treemap_item *items, size_t count,
			     ctui_rect area, ctui_rect *out)
{
	for (size_t i = 0; out && i < count; i++)
		out[i] = (ctui_rect){
		    .y = area.y, .x = area.x, .height = 0, .width = 0};
	if (!items || !out || count == 0 || area.width <= 0 ||
	    area.height <= 0 || (long long)area.x + area.width > INT_MAX ||
	    (long long)area.y + area.height > INT_MAX)
		return 0;

	double top = 0;
	for (size_t i = 0; i < count; i++)
		if (ctui_treemap_weight_ok(items[i].weight) &&
		    items[i].weight > top)
			top = items[i].weight;
	if (top <= 0)
		return 0;
	entry *e = malloc(count * sizeof *e);
	if (!e)
		return 0;
	size_t n = 0;
	double sum = 0;
	for (size_t i = 0; i < count; i++) {
		double w = ctui_treemap_weight_ok(items[i].weight)
			       ? items[i].weight / top
			       : 0;
		if (w > 0) { /* not lost to underflow */
			e[n++] = (entry){.w = w, .i = i};
			sum += w;
		}
	}
	qsort(e, n, sizeof *e, by_weight);

	/* Work in cell widths, with a row counting for two so that pieces look
	 * square on a terminal. */
	double W = area.width, H = 2.0 * area.height;
	double unit = W * H / sum;
	double rx = 0, ry = 0, rw = W, rh = H;
	size_t placed = 0;
	for (size_t start = 0; start < n;) {
		double side = rw < rh ? rw : rh, s = 0, best = INFINITY;
		double amax = e[start].w * unit;
		size_t end = start;
		while (end < n) {
			double a = e[end].w * unit, ns = s + a;
			double w = worst(side, ns, amax, a);
			if (end > start && w > best)
				break;
			s = ns;
			best = w;
			end++;
		}
		bool last = end == n;
		bool column = rw >= rh; /* pieces stacked along the left edge */
		double thick =
		    last ? (column ? rw : rh) : s / (column ? rh : rw);
		double along = column ? ry : rx,
		       limit = column ? ry + rh : rx + rw;
		for (size_t k = start; k < end; k++) {
			double a = e[k].w * unit;
			double len = k == end - 1 ? limit - along : a / thick;
			double px = column ? rx : along,
			       py = column ? along : ry;
			double pw = column ? thick : len,
			       ph = column ? len : thick;
			int x0 = cell(px, area.width),
			    x1 = cell(px + pw, area.width);
			int y0 = cell(py / 2, area.height),
			    y1 = cell((py + ph) / 2, area.height);
			ctui_rect *r = &out[e[k].i];
			if (x1 > x0 && y1 > y0) {
				*r = (ctui_rect){.y = area.y + y0,
						 .x = area.x + x0,
						 .height = y1 - y0,
						 .width = x1 - x0};
				placed++;
			}
			along += len;
		}
		if (column)
			rx += thick, rw -= thick;
		else
			ry += thick, rh -= thick;
		start = end;
	}
	free(e);
	return placed;
}

long ctui_treemap_hit(const ctui_treemap *t, int width, int height, int y,
		      int x)
{
	if (!t->count || width <= 0 || height <= 0 || y < 0 || x < 0 ||
	    y >= height || x >= width)
		return -1;
	ctui_rect *r = malloc(t->count * sizeof *r);
	if (!r)
		return -1;
	long found = -1;
	ctui_treemap_squarify(t->items, t->count,
			      (ctui_rect){.height = height, .width = width}, r);
	for (size_t i = 0; i < t->count; i++)
		if (r[i].width > 0 && y >= r[i].y && y < r[i].y + r[i].height &&
		    x >= r[i].x && x < r[i].x + r[i].width) {
			found = (long)i;
			break;
		}
	free(r);
	return found;
}

static int clip_lo(int v, int lo)
{
	return v < lo ? lo : v;
}

static void draw_item(const ctui_treemap_item *it, size_t index, ctui_rect r,
		      int y, int x, bool fill, bool show_weight)
{
	const ctui_palette *p = ctui_theme_palette();
	int color = p->icon[index % PALETTE_COLORS];
	const ctui_glyphs *g = ctui_theme_glyphs();
	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	/* only the part of the rectangle on the screen is visited */
	int ry0 = clip_lo(y + r.y, 0), ry1 = y + r.y + r.height;
	int rx0 = clip_lo(x + r.x, 0), rx1 = x + r.x + r.width;
	if (ry1 > rows)
		ry1 = rows;
	if (rx1 > cols)
		rx1 = cols;
	if (ry0 >= ry1 || rx0 >= rx1)
		return;
	bool big = r.width >= 3 && r.height >= 3;
	int attr_fill =
	    ctui_attr_icon((ctui_icon_color)(index % PALETTE_COLORS), false);
	int text = ctui_attr(CTUI_PAIR_BODY, true);
	if (fill) {
		int bg = ctui_attr_color(p->black, color);
		for (int row = ry0; row < ry1; row++) {
			int gap_row = big && row == y + r.y + r.height - 1;
			for (int col = rx0; col < rx1; col++) {
				bool gap =
				    gap_row ||
				    (big && col == x + r.x + r.width - 1);
				ctui_put(row, col, 1, " ", gap ? 0 : bg);
			}
		}
		text = bg;
	} else if (!big) {
		const char *ch = (index & 1U) ? "+" : "#";
		for (int row = ry0; row < ry1; row++)
			for (int col = rx0; col < rx1; col++)
				ctui_put(row, col, 1, ch, attr_fill);
	} else {
		int bx = x + r.x, by = y + r.y, bw = r.width, bh = r.height;
		for (int col = 1; col < bw - 1; col++) {
			ctui_put(by, bx + col, 1, g->box_h, attr_fill);
			ctui_put(by + bh - 1, bx + col, 1, g->box_h, attr_fill);
		}
		for (int row = 1; row < bh - 1; row++) {
			ctui_put(by + row, bx, 1, g->box_v, attr_fill);
			ctui_put(by + row, bx + bw - 1, 1, g->box_v, attr_fill);
		}
		ctui_put(by, bx, 1, g->box_tl, attr_fill);
		ctui_put(by, bx + bw - 1, 1, g->box_tr, attr_fill);
		ctui_put(by + bh - 1, bx, 1, g->box_bl, attr_fill);
		ctui_put(by + bh - 1, bx + bw - 1, 1, g->box_br, attr_fill);
		text = ctui_attr(CTUI_PAIR_BODY, true);
	}

	/* label and weight, inside the gap or the border when the rectangle is
	 * big */
	if (r.width >= 3) {
		int pad = big ? 1 : 0, room = r.width - 2 * pad;
		int row = y + r.y + pad;
		if (it->label && row >= 0 && row < rows)
			ctui_put(row, x + r.x + pad, room, it->label, text);
		if (show_weight && big && r.height >= 4 && row + 1 < rows) {
			char w[32];
			snprintf(w, sizeof w, "%.4g", it->weight);
			ctui_put(row + 1, x + r.x + pad, room, w, text);
		}
	}
}

void ctui_treemap_draw(const ctui_treemap *t, int y, int x, int width,
		       int height)
{
	if (!t->count || width <= 0 || height <= 0)
		return;
	ctui_rect *r = malloc(t->count * sizeof *r);
	if (!r)
		return;
	ctui_style_begin(t->style);
	ctui_treemap_squarify(t->items, t->count,
			      (ctui_rect){.height = height, .width = width}, r);
	bool fill = ctui_theme_unicode() && ctui_theme_has_colors();
	for (size_t i = 0; i < t->count; i++)
		if (r[i].width > 0)
			draw_item(&t->items[i], i, r[i], y, x, fill,
				  t->show_weights);
	ctui_style_end();
	free(r);
}
