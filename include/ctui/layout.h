/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * layout.h - split a rectangle into child rectangles from constraints.
 *
 * Pure functions, nothing is drawn. ctui_layout_split() cuts an ctui_rect
 * along one direction (children side by side, or stacked) and returns one
 * rectangle per constraint, in order. A child keeps the whole extent of
 * the area across the direction. To nest, split a returned rectangle
 * again.
 *
 * Constraints:
 *   ctui_con_len(n)       exactly n cells
 *   ctui_con_min(n)       at least n cells; it also grows to take the space
 *                   that nothing else wants
 *   ctui_con_max(n)       at most n cells; it takes up to n and gives way
 *                   sooner than a length when space is short
 *   ctui_con_pct(p)       p percent (0..100) of the space to share
 *   ctui_con_ratio(a, b)  a / b of the space to share (b <= 0 gives 0 cells)
 *   ctui_con_fill(w)      a share of what is left, in proportion to the weight
 * w (w < 1 counts as 1)
 *
 * The space to share is the extent of the area, less the margin and the
 * spacing between children. It is solved in this order of priority, each
 * step taking from what the previous ones left:
 *   1. lengths, in order
 *   2. minimums and maximums, in order; a minimum that cannot get its
 *      whole size gets 0 cells rather than a smaller one
 *   3. percentages and ratios, in order, rounded down
 *   4. fills, by weight; the rounding remainder goes one cell at a time
 *      to the first fills
 * A constraint never takes more than what is left, so the sizes always
 * add up to at most the space to share, the rectangles follow each other
 * without overlapping, and no size is negative. What a constraint cannot
 * get is lost: the later (lower priority) ones shrink first, and a size
 * is cut, not moved, in the end.
 *
 * What is left once all of that is solved goes, in this order, to the
 * fills (all of it), else to the minimums (in equal parts), else to the
 * flex mode, which places the children in the free space. Sizes and
 * coordinates are computed in 64 bits and clamped, so huge values do not
 * overflow. A negative height, width or spacing counts as 0.
 */
#ifndef CTUI_LAYOUT_H
#define CTUI_LAYOUT_H

#include <stddef.h>

#include <ctui/dialog.h> /* ctui_rect */

typedef enum ctui_layout_dir {
	CTUI_LAYOUT_ROW = 0, /* children side by side: the width is split */
	CTUI_LAYOUT_COLUMN   /* children stacked: the height is split */
} ctui_layout_dir;

/* Where the children go when the free space is not taken by a fill or a
 * minimum. */
typedef enum ctui_layout_flex {
	CTUI_LAYOUT_START = 0,	   /* packed at the start */
	CTUI_LAYOUT_CENTER,	   /* packed in the middle */
	CTUI_LAYOUT_END,	   /* packed at the end */
	CTUI_LAYOUT_SPACE_BETWEEN, /* free space shared between the children */
	CTUI_LAYOUT_SPACE_AROUND,  /* free space shared around each child */
	CTUI_LAYOUT_STRETCH /* legacy: the last child takes the free space */
} ctui_layout_flex;

typedef enum ctui_constraint_kind {
	CTUI_CONSTRAINT_LENGTH = 0,
	CTUI_CONSTRAINT_MIN,
	CTUI_CONSTRAINT_MAX,
	CTUI_CONSTRAINT_PERCENT,
	CTUI_CONSTRAINT_RATIO,
	CTUI_CONSTRAINT_FILL
} ctui_constraint_kind;

typedef struct ctui_constraint {
	ctui_constraint_kind kind;
	int value; /* cells, percent, numerator or weight */
	int den;   /* denominator of a ratio */
} ctui_constraint;

ctui_constraint ctui_con_len(int cells);
ctui_constraint ctui_con_min(int cells);
ctui_constraint ctui_con_max(int cells);
ctui_constraint ctui_con_pct(int percent);
ctui_constraint ctui_con_ratio(int num, int den);
ctui_constraint ctui_con_fill(int weight);

/* Cells removed on each side of a rectangle. Negative values count as 0. */
typedef struct ctui_insets {
	int top, right, bottom, left;
} ctui_insets;

ctui_insets ctui_insets_all(int n);

typedef struct ctui_layout {
	ctui_layout_dir dir;
	ctui_layout_flex flex;
	int spacing; /* cells between two children (cut when it cannot fit) */
	ctui_insets margin; /* removed from the area before splitting */
} ctui_layout;

/* Split `area` into n rectangles written to out[0..n-1], one per
 * constraint. Returns n, or 0 when a pointer is NULL. A zero-size area
 * gives n empty rectangles. */
size_t ctui_layout_split(const ctui_layout *layout, const ctui_rect *area,
			 const ctui_constraint *cons, size_t n, ctui_rect *out);

/* `r` less the insets, never below 0 x 0 (the origin then stays inside r).
 * This is the padding helper. */
ctui_rect ctui_layout_shrink(ctui_rect r, ctui_insets insets);

/* A height x width rectangle centred in `outer` (rounded towards the
 * origin), cut to outer when larger. Compare ctui_dialog_box(), which also
 * keeps a margin on a screen. */
ctui_rect ctui_layout_center(ctui_rect outer, int height, int width);

#endif /* CTUI_LAYOUT_H */
