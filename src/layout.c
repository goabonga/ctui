/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include <ctui/layout.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

ctui_constraint ctui_con_len(int cells)
{
	return (ctui_constraint){CTUI_CONSTRAINT_LENGTH, cells, 1};
}
ctui_constraint ctui_con_min(int cells)
{
	return (ctui_constraint){CTUI_CONSTRAINT_MIN, cells, 1};
}
ctui_constraint ctui_con_max(int cells)
{
	return (ctui_constraint){CTUI_CONSTRAINT_MAX, cells, 1};
}
ctui_constraint ctui_con_pct(int percent)
{
	return (ctui_constraint){CTUI_CONSTRAINT_PERCENT, percent, 1};
}
ctui_constraint ctui_con_ratio(int num, int den)
{
	return (ctui_constraint){CTUI_CONSTRAINT_RATIO, num, den};
}
ctui_constraint ctui_con_fill(int weight)
{
	return (ctui_constraint){CTUI_CONSTRAINT_FILL, weight, 1};
}

ctui_insets ctui_insets_all(int n)
{
	return (ctui_insets){n, n, n, n};
}

static int64_t pos64(int64_t v)
{
	return v > 0 ? v : 0;
}

/* The extent of a rectangle: not negative, and its far edge fits an int. */
static int64_t extent(int origin, int size)
{
	int64_t s = pos64(size);
	if (origin > 0 && s > (int64_t)INT_MAX - origin)
		s = (int64_t)INT_MAX - origin;
	return s;
}

static int clamp_int(int64_t v)
{
	return v > INT_MAX ? INT_MAX : v < INT_MIN ? INT_MIN : (int)v;
}

ctui_rect ctui_layout_shrink(ctui_rect r, ctui_insets in)
{
	int64_t h = extent(r.y, r.height), w = extent(r.x, r.width);
	int64_t t = pos64(in.top), b = pos64(in.bottom), l = pos64(in.left),
		rt = pos64(in.right);
	if (t > h)
		t = h;
	if (b > h - t)
		b = h - t;
	if (l > w)
		l = w;
	if (rt > w - l)
		rt = w - l;
	return (ctui_rect){clamp_int((int64_t)r.y + t),
			   clamp_int((int64_t)r.x + l), clamp_int(h - t - b),
			   clamp_int(w - l - rt)};
}

ctui_rect ctui_layout_center(ctui_rect outer, int height, int width)
{
	int64_t oh = extent(outer.y, outer.height),
		ow = extent(outer.x, outer.width);
	int64_t h = pos64(height), w = pos64(width);
	if (h > oh)
		h = oh;
	if (w > ow)
		w = ow;
	return (ctui_rect){clamp_int((int64_t)outer.y + (oh - h) / 2),
			   clamp_int((int64_t)outer.x + (ow - w) / 2),
			   clamp_int(h), clamp_int(w)};
}

static int64_t weight(const ctui_constraint *c)
{
	return c->value < 1 ? 1 : c->value;
}

static int64_t min64(int64_t a, int64_t b)
{
	return a < b ? a : b;
}

