/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * palette.c - command palette overlay (palette.h).
 */
#include <ctui/palette.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define NEG (INT_MIN / 2)

#define SCORE_CHAR 10
#define SCORE_WORD 8
#define SCORE_CONSECUTIVE 15
#define SCORE_CASE 1
#define SCORE_FIRST 4
#define LEADING_MAX 3
#define WHEEL_STEP 3

static unsigned long fold(unsigned long cp)
{
	return cp >= 'A' && cp <= 'Z' ? cp + ('a' - 'A') : cp;
}

static bool is_upper(unsigned long cp)
{
	return cp >= 'A' && cp <= 'Z';
}
static bool is_lower(unsigned long cp)
{
	return cp >= 'a' && cp <= 'z';
}

/* A separator: ASCII that is neither a letter nor a digit. */
static bool is_separator(unsigned long cp)
{
	return cp < 128 && !is_upper(cp) && !is_lower(cp) &&
	       !(cp >= '0' && cp <= '9');
}

/* The query as folded and raw code points, spaces dropped. */
static size_t query_chars(const char *query, unsigned long *folded,
			  unsigned long *raw)
{
	size_t m = 0;
	for (const char *s = query; s && *s && m < CTUI_PALETTE_QUERY_MAX;) {
		unsigned long cp;
		s += ctui_utf8_decode(s, &cp);
		if (cp == ' ')
			continue;
		if (raw)
			raw[m] = cp;
		if (folded)
			folded[m] = fold(cp);
		m++;
	}
	return m;
}

int ctui_palette_score(const char *query, const char *text,
		       unsigned short *positions, size_t max_positions)
{
	unsigned long qf[CTUI_PALETTE_QUERY_MAX], qr[CTUI_PALETTE_QUERY_MAX];
	size_t m = query_chars(query, qf, qr);
	if (m == 0)
		return 0;

	unsigned long tf[CTUI_PALETTE_TEXT_MAX], tr[CTUI_PALETTE_TEXT_MAX];
	unsigned short tb[CTUI_PALETTE_TEXT_MAX];
	size_t n = 0;
	for (const char *s = text; s && *s && n < CTUI_PALETTE_TEXT_MAX;) {
		unsigned long cp;
		tb[n] = (unsigned short)(s - text);
		s += ctui_utf8_decode(s, &cp);
		tr[n] = cp;
		tf[n++] = fold(cp);
	}

	/* Cheap rejection: is the query a subsequence at all? */
	size_t j = 0;
	for (size_t i = 0; i < n && j < m; i++)
		if (tf[i] == qf[j])
			j++;
	if (j < m)
		return CTUI_PALETTE_NO_MATCH;

	/* sc[j][i]: best score with query[j] matched on text[i]; par[j][i]:
	 * where query[j-1] sat in that alignment. */
	int sc[CTUI_PALETTE_QUERY_MAX][CTUI_PALETTE_TEXT_MAX];
	short par[CTUI_PALETTE_QUERY_MAX][CTUI_PALETTE_TEXT_MAX];
	for (j = 0; j < m; j++) {
		int bestv = NEG; /* max over k <= i - 2 of sc[j-1][k] + k */
		short bestk = -1;
		for (size_t i = 0; i < n; i++) {
			sc[j][i] = NEG;
			par[j][i] = -1;
			if (j > 0 && i >= 2 && sc[j - 1][i - 2] > NEG &&
			    sc[j - 1][i - 2] + (int)(i - 2) > bestv) {
				bestv = sc[j - 1][i - 2] + (int)(i - 2);
				bestk = (short)(i - 2);
			}
			if (tf[i] != qf[j])
				continue;
			int base =
			    SCORE_CHAR + (tr[i] == qr[j] ? SCORE_CASE : 0);
			if (i == 0)
				base += SCORE_FIRST + SCORE_WORD;
			else if (is_separator(tr[i - 1]) ||
				 (is_lower(tr[i - 1]) && is_upper(tr[i])))
				base += SCORE_WORD;
			if (j == 0) {
				sc[0][i] =
				    base -
				    (i < LEADING_MAX ? (int)i : LEADING_MAX);
				continue;
			}
			int best = NEG;
			short from = -1;
			if (i >= 1 && sc[j - 1][i - 1] > NEG) {
				best = sc[j - 1][i - 1] + SCORE_CONSECUTIVE;
				from = (short)(i - 1);
			}
			if (bestk >= 0 && bestv - (int)(i - 1) > best) {
				best = bestv - (int)(i - 1);
				from = bestk;
			}
			if (from >= 0) {
				sc[j][i] = base + best;
				par[j][i] = from;
			}
		}
	}
	int top = NEG;
	short at = -1;
	for (size_t i = 0; i < n; i++)
		if (sc[m - 1][i] > top) {
			top = sc[m - 1][i];
			at = (short)i;
		}
	if (at < 0)
		return CTUI_PALETTE_NO_MATCH;
	if (positions) {
		size_t i = (size_t)at;
		for (size_t jj = m; jj-- > 0;) {
			if (jj < max_positions)
				positions[jj] = tb[i];
			if (jj > 0)
				i = (size_t)par[jj][i];
		}
	}
	return top > 0 ? top : 0;
}

