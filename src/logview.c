/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * logview.c - a log viewer with a bounded ring of records (logview.h).
 */
#include <ctui/logview.h>

#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define LEVEL_COLS 5  /* TRACE .. ERROR */
#define SOURCE_COLS 8 /* source tags are cut to this in the list */
#define TIME_COLS 9   /* "HH:MM:SS " */
#define PREFIX_COLS (LEVEL_COLS + 1 + SOURCE_COLS + 1)

bool ctui_logview_init(ctui_logview *v, size_t capacity)
{
	*v = (ctui_logview){.levels = (1U << (unsigned)CTUI_LOG_LEVELS) - 1U,
			    .follow = true};
	if (capacity > CTUI_LOG_CAPACITY_MAX)
		return false;
	if (capacity) {
		v->ring = calloc(capacity, sizeof *v->ring);
		if (!v->ring)
			return false;
	}
	v->capacity = capacity;
	return true;
}

static const ctui_log_record *slot(const ctui_logview *v, size_t i)
{
	return &v->ring[(v->head + i) % v->capacity];
}

void ctui_logview_clear(ctui_logview *v)
{
	for (size_t i = 0; i < v->count; i++)
		free(v->ring[(v->head + i) % v->capacity].message);
	v->head = v->count = 0;
	v->has_selection = false;
}

void ctui_logview_free(ctui_logview *v)
{
	ctui_logview_clear(v);
	free(v->ring);
	*v = (ctui_logview){0};
}

const ctui_log_record *ctui_logview_at(const ctui_logview *v, size_t i)
{
	return i < v->count ? slot(v, i) : NULL;
}

bool ctui_logview_push(ctui_logview *v, double time, ctui_log_level level,
		       const char *source, const char *message)
{
	if (v->capacity == 0) {
		v->dropped++;
		return false;
	}
	if (!message)
		message = "";
	size_t n = 0;
	while (n < CTUI_LOG_MESSAGE_MAX && message[n])
		n++;
	char *tmp = malloc(n + 1), *text = malloc(n + 1);
	if (!tmp || !text) {
		free(tmp);
		free(text);
		v->dropped++;
		return false;
	}
	for (size_t i = 0; i < n; i++)
		tmp[i] = message[i] == '\n' || message[i] == '\t' ||
				 message[i] == '\r'
			     ? ' '
			     : message[i];
	tmp[n] = '\0';
	ctui_sanitize(text, n + 1, tmp, true);
	free(tmp);

	ctui_log_record *r;
	if (v->count == v->capacity) {
		r = &v->ring[v->head];
		free(r->message);
		v->head = (v->head + 1) % v->capacity;
		v->dropped++;
	} else {
		r = &v->ring[(v->head + v->count) % v->capacity];
		v->count++;
	}
	r->time = time;
	int lv = (int)level;
	r->level = lv < CTUI_LOG_TRACE	 ? CTUI_LOG_TRACE
		   : lv > CTUI_LOG_ERROR ? CTUI_LOG_ERROR
					 : (ctui_log_level)lv;
	char src[CTUI_LOG_SOURCE_MAX * 4];
	size_t sn = 0;
	while (source && source[sn] && sn < sizeof src - 1) {
		src[sn] = source[sn] == '\n' || source[sn] == '\t' ||
				  source[sn] == '\r'
			      ? ' '
			      : source[sn];
		sn++;
	}
	src[sn] = '\0';
	ctui_sanitize(r->source, sizeof r->source, src, true);
	r->message = text;
	r->seq = v->next_seq++;
	return true;
}

void ctui_logview_set_level(ctui_logview *v, ctui_log_level level, bool on)
{
	if (level < CTUI_LOG_TRACE || level > CTUI_LOG_ERROR)
		return;
	if (on)
		v->levels |= 1u << level;
	else
		v->levels &= ~(1u << level);
}

bool ctui_logview_level_on(const ctui_logview *v, ctui_log_level level)
{
	return level >= CTUI_LOG_TRACE && level <= CTUI_LOG_ERROR &&
	       (v->levels >> level & 1u);
}