size_t ctui_layout_split(const ctui_layout *layout, const ctui_rect *area,
			 const ctui_constraint *cons, size_t n, ctui_rect *out)
{
	if (!layout || !area || !out || (n && !cons))
		return 0;
	bool row = layout->dir == CTUI_LAYOUT_ROW;
	ctui_rect in = ctui_layout_shrink(*area, layout->margin);
	int64_t len = row ? in.width : in.height;
	int64_t gap = pos64(layout->spacing);
	if (n > 1 && gap > len / (int64_t)(n - 1))
		gap = len / (int64_t)(n - 1);
	int64_t avail = len - (n > 1 ? gap * (int64_t)(n - 1) : 0), rem = avail;

#define SIZE(i) (*(row ? &out[i].width : &out[i].height))
	for (size_t i = 0; i < n; i++)
		SIZE(i) = 0;
	for (size_t i = 0; i < n; i++)
		if (cons[i].kind == CTUI_CONSTRAINT_LENGTH) {
			SIZE(i) = (int)min64(pos64(cons[i].value), rem);
			rem -= SIZE(i);
		}
	for (size_t i = 0; i < n; i++) {
		int64_t v = pos64(cons[i].value);
		if (cons[i].kind == CTUI_CONSTRAINT_MIN)
			SIZE(i) = v <= rem ? (int)v : 0;
		else if (cons[i].kind == CTUI_CONSTRAINT_MAX)
			SIZE(i) = (int)min64(v, rem);
		else
			continue;
		rem -= SIZE(i);
	}
	for (size_t i = 0; i < n; i++) {
		int64_t v;
		if (cons[i].kind == CTUI_CONSTRAINT_PERCENT) {
			v = avail * min64(pos64(cons[i].value), 100) / 100;
		} else if (cons[i].kind == CTUI_CONSTRAINT_RATIO) {
			v = cons[i].den <= 0
				? 0
				: avail *
				      min64(pos64(cons[i].value), cons[i].den) /
				      cons[i].den;
		} else {
			continue;
		}
		SIZE(i) = (int)min64(v, rem);
		rem -= SIZE(i);
	}

	int64_t total = 0, mins = 0;
	for (size_t i = 0; i < n; i++) {
		if (cons[i].kind == CTUI_CONSTRAINT_FILL)
			total += weight(&cons[i]);
		else if (cons[i].kind == CTUI_CONSTRAINT_MIN)
			mins++;
	}
	if (total > 0 && rem > 0) {
		int64_t given = 0;
		for (size_t i = 0; i < n; i++)
			if (cons[i].kind == CTUI_CONSTRAINT_FILL) {
				SIZE(i) = (int)(rem * weight(&cons[i]) / total);
				given += SIZE(i);
			}
		int64_t left = rem - given;
		for (size_t i = 0; i < n && left > 0; i++)
			if (cons[i].kind == CTUI_CONSTRAINT_FILL) {
				SIZE(i)++;
				left--;
			}
		rem = 0;
	} else if (total == 0 && mins > 0 && rem > 0) {
		int64_t q = rem / mins, r = rem % mins;
		for (size_t i = 0; i < n; i++)
			if (cons[i].kind == CTUI_CONSTRAINT_MIN) {
				SIZE(i) = (int)(SIZE(i) + q + (r > 0 ? 1 : 0));
				if (r > 0)
					r--;
			}
		rem = 0;
	}

	int64_t lead = 0, extra = 0, extra_r = 0;
	if (rem > 0 && n > 0) {
		switch (layout->flex) {
		case CTUI_LAYOUT_CENTER:
			lead = rem / 2;
			break;
		case CTUI_LAYOUT_END:
			lead = rem;
			break;
		case CTUI_LAYOUT_SPACE_BETWEEN:
			if (n > 1) {
				extra = rem / (int64_t)(n - 1);
				extra_r = rem % (int64_t)(n - 1);
			}
			break;
		case CTUI_LAYOUT_SPACE_AROUND:
			extra = rem / (int64_t)n;
			lead = extra / 2;
			break;
		case CTUI_LAYOUT_STRETCH:
			SIZE(n - 1) = (int)(SIZE(n - 1) + rem);
			break;
		default:
			break;
		}
	}

	int64_t at = (row ? in.x : in.y) + lead;
	for (size_t i = 0; i < n; i++) {
		int64_t size = SIZE(i);
		if (row) {
			out[i].x = clamp_int(at);
			out[i].y = in.y;
			out[i].height = in.height;
		} else {
			out[i].y = clamp_int(at);
			out[i].x = in.x;
			out[i].width = in.width;
		}
		at += size + gap + extra + ((int64_t)i < extra_r ? 1 : 0);
	}
#undef SIZE
	return n;
}
