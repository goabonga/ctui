/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * treemap.h - squarified treemap of weighted items.
 *
 * Each item gets a rectangle whose area is proportional to its weight,
 * laid out with the squarified algorithm (Bruls, Huizing, van Wijk) so the
 * rectangles stay close to squares. A terminal cell is about twice as tall
 * as wide, and the layout accounts for it. Items with a weight that is zero,
 * negative, NaN or infinite are ignored and get an empty rectangle. The
 * layout is deterministic: items are placed by decreasing weight, equal
 * weights in their order in the array, and the rectangles tile the area
 * exactly (an item too light for one cell gets an empty rectangle).
 *
 * Items take the colours of the icon palette in turn, by their position in
 * the array. With colours and UTF-8 a rectangle is filled with its colour,
 * leaving a gap on its right and bottom edges when it is large enough; else
 * it is drawn as a box (with the ASCII glyphs of the theme in ASCII
 * locales). The label is clipped to the rectangle, and the weight is shown
 * under it when there is room. Rectangles of one or two cells are filled.
 *
 * The caller owns the items and their labels.
 */
#ifndef CTUI_TREEMAP_H
#define CTUI_TREEMAP_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/theme.h>

typedef struct ctui_treemap_item {
	const char *label; /* may be NULL */
	double weight;
} ctui_treemap_item;

typedef struct ctui_treemap {
	const ctui_treemap_item *items;
	size_t count;
	bool show_weights;	 /* the weight under the label, when it fits */
	const ctui_style *style; /* may be NULL; set after init */
} ctui_treemap;

/* Weights shown, no style. */
void ctui_treemap_init(ctui_treemap *t, const ctui_treemap_item *items,
		       size_t count);

/* Whether an item takes part in the layout: its weight is finite and > 0. */
bool ctui_treemap_weight_ok(double weight);

/* Lay the items out in `area`: out[i] is the rectangle of items[i], with
 * the position of the area and a zero size for an ignored item. Returns the
 * number of items with a non-empty rectangle. `out` has `count` entries.
 * Nothing is laid out (0) when the area is empty or does not fit in int. */
size_t ctui_treemap_squarify(const ctui_treemap_item *items, size_t count,
			     ctui_rect area, ctui_rect *out);

/* The item under cell (y, x) of a treemap of width x height cells, or -1. */
long ctui_treemap_hit(const ctui_treemap *t, int width, int height, int y,
		      int x);

void ctui_treemap_draw(const ctui_treemap *t, int y, int x, int width,
		       int height);

#endif /* CTUI_TREEMAP_H */