static int hit_cmp(const void *a, const void *b)
{
	const ctui_palette_hit *x = a, *y = b;
	if (x->score != y->score)
		return x->score > y->score ? -1 : 1;
	return x->index < y->index ? -1 : x->index > y->index;
}

size_t ctui_palette_filter(const ctui_command *commands, size_t count,
			   const char *query, ctui_palette_hit *out, size_t cap)
{
	size_t m = query_chars(query, NULL, NULL), n = 0;
	for (size_t i = 0; i < count && n < cap; i++) {
		ctui_palette_hit *h = &out[n];
		h->score = ctui_palette_score(
		    query, commands[i].name ? commands[i].name : "", h->pos,
		    CTUI_PALETTE_QUERY_MAX);
		if (h->score == CTUI_PALETTE_NO_MATCH)
			continue;
		h->index = i;
		h->count = (unsigned short)m;
		n++;
	}
	if (n > 1)
		qsort(out, n, sizeof out[0], hit_cmp);
	return n;
}

/* ---- dialog ------------------------------------------------------ */

/* Whether the query ends on a complete UTF-8 character. */
static bool query_complete(const ctui_cmdpal *p)
{
	size_t i = p->used;
	while (i > 0 &&
	       ((unsigned)(unsigned char)p->query[i - 1] & 0xC0U) == 0x80U)
		i--;
	if (i == 0)
		return true;
	unsigned char lead = (unsigned char)p->query[i - 1];
	size_t need = lead >= 0xF0   ? 4
		      : lead >= 0xE0 ? 3
		      : lead >= 0xC0 ? 2
				     : 1;
	return p->used - (i - 1) >= need;
}

static void refilter(ctui_cmdpal *p)
{
	p->matches = ctui_palette_filter(p->commands, p->count, p->query,
					 p->hits, p->hit_cap);
	p->selected = 0;
	p->first = 0;
}

void ctui_cmdpal_init(ctui_cmdpal *p, const ctui_command *commands,
		      size_t count, ctui_palette_hit *hits, size_t hit_cap)
{
	*p = (ctui_cmdpal){.commands = commands,
			   .count = count,
			   .hits = hits,
			   .hit_cap = hit_cap};
	refilter(p);
}

void ctui_cmdpal_set_query(ctui_cmdpal *p, const char *query)
{
	snprintf(p->query, sizeof p->query, "%s", query ? query : "");
	p->used = strlen(p->query);
	refilter(p);
}

size_t ctui_cmdpal_chosen(const ctui_cmdpal *p)
{
	return p->matches ? p->hits[p->selected].index : (size_t)-1;
}

bool ctui_cmdpal_geometry(const ctui_cmdpal *p, int lines, int cols,
			  ctui_rect *box, int *rows)
{
	(void)p;
	int height = lines - 4 < 18 ? lines - 4 : 18;
	if (!ctui_dialog_box(lines, cols, cols - 4 < 76 ? cols - 4 : 76, height,
			     box) ||
	    box->height < 5 || box->width < 20)
		return false;
	*rows = box->height - 4;
	return true;
}

/* First visible hit so that the selection shows in a window of rows. */
static size_t window_first(const ctui_cmdpal *p, int rows)
{
	size_t first = p->first, r = (size_t)rows;
	if (p->selected < first)
		first = p->selected;
	else if (p->selected >= first + r)
		first = p->selected - r + 1;
	return first;
}

