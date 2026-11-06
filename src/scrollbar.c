/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * scrollbar.c - thumb geometry, pointer handling and drawing.
 */
#include <ctui/scrollbar.h>

#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

static size_t max_offset(const ctui_scrollbar *bar)
{
	return bar->total > bar->visible ? bar->total - bar->visible : 0;
}

static size_t clamp_offset(const ctui_scrollbar *bar, size_t offset)
{
	size_t max = max_offset(bar);
	return offset > max ? max : offset;
}

void ctui_scrollbar_set(ctui_scrollbar *bar, size_t total, size_t visible,
			size_t offset)
{
	bar->total = total;
	bar->visible = visible;
	bar->offset = clamp_offset(bar, offset);
}

void ctui_scrollbar_place(ctui_scrollbar *bar, int y, int x, int height)
{
	bar->y = y;
	bar->x = x;
	bar->height = height;
}

void ctui_scrollbar_place_horizontal(ctui_scrollbar *bar, int y, int x,
				     int width)
{
	ctui_scrollbar_place(bar, y, x, width);
	bar->horizontal = true;
}

bool ctui_scrollbar_thumb(size_t total, size_t visible, size_t offset,
			  int track, int *start, int *length)
{
	if (track < 1 || visible == 0 || total <= visible)
		return false;
	size_t t = (size_t)track;
	size_t len = (t * visible + total / 2) / total;
	if (len < 1)
		len = 1;
	if (len > t)
		len = t;
	size_t range = t - len, max = total - visible;
	if (offset > max)
		offset = max;
	size_t pos = range == 0 ? 0 : (offset * range + max / 2) / max;
	if (start)
		*start = (int)pos;
	if (length)
		*length = (int)len;
	return true;
}

bool ctui_scrollbar_needed(const ctui_scrollbar *bar)
{
	return ctui_scrollbar_thumb(bar->total, bar->visible, bar->offset,
				    bar->height, NULL, NULL);
}

bool ctui_scrollbar_over(const ctui_scrollbar *bar, int y, int x)
{
	if (!ctui_scrollbar_needed(bar))
		return false;
	if (bar->horizontal)
		return y == bar->y && x >= bar->x && x < bar->x + bar->height;
	return x == bar->x && y >= bar->y && y < bar->y + bar->height;
}

/* Offset for a thumb whose first row is `start`. */
static size_t offset_at(const ctui_scrollbar *bar, int length, int start)
{
	size_t range = (size_t)(bar->height - length), max = max_offset(bar);
	if (range == 0 || start <= 0)
		return 0;
	if ((size_t)start >= range)
		return max;
	return ((size_t)start * max + range / 2) / range;
}

bool ctui_scrollbar_press(ctui_scrollbar *bar, int y, int x)
{
	int start, length;
	if (!ctui_scrollbar_over(bar, y, x) ||
	    !ctui_scrollbar_thumb(bar->total, bar->visible, bar->offset,
				  bar->height, &start, &length))
		return false;
	int row = bar->horizontal ? x - bar->x : y - bar->y;
	if (row >= start && row < start + length) {
		bar->dragging = true;
		bar->grab = row - start;
	} else if (row < start) {
		bar->offset =
		    bar->offset > bar->visible ? bar->offset - bar->visible : 0;
	} else {
		bar->offset = clamp_offset(bar, bar->offset + bar->visible);
	}
	return true;
}

bool ctui_scrollbar_drag(ctui_scrollbar *bar, int pos)
{
	int start, length;
	if (!bar->dragging ||
	    !ctui_scrollbar_thumb(bar->total, bar->visible, bar->offset,
				  bar->height, &start, &length))
		return false;
	int origin = bar->horizontal ? bar->x : bar->y;
	size_t offset = offset_at(bar, length, pos - origin - bar->grab);
	bool changed = offset != bar->offset;
	bar->offset = offset;
	return changed;
}

bool ctui_scrollbar_release(ctui_scrollbar *bar)
{
	bool was = bar->dragging;
	bar->dragging = false;
	return was;
}

bool ctui_scrollbar_wheel(ctui_scrollbar *bar, int rows)
{
	size_t offset;
	if (rows < 0)
		offset = bar->offset > (size_t)-rows
			     ? bar->offset - (size_t)-rows
			     : 0;
	else
		offset = clamp_offset(bar, bar->offset + (size_t)rows);
	bool changed = offset != bar->offset;
	bar->offset = offset;
	return changed;
}

const char *ctui_scrollbar_glyph(const ctui_scrollbar *bar, bool thumb)
{
	const char *own = thumb ? bar->thumb_glyph : bar->track_glyph;
	if (own)
		return own;
	const ctui_glyphs *g = ctui_theme_glyphs();
	if (!thumb && bar->horizontal)
		return g->box_h;
	/* A cell is about twice as tall as wide: the full block that makes a
	 * vertical thumb one cell wide would make a horizontal one twice as
	 * thick, so the horizontal thumb is the lower half block. */
	if (thumb && bar->horizontal && g->unicode &&
	    strcmp(g->scroll_thumb, "\xe2\x96\x88") == 0)
		return "\xe2\x96\x84";
	return thumb ? g->scroll_thumb : g->scroll_track;
}

bool ctui_scrollbar_draw(const ctui_scrollbar *bar, bool dialog)
{
	int start, length;
	if (!ctui_scrollbar_thumb(bar->total, bar->visible, bar->offset,
				  bar->height, &start, &length))
		return false;
	ctui_pair pair = dialog ? CTUI_PAIR_DIALOG_BORDER : CTUI_PAIR_BORDER;
	int track_attr =
	    bar->track_attr ? bar->track_attr : ctui_attr(pair, false);
	int thumb_attr = bar->thumb_attr
			     ? bar->thumb_attr
			     : ctui_attr(dialog ? CTUI_PAIR_DIALOG
						: CTUI_PAIR_BORDER_SELECTED,
					 true);
	for (int r = 0; r < bar->height; r++) {
		bool thumb = r >= start && r < start + length;
		const char *glyph = ctui_scrollbar_glyph(bar, thumb);
		if (bar->horizontal)
			ctui_put(bar->y, bar->x + r, 1, glyph,
				 thumb ? thumb_attr : track_attr);
		else
			ctui_put(bar->y + r, bar->x, 1, glyph,
				 thumb ? thumb_attr : track_attr);
	}
	return true;
}