const char *ctui_log_level_name(ctui_log_level level)
{
	static const char *const names[CTUI_LOG_LEVELS] = {
	    "TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
	return level >= CTUI_LOG_TRACE && level <= CTUI_LOG_ERROR ? names[level]
								  : "?";
}

ctui_pair ctui_log_level_pair(ctui_log_level level)
{
	switch (level) {
	case CTUI_LOG_TRACE:
		return CTUI_PAIR_MUTED;
	case CTUI_LOG_DEBUG:
		return CTUI_PAIR_CURSOR;
	case CTUI_LOG_WARN:
		return CTUI_PAIR_GAUGE_YELLOW;
	case CTUI_LOG_ERROR:
		return CTUI_PAIR_ERROR;
	default:
		return CTUI_PAIR_BODY;
	}
}

void ctui_logview_set_filter(ctui_logview *v, const char *text)
{
	char raw[CTUI_LOG_FILTER_MAX * 2];
	size_t n = 0;
	while (text && text[n] && n < sizeof raw - 1) {
		raw[n] = text[n];
		n++;
	}
	raw[n] = '\0';
	ctui_sanitize(v->filter, sizeof v->filter, raw, true);
}

static bool is_cont(char c)
{
	return ((unsigned)(unsigned char)c & 0xC0U) == 0x80U;
}

size_t ctui_log_find(const char *hay, const char *needle, size_t from)
{
	size_t n = needle ? strlen(needle) : 0, len = hay ? strlen(hay) : 0;
	if (n == 0 || n > len)
		return SIZE_MAX;
	for (size_t i = from; i + n <= len; i++) {
		if (is_cont(hay[i]))
			continue;
		size_t k = 0;
		while (k < n) {
			unsigned char a = (unsigned char)hay[i + k],
				      b = (unsigned char)needle[k];
			if (a < 0x80)
				a = (unsigned char)tolower(a);
			if (b < 0x80)
				b = (unsigned char)tolower(b);
			if (a != b)
				break;
			k++;
		}
		if (k == n && (hay[i + n] == '\0' || !is_cont(hay[i + n])))
			return i;
	}
	return SIZE_MAX;
}

void ctui_log_format_time(double seconds, char *out, size_t cap)
{
	long long s = seconds > 0 && seconds < 9e15 ? (long long)seconds : 0;
	s %= 86400;
	snprintf(out, cap, "%02d:%02d:%02d", (int)(s / 3600),
		 (int)(s / 60 % 60), (int)(s % 60));
}

bool ctui_logview_matches(const ctui_logview *v, const ctui_log_record *r)
{
	if (!ctui_logview_level_on(v, r->level))
		return false;
	if (!v->filter[0])
		return true;
	return ctui_log_find(r->message, v->filter, 0) != SIZE_MAX ||
	       ctui_log_find(r->source, v->filter, 0) != SIZE_MAX;
}

size_t ctui_logview_visible(const ctui_logview *v)
{
	size_t n = 0;
	for (size_t i = 0; i < v->count; i++)
		n += ctui_logview_matches(v, slot(v, i));
	return n;
}

/* Ring index of the idx-th visible record; v->count when there is none. */
static size_t visible_pos(const ctui_logview *v, size_t idx)
{
	for (size_t i = 0; i < v->count; i++)
		if (ctui_logview_matches(v, slot(v, i)) && idx-- == 0)
			return i;
	return v->count;
}

const ctui_log_record *ctui_logview_visible_at(const ctui_logview *v, size_t i)
{
	size_t pos = visible_pos(v, i);
	return pos < v->count ? slot(v, pos) : NULL;
}

ctui_logview_layout ctui_logview_layout_for(const ctui_logview *v, int width)
{
	ctui_logview_layout l = {0};
	l.body = width > 1 ? width - 1 : 1;
	if (v->gutter == CTUI_LOG_GUTTER_TIME) {
		l.gutter = TIME_COLS;
	} else if (v->gutter == CTUI_LOG_GUTTER_NUMBER) {
		int digits = 1;
		for (size_t n = v->next_seq; n >= 10; n /= 10)
			digits++;
		l.gutter = digits + 1;
	}
	l.prefix = PREFIX_COLS;
	l.text = l.body - l.gutter - l.prefix;
	if (l.text < 1)
		l.text = 1;
	return l;
}

/* Bytes of the next row of a message wrapped to cols, and the step to the one
 * after. */
static size_t next_row(const char *msg, size_t off, int cols, size_t *step)
{
	size_t len = ctui_wrap_chunk(msg + off, cols);
	*step = ctui_wrap_skip(msg + off, len);
	return len;
}

size_t ctui_logview_row_count(const ctui_logview *v, const ctui_log_record *r,
			      int width)
{
	if (!v->wrap)
		return 1;
	int cols = ctui_logview_layout_for(v, width).text;
	size_t rows = 0, off = 0;
	do {
		size_t step;
		next_row(r->message, off, cols, &step);
		rows++;
		if (step == 0)
			break;
		off += step;
	} while (r->message[off]);
	return rows;
}

size_t ctui_logview_last_top(const ctui_logview *v, int width, int height)
{
	size_t nvis = ctui_logview_visible(v), idx = nvis, sum = 0;
	for (size_t i = v->count; i-- > 0 && idx > 0;) {
		const ctui_log_record *r = slot(v, i);
		if (!ctui_logview_matches(v, r))
			continue;
		size_t h = ctui_logview_row_count(v, r, width);
		if (idx != nvis && sum + h > (size_t)(height > 0 ? height : 0))
			break;
		sum += h;
		idx--;
	}
	return idx;
}

/* Number of visible records older than the anchor. */
static size_t anchor_index(const ctui_logview *v)
{
	size_t n = 0;
	for (size_t i = 0; i < v->count; i++) {
		const ctui_log_record *r = slot(v, i);
		if (r->seq >= v->top_seq)
			break;
		n += ctui_logview_matches(v, r);
	}
	return n;
}

size_t ctui_logview_top(const ctui_logview *v, int width, int height)
{
	size_t last = ctui_logview_last_top(v, width, height);
	if (v->follow)
		return last;
	size_t a = anchor_index(v);
	return a < last ? a : last;
}

static void set_anchor(ctui_logview *v, size_t idx)
{
	size_t pos = visible_pos(v, idx);
	v->top_seq = pos < v->count ? slot(v, pos)->seq : v->next_seq;
}

bool ctui_logview_hit(const ctui_logview *v, int oy, int ox, int width,
		      int height, int y, int x, size_t *index)
{
	ctui_logview_layout l = ctui_logview_layout_for(v, width);
	if (width < 2 || y < oy || y >= oy + height || x < ox ||
	    x >= ox + l.body)
		return false;
	size_t idx = ctui_logview_top(v, width, height), row = 0,
	       target = (size_t)(y - oy);
	for (size_t pos = visible_pos(v, idx); pos < v->count; pos++) {
		const ctui_log_record *r = slot(v, pos);
		if (!ctui_logview_matches(v, r))
			continue;
		size_t h = ctui_logview_row_count(v, r, width);
		if (target < row + h) {
			if (index)
				*index = idx;
			return true;
		}
		row += h;
		idx++;
	}
	return false;
}

static void freeze(ctui_logview *v, int width, int height)
{
	if (v->follow) {
		set_anchor(v, ctui_logview_top(v, width, height));
		v->follow = false;
	}
}

bool ctui_logview_scroll(ctui_logview *v, int width, int height, int delta)
{
	size_t cur = ctui_logview_top(v, width, height),
	       last = ctui_logview_last_top(v, width, height);
	size_t next = cur;
	if (delta < 0)
		next = (size_t)(-(long long)delta) > cur
			   ? 0
			   : cur - (size_t)(-(long long)delta);
	else if (delta > 0)
		next = (size_t)delta > last - cur ? last : cur + (size_t)delta;
	if (next == cur)
		return false;
	if (next < cur)
		freeze(v, width, height);
	if (!v->follow)
		set_anchor(v, next);
	return true;
}

void ctui_logview_set_follow(ctui_logview *v, bool follow)
{
	if (follow)
		v->follow = true;
	else if (v->width > 0)
		freeze(v, v->width, v->height);
	else
		v->follow = false;
}

const ctui_log_record *ctui_logview_selected(const ctui_logview *v)
{
	if (!v->has_selection || v->count == 0)
		return NULL;
	size_t oldest = v->next_seq - v->count;
	if (v->selected_seq < oldest || v->selected_seq >= v->next_seq)
		return NULL;
	return slot(v, v->selected_seq - oldest);
}

void ctui_logview_select(ctui_logview *v, const ctui_log_record *r)
{
	v->has_selection = r != NULL;
	if (r)
		v->selected_seq = r->seq;
}

bool ctui_logview_key(ctui_logview *v, int key, int width, int height)
{
	int page = height > 1 ? height - 1 : 1;
	switch (key) {
	case KEY_UP:
	case 'k':
		ctui_logview_scroll(v, width, height, -1);
		return true;
	case KEY_DOWN:
	case 'j':
		ctui_logview_scroll(v, width, height, 1);
		return true;
	case KEY_PPAGE:
		ctui_logview_scroll(v, width, height, -page);
		return true;
	case KEY_NPAGE:
		ctui_logview_scroll(v, width, height, page);
		return true;
	case KEY_HOME: {
		size_t top = ctui_logview_top(v, width, height);
		ctui_logview_scroll(v, width, height,
				    -(top > INT_MAX ? INT_MAX : (int)top));
		return true;
	}
	case KEY_END:
		v->follow = true;
		return true;
	case 'f':
		if (v->follow)
			freeze(v, width, height);
		else
			v->follow = true;
		return true;
	case 't':
	case 'd':
	case 'i':
	case 'w':
	case 'e': {
		ctui_log_level l = key == 't'	? CTUI_LOG_TRACE
				   : key == 'd' ? CTUI_LOG_DEBUG
				   : key == 'i' ? CTUI_LOG_INFO
				   : key == 'w' ? CTUI_LOG_WARN
						: CTUI_LOG_ERROR;
		ctui_logview_set_level(v, l, !ctui_logview_level_on(v, l));
		return true;
	}
	case 'r':
		v->wrap = !v->wrap;
		return true;
	case 'g':
		v->gutter = (ctui_log_gutter)((v->gutter + 1) % 3);
		return true;
	default:
		return false;
	}
}

static void apply_bar(ctui_logview *v)
{
	size_t cur = ctui_logview_top(v, v->width, v->height);
	size_t next = v->bar.offset;
	if (next < cur)
		freeze(v, v->width, v->height);
	if (!v->follow)
		set_anchor(v, next);
}

bool ctui_logview_press(ctui_logview *v, int y, int x)
{
	if (v->width < 2)
		return false;
	if (ctui_scrollbar_press(&v->bar, y, x)) {
		apply_bar(v);
		return true;
	}
	size_t idx;
	if (!ctui_logview_hit(v, v->oy, v->ox, v->width, v->height, y, x, &idx))
		return false;
	ctui_logview_select(v, ctui_logview_visible_at(v, idx));
	return true;
}

bool ctui_logview_drag(ctui_logview *v, int y)
{
	if (!v->bar.dragging)
		return false;
	if (ctui_scrollbar_drag(&v->bar, y))
		apply_bar(v);
	return true;
}

bool ctui_logview_release(ctui_logview *v)
{
	return ctui_scrollbar_release(&v->bar);
}

bool ctui_logview_wheel(ctui_logview *v, int rows)
{
	if (v->width < 2)
		return false;
	ctui_logview_scroll(v, v->width, v->height, rows);
	return true;
}

/* Draw bytes [a, b) of msg at (y, x + *used) within cols columns. */
static void put_bytes(int y, int x, int cols, int *used, const char *msg,
		      size_t a, size_t b, int attr)
{
	char buf[CTUI_TEXT_MAX];
	if (b <= a || *used >= cols)
		return;
	size_t n = b - a < sizeof buf - 1 ? b - a : sizeof buf - 1;
	memcpy(buf, msg + a, n);
	buf[n] = '\0';
	*used += ctui_put(y, x + *used, cols - *used, buf, attr);
}

/* Bytes [cs, ce) of the message in cols columns, matches highlighted;
 * returns the columns used. */
static int put_message(const ctui_logview *v, int y, int x, int cols,
		       const char *msg, size_t cs, size_t ce, int base, int hl)
{
	int used = 0;
	size_t pos = cs, flen = strlen(v->filter);
	if (flen) {
		for (size_t m = ctui_log_find(msg, v->filter, 0);
		     m != SIZE_MAX && m < ce;
		     m = ctui_log_find(msg, v->filter, m + flen)) {
			size_t me = m + flen;
			if (me <= cs)
				continue;
			size_t a = m > cs ? m : cs, b = me < ce ? me : ce;
			put_bytes(y, x, cols, &used, msg, pos, a, base);
			put_bytes(y, x, cols, &used, msg, a, b, hl);
			pos = b;
		}
	}
	put_bytes(y, x, cols, &used, msg, pos, ce, base);
	return used;
}

static void draw_record(const ctui_logview *v, const ctui_log_record *r,
			ctui_logview_layout l, int y, int x, int rows,
			int avail)
{
	bool sel = v->has_selection && v->selected_seq == r->seq;
	int base = ctui_attr(sel ? CTUI_PAIR_SELECTION : CTUI_PAIR_BODY, false);
	int muted = sel ? base : ctui_attr(CTUI_PAIR_MUTED, false);
	int hl = ctui_attr(CTUI_PAIR_PLAYHEAD, false);
	for (int k = 0; k < rows && k < avail; k++)
		ctui_fill(y + k, x, l.body, base);
	if (l.gutter) {
		char g[48];
		if (v->gutter == CTUI_LOG_GUTTER_TIME)
			ctui_log_format_time(r->time, g, sizeof g);
		else
			snprintf(g, sizeof g, "%*zu",
				 l.gutter - 1 > 20 ? 20 : l.gutter - 1,
				 r->seq + 1);
		ctui_put(y, x, l.gutter - 1, g, muted);
	}
	int px = x + l.gutter;
	char tag[LEVEL_COLS + 1];
	snprintf(tag, sizeof tag, "%-5s", ctui_log_level_name(r->level));
	ctui_put(y, px, l.body - l.gutter, tag,
		 sel ? base
		     : ctui_attr(ctui_log_level_pair(r->level),
				 r->level == CTUI_LOG_ERROR));
	char src[CTUI_LOG_SOURCE_MAX];
	size_t sb = ctui_utf8_prefix_bytes(r->source, SOURCE_COLS);
	memcpy(src, r->source, sb);
	src[sb] = '\0';
	ctui_put(y, px + LEVEL_COLS + 1, SOURCE_COLS, src, muted);

	if (l.body - l.gutter - l.prefix < 1)
		return;
	int mx = px + l.prefix, cols = l.text;
	if (!v->wrap) {
		if (ctui_utf8_columns(r->message) <= (size_t)cols) {
			put_message(v, y, mx, cols, r->message, 0,
				    strlen(r->message), base, hl);
		} else {
			const char *ell = ctui_theme_glyphs()->ellipsis;
			int ew = (int)ctui_utf8_columns(ell);
			size_t keep = cols > ew
					  ? ctui_utf8_prefix_bytes(
						r->message, (size_t)(cols - ew))
					  : 0;
			int used = put_message(v, y, mx, cols, r->message, 0,
					       keep, base, hl);
			if (cols > ew)
				ctui_put(y, mx + used, ew, ell, muted);
		}
		return;
	}
	size_t off = 0;
	for (int k = 0; k < rows && k < avail; k++) {
		size_t step, len = next_row(r->message, off, cols, &step);
		put_message(v, y + k, mx, cols, r->message, off, off + len,
			    base, hl);
		off += step;
		if (step == 0 || !r->message[off])
			break;
	}
}

void ctui_logview_draw(ctui_logview *v, int y, int x, int width, int height)
{
	if (width < 2 || height <= 0)
		return;
	ctui_style_begin(v->style);
	v->oy = y;
	v->ox = x;
	v->width = width;
	v->height = height;
	ctui_logview_layout l = ctui_logview_layout_for(v, width);
	for (int r = 0; r < height; r++)
		ctui_fill(y + r, x, width, ctui_attr(CTUI_PAIR_BODY, false));
	size_t nvis = ctui_logview_visible(v),
	       top = ctui_logview_top(v, width, height);
	size_t last = ctui_logview_last_top(v, width, height);
	int row = 0;
	for (size_t pos = visible_pos(v, top); pos < v->count && row < height;
	     pos++) {
		const ctui_log_record *r = slot(v, pos);
		if (!ctui_logview_matches(v, r))
			continue;
		int rows = (int)ctui_logview_row_count(v, r, width);
		draw_record(v, r, l, y + row, x, rows, height - row);
		row += rows;
	}
	ctui_scrollbar_set(&v->bar, nvis, nvis - last, top);
	ctui_scrollbar_place(&v->bar, y, x + width - 1, height);
	ctui_scrollbar_draw(&v->bar, false);
	ctui_style_end();
}