ctui_dialog_result ctui_cmdpal_key(ctui_cmdpal *p, int key, int lines, int cols)
{
	ctui_rect box;
	int rows;
	if (!ctui_cmdpal_geometry(p, lines, cols, &box, &rows))
		rows = 1;
	size_t page = rows > 1 ? (size_t)rows - 1 : 1;
	switch (key) {
	case 27:
	case 3:
		return CTUI_DIALOG_CANCELLED;
	case '\n':
	case KEY_ENTER:
		return p->matches ? CTUI_DIALOG_ACCEPTED : CTUI_DIALOG_OPEN;
	case KEY_UP:
	case 16: /* Ctrl-P */
		if (p->matches)
			p->selected =
			    p->selected ? p->selected - 1 : p->matches - 1;
		break;
	case KEY_DOWN:
	case 14: /* Ctrl-N */
		if (p->matches)
			p->selected =
			    p->selected + 1 < p->matches ? p->selected + 1 : 0;
		break;
	case KEY_PPAGE:
		p->selected = p->selected > page ? p->selected - page : 0;
		break;
	case KEY_NPAGE:
		if (p->matches)
			p->selected = p->selected + page < p->matches
					  ? p->selected + page
					  : p->matches - 1;
		break;
	case KEY_HOME:
		p->selected = 0;
		break;
	case KEY_END:
		p->selected = p->matches ? p->matches - 1 : 0;
		break;
	case 21: /* Ctrl-U */
		p->used = 0;
		p->query[0] = '\0';
		refilter(p);
		break;
	case KEY_BACKSPACE:
	case 127:
	case 8:
		if (p->used) {
			do
				p->used--;
			while (p->used &&
			       ((unsigned)(unsigned char)p->query[p->used] &
				0xC0U) == 0x80U);
			p->query[p->used] = '\0';
			refilter(p);
		}
		break;
	default:
		if (key >= 32 && key <= 255 && key != 127 &&
		    p->used + 1 < sizeof p->query) {
			p->query[p->used++] = (char)key;
			p->query[p->used] = '\0';
			if (query_complete(p))
				refilter(p);
		}
		break;
	}
	p->first = window_first(p, rows);
	return CTUI_DIALOG_OPEN;
}

ctui_dialog_result ctui_cmdpal_click(ctui_cmdpal *p, int lines, int cols, int y,
				     int x)
{
	ctui_rect box;
	int rows;
	if (!ctui_cmdpal_geometry(p, lines, cols, &box, &rows))
		return CTUI_DIALOG_OPEN;
	if (ctui_frame_close_hit(box.y, box.x, box.width, y, x))
		return CTUI_DIALOG_CANCELLED;
	if (y < box.y + 3 || y >= box.y + 3 + rows || x <= box.x ||
	    x >= box.x + box.width - 1)
		return CTUI_DIALOG_OPEN;
	size_t first = window_first(p, rows),
	       i = first + (size_t)(y - box.y - 3);
	if (i >= p->matches)
		return CTUI_DIALOG_OPEN;
	p->first = first;
	p->selected = i;
	return CTUI_DIALOG_ACCEPTED;
}

void ctui_cmdpal_wheel(ctui_cmdpal *p, int lines, int cols, int dir)
{
	ctui_rect box;
	int rows;
	if (!ctui_cmdpal_geometry(p, lines, cols, &box, &rows) || dir == 0)
		return;
	size_t first = window_first(p, rows);
	size_t max = p->matches > (size_t)rows ? p->matches - (size_t)rows : 0;
	if (dir > 0)
		first = first + WHEEL_STEP < max ? first + WHEEL_STEP : max;
	else
		first = first > WHEEL_STEP ? first - WHEEL_STEP : 0;
	p->first = first;
	if (p->selected < first)
		p->selected = first;
	else if (p->selected >= first + (size_t)rows)
		p->selected = first + (size_t)rows - 1;
	if (p->matches && p->selected >= p->matches)
		p->selected = p->matches - 1;
}

static bool is_matched(const ctui_palette_hit *h, size_t offset)
{
	for (size_t k = 0; k < h->count && k < CTUI_PALETTE_QUERY_MAX; k++)
		if (h->pos[k] == offset)
			return true;
	return false;
}

/* The name with its matched characters emphasised, in at most avail
 * columns. Returns the columns used. */
static int name_draw(int y, int x, int avail, const char *name,
		     const ctui_palette_hit *h, int attr)
{
	int used = 0;
	for (const char *s = name; s && *s && used < avail;) {
		unsigned long cp;
		size_t len = ctui_utf8_decode(s, &cp);
		int w = ctui_codepoint_width(cp);
		if (used + w > avail)
			break;
		char one[8];
		memcpy(one, s, len);
		one[len] = '\0';
		int a = is_matched(h, (size_t)(s - name))
			    ? (int)((unsigned)attr | (unsigned)A_BOLD |
				    (unsigned)A_UNDERLINE)
			    : attr;
		ctui_put(y, x + used, avail - used, one, a);
		used += w;
		s += len;
	}
	return used;
}

static void cmdpal_draw_impl(const ctui_cmdpal *p)
{
	ctui_rect box;
	int rows;
	if (!ctui_cmdpal_geometry(p, LINES, COLS, &box, &rows))
		return;
	ctui_labels l = ctui_labels_resolve(p->labels);
	ctui_frame_draw(box.y, box.x, box.height, box.width, true, true,
			l.palette_title);
	ctui_frame_close_draw(box.y, box.x, box.width, true);
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false),
	    dim = (int)((unsigned)dialog | (unsigned)A_DIM);
	int inner_x = box.x + 2, inner_w = box.width - 4;

	/* input line: a prompt mark and the tail of the query */
	int row = box.y + 1;
	ctui_put(row, inner_x, 1, ">", ctui_attr(CTUI_PAIR_DIALOG, true));
	int field = inner_w - 2 > 1 ? inner_w - 2 : 1;
	const char *shown = p->query;
	size_t cols = ctui_utf8_columns(shown);
	while ((int)cols >= field && *shown) {
		shown++;
		while (((unsigned)(unsigned char)*shown & 0xC0U) == 0x80U)
			shown++;
		cols = ctui_utf8_columns(shown);
	}
	ctui_put(row, inner_x + 2, field, shown, dialog);
	ctui_separator(box.y + 2, box.x + 1, box.width - 2,
		       ctui_attr(CTUI_PAIR_DIALOG_BORDER, false));

	size_t first = window_first(p, rows);
	if (p->matches == 0)
		ctui_put(box.y + 3, inner_x, inner_w, l.palette_empty, dim);
	for (int r = 0; r < rows && first + (size_t)r < p->matches; r++) {
		size_t k = first + (size_t)r;
		const ctui_palette_hit *h = &p->hits[k];
		const ctui_command *c = &p->commands[h->index];
		bool on = k == p->selected;
		int attr = on ? ctui_attr(CTUI_PAIR_SELECTION, true) : dialog;
		int sub = on ? attr : dim;
		int y = box.y + 3 + r, avail = inner_w;
		ctui_fill(y, box.x + 1, box.width - 2, attr);
		if (c->shortcut && *c->shortcut) {
			int sw = (int)ctui_utf8_columns(c->shortcut);
			if (sw + 4 < avail) {
				ctui_put(y, inner_x + inner_w - sw, sw,
					 c->shortcut, sub);
				avail -= sw + 2;
			}
		}
		int used = name_draw(y, inner_x, avail, c->name, h, attr);
		if (c->description && *c->description && avail - used > 6)
			ctui_put(y, inner_x + used + 2, avail - used - 2,
				 c->description, sub);
	}
	if (p->matches > (size_t)rows && box.width > 20) {
		char pos[48];
		snprintf(pos, sizeof pos, " %zu/%zu ", p->selected + 1,
			 p->matches);
		ctui_put(box.y + box.height - 1,
			 box.x + box.width - 2 - (int)strlen(pos),
			 (int)strlen(pos), pos,
			 ctui_attr(CTUI_PAIR_DIALOG_BORDER, false));
	}
	move(row, inner_x + 2 + (int)cols);
}

void ctui_cmdpal_draw(const ctui_cmdpal *p)
{
	ctui_style_begin(p->style);
	cmdpal_draw_impl(p);
	ctui_style_end();
}
