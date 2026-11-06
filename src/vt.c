/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * vt.c - the terminal emulator state machine (see ctui/vt.h).
 */
#include <ctui/vt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/textview.h>

#define MAX_PARAMS 16
#define MAX_PARAM_VALUE 65535

typedef enum {
	ST_GROUND,
	ST_ESC,
	ST_ESC_INTER,
	ST_CSI,
	ST_CSI_INTER,
	ST_CSI_IGNORE,
	ST_OSC,
	ST_STRING,
	ST_STR_ESC
} vt_state;

typedef struct {
	int row, col;
	bool pending;
	uint32_t fg, bg;
	uint8_t attrs;
	bool origin;
	uint8_t g[2];
	int gl;
	bool valid;
} saved_cursor;

typedef struct {
	ctui_vt_cell **lines;
	int *lens;
	size_t cap, start, count, pushed;
} history;

struct ctui_vt {
	int rows, cols;
	ctui_vt_cell *buf[2];
	int cur; /* 0 primary, 1 alternate */
	int row, col;
	bool pending; /* deferred wrap */
	int top, bot; /* scroll region, inclusive */
	bool *tabs;
	ctui_vt_modes modes;
	uint32_t fg, bg;
	uint8_t attrs;
	uint8_t g[2]; /* 0 = ASCII, 1 = DEC special graphics */
	int gl;
	saved_cursor saved[2];
	/* parser */
	vt_state state;
	int params[MAX_PARAMS];
	bool sub[MAX_PARAMS];
	int pcount;
	bool pcur, next_sub, overflow;
	char priv, inter;
	bool str_osc;
	uint32_t u_cp, u_min;
	int u_need;
	char osc[CTUI_VT_OSC_MAX];
	size_t osc_len;
	char title[CTUI_VT_TITLE_MAX + 1];
	char resp[CTUI_VT_RESPONSE_MAX];
	size_t resp_len;
	history hist;
	unsigned long gen;
};

static const ctui_vt_cell BLANK = {.cp = 0, .width = 1};

/* DEC special graphics for 0x5f .. 0x7e. */
static const uint16_t GRAPHICS[32] = {
    0x00A0, 0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0,
    0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C,
    0x23BA, 0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534,
    0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7};

/* ---- cells and rows ---- */

static ctui_vt_cell *cell(const ctui_vt *vt, int r, int c)
{
	return &vt->buf[vt->cur][(size_t)r * (size_t)vt->cols + (size_t)c];
}

static ctui_vt_cell blank(const ctui_vt *vt)
{
	ctui_vt_cell c = BLANK;
	c.bg = vt->bg;
	return c;
}

static void fill(ctui_vt_cell *cells, size_t n, ctui_vt_cell with)
{
	for (size_t i = 0; i < n; i++)
		cells[i] = with;
}

/* Blank halves of wide characters whose other half is gone. */
static void fix_row(ctui_vt *vt, int r)
{
	for (int c = 0; c < vt->cols; c++) {
		ctui_vt_cell *p = cell(vt, r, c);
		if (p->width == 2 &&
		    (c + 1 >= vt->cols || cell(vt, r, c + 1)->width != 0)) {
			ctui_vt_cell b = BLANK;
			b.bg = p->bg;
			*p = b;
		} else if (p->width == 0 &&
			   (c == 0 || cell(vt, r, c - 1)->width != 2)) {
			ctui_vt_cell b = BLANK;
			b.bg = p->bg;
			*p = b;
		}
	}
}

/* Before overwriting cell c: break the wide character it belongs to. */
static void unwide(ctui_vt *vt, int r, int c)
{
	ctui_vt_cell *p = cell(vt, r, c);
	if (p->width == 2 && c + 1 < vt->cols)
		*cell(vt, r, c + 1) = blank(vt);
	else if (p->width == 0 && c > 0)
		*cell(vt, r, c - 1) = blank(vt);
}

static void clear_cells(ctui_vt *vt, int r, int c0, int c1)
{
	if (c0 < 0)
		c0 = 0;
	if (c1 > vt->cols)
		c1 = vt->cols;
	if (c0 >= c1)
		return;
	fill(cell(vt, r, c0), (size_t)(c1 - c0), blank(vt));
	fix_row(vt, r);
}

/* ---- history ---- */

static void history_free_lines(history *h)
{
	for (size_t i = 0; i < h->cap; i++)
		free(h->lines[i]);
}

static void history_clear(history *h)
{
	history_free_lines(h);
	if (h->cap) {
		memset((void *)h->lines, 0, h->cap * sizeof *h->lines);
		memset(h->lens, 0, h->cap * sizeof *h->lens);
	}
	h->start = h->count = 0;
}

static bool is_blank(const ctui_vt_cell *c)
{
	return c->cp == 0 && c->bg == 0 && c->attrs == 0 && c->width == 1;
}

static void history_push(ctui_vt *vt, const ctui_vt_cell *row)
{
	history *h = &vt->hist;
	if (h->cap == 0)
		return;
	int len = vt->cols;
	while (len > 0 && is_blank(&row[len - 1]))
		len--;
	ctui_vt_cell *copy = NULL;
	if (len > 0) {
		copy = malloc((size_t)len * sizeof *copy);
		if (!copy)
			return;
		memcpy(copy, row, (size_t)len * sizeof *copy);
	}
	size_t idx;
	if (h->count == h->cap) {
		idx = h->start;
		free(h->lines[idx]);
		h->start = (h->start + 1) % h->cap;
	} else {
		idx = (h->start + h->count) % h->cap;
		h->count++;
	}
	h->lines[idx] = copy;
	h->lens[idx] = len;
	h->pushed++;
}

/* ---- scrolling ---- */

/* Scroll rows top..bot by n, up or down; blank rows come in. */
static void scroll_region(ctui_vt *vt, int top, int bot, int n, bool up,
			  bool to_history)
{
	int height = bot - top + 1;
	if (n <= 0 || height <= 0)
		return;
	if (n > height)
		n = height;
	size_t w = (size_t)vt->cols;
	if (up) {
		if (to_history && vt->cur == 0 && top == 0 &&
		    bot == vt->rows - 1)
			for (int i = 0; i < n; i++)
				history_push(vt, cell(vt, i, 0));
		memmove(cell(vt, top, 0), cell(vt, top + n, 0),
			(size_t)(height - n) * w * sizeof(ctui_vt_cell));
		for (int r = bot - n + 1; r <= bot; r++)
			fill(cell(vt, r, 0), w, blank(vt));
	} else {
		memmove(cell(vt, top + n, 0), cell(vt, top, 0),
			(size_t)(height - n) * w * sizeof(ctui_vt_cell));
		for (int r = top; r < top + n; r++)
			fill(cell(vt, r, 0), w, blank(vt));
	}
}

static void linefeed(ctui_vt *vt)
{
	vt->pending = false;
	if (vt->row == vt->bot)
		scroll_region(vt, vt->top, vt->bot, 1, true, true);
	else if (vt->row < vt->rows - 1)
		vt->row++;
}

static void reverse_index(ctui_vt *vt)
{
	vt->pending = false;
	if (vt->row == vt->top)
		scroll_region(vt, vt->top, vt->bot, 1, false, false);
	else if (vt->row > 0)
		vt->row--;
}

/* ---- cursor ---- */

static int clampi(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static void move_to(ctui_vt *vt, int r, int c)
{
	int lo = 0, hi = vt->rows - 1;
	if (vt->modes.origin) {
		r += vt->top;
		lo = vt->top;
		hi = vt->bot;
	}
	vt->row = clampi(r, lo, hi);
	vt->col = clampi(c, 0, vt->cols - 1);
	vt->pending = false;
}

static void next_tab(ctui_vt *vt, int n)
{
	for (; n > 0; n--) {
		int c = vt->col + 1;
		while (c < vt->cols - 1 && !vt->tabs[c])
			c++;
		vt->col = c < vt->cols ? c : vt->cols - 1;
	}
	vt->pending = false;
}

static void prev_tab(ctui_vt *vt, int n)
{
	for (; n > 0; n--) {
		int c = vt->col - 1;
		while (c > 0 && !vt->tabs[c])
			c--;
		vt->col = c < 0 ? 0 : c;
	}
	vt->pending = false;
}

static void save_cursor(ctui_vt *vt)
{
	saved_cursor *s = &vt->saved[vt->cur];
	*s = (saved_cursor){
	    vt->row,   vt->col,		 vt->pending,	       vt->fg, vt->bg,
	    vt->attrs, vt->modes.origin, {vt->g[0], vt->g[1]}, vt->gl, true};
}

static void restore_cursor_from(ctui_vt *vt, const saved_cursor *s)
{
	if (!s->valid) {
		vt->row = vt->col = 0;
		vt->pending = false;
		vt->modes.origin = false;
		return;
	}
	vt->row = clampi(s->row, 0, vt->rows - 1);
	vt->col = clampi(s->col, 0, vt->cols - 1);
	vt->pending = s->pending;
	vt->fg = s->fg;
	vt->bg = s->bg;
	vt->attrs = s->attrs;
	vt->modes.origin = s->origin;
	vt->g[0] = s->g[0];
	vt->g[1] = s->g[1];
	vt->gl = s->gl;
}

/* ---- printing ---- */

static void print_cp(ctui_vt *vt, uint32_t cp)
{
	int w = (int)ctui_cell_width(cp, 0);
	if (w == 0) {
		int c = vt->pending ? vt->col : vt->col - 1;
		if (c < 0)
			return;
		ctui_vt_cell *p = cell(vt, vt->row, c);
		if (p->width == 0 && c > 0)
			p = cell(vt, vt->row, c - 1);
		if (p->cp == 0)
			p->cp = ' ';
		if (p->comb == 0)
			p->comb = cp;
		return;
	}
	if (w > 2)
		w = 2;
	if (vt->pending) {
		if (vt->modes.autowrap) {
			vt->col = 0;
			linefeed(vt);
		}
		vt->pending = false;
	}
	if (w == 2 && vt->col == vt->cols - 1) {
		if (vt->cols < 2)
			return;
		if (!vt->modes.autowrap)
			return;
		unwide(vt, vt->row, vt->col);
		*cell(vt, vt->row, vt->col) = blank(vt);
		vt->col = 0;
		linefeed(vt);
	}
	if (vt->modes.insert) {
		ctui_vt_cell *row = cell(vt, vt->row, 0);
		int n = vt->cols - vt->col - w;
		if (n > 0)
			memmove(&row[vt->col + w], &row[vt->col],
				(size_t)n * sizeof *row);
	}
	unwide(vt, vt->row, vt->col);
	if (w == 2)
		unwide(vt, vt->row, vt->col + 1);
	ctui_vt_cell c = {.cp = cp,
			  .fg = vt->fg,
			  .bg = vt->bg,
			  .attrs = vt->attrs,
			  .width = (uint8_t)w};
	*cell(vt, vt->row, vt->col) = c;
	if (w == 2) {
		c.cp = 0;
		c.width = 0;
		*cell(vt, vt->row, vt->col + 1) = c;
	}
	if (vt->modes.insert)
		fix_row(vt, vt->row);
	vt->col += w;
	if (vt->col >= vt->cols) {
		vt->col = vt->cols - 1;
		vt->pending = true;
	}
}

/* ---- responses and titles ---- */

static void respond(ctui_vt *vt, const char *s)
{
	size_t n = strlen(s);
	if (vt->resp_len + n > sizeof vt->resp)
		return;
	memcpy(vt->resp + vt->resp_len, s, n);
	vt->resp_len += n;
}

static void osc_dispatch(ctui_vt *vt)
{
	vt->osc[vt->osc_len] = '\0';
	char *p = vt->osc;
	int cmd = 0;
	size_t digits = 0;
	while (*p >= '0' && *p <= '9' && digits < 4) {
		cmd = cmd * 10 + (*p - '0');
		p++;
		digits++;
	}
	if (digits == 0 || *p != ';')
		return;
	if (cmd == 0 || cmd == 2)
		ctui_sanitize(vt->title, sizeof vt->title, p + 1, true);
}

/* ---- erase and edit ---- */

static void erase_display(ctui_vt *vt, int mode)
{
	switch (mode) {
	case 0:
		clear_cells(vt, vt->row, vt->col, vt->cols);
		for (int r = vt->row + 1; r < vt->rows; r++)
			clear_cells(vt, r, 0, vt->cols);
		break;
	case 1:
		for (int r = 0; r < vt->row; r++)
			clear_cells(vt, r, 0, vt->cols);
		clear_cells(vt, vt->row, 0, vt->col + 1);
		break;
	case 2:
		for (int r = 0; r < vt->rows; r++)
			clear_cells(vt, r, 0, vt->cols);
		break;
	case 3:
		history_clear(&vt->hist);
		break;
	default:
		break;
	}
}

static void erase_line(ctui_vt *vt, int mode)
{
	if (mode == 0)
		clear_cells(vt, vt->row, vt->col, vt->cols);
	else if (mode == 1)
		clear_cells(vt, vt->row, 0, vt->col + 1);
	else if (mode == 2)
		clear_cells(vt, vt->row, 0, vt->cols);
}

static void insert_chars(ctui_vt *vt, int n)
{
	n = clampi(n, 1, vt->cols - vt->col);
	ctui_vt_cell *row = cell(vt, vt->row, 0);
	memmove(&row[vt->col + n], &row[vt->col],
		(size_t)(vt->cols - vt->col - n) * sizeof *row);
	fill(&row[vt->col], (size_t)n, blank(vt));
	fix_row(vt, vt->row);
	vt->pending = false;
}

static void delete_chars(ctui_vt *vt, int n)
{
	n = clampi(n, 1, vt->cols - vt->col);
	ctui_vt_cell *row = cell(vt, vt->row, 0);
	memmove(&row[vt->col], &row[vt->col + n],
		(size_t)(vt->cols - vt->col - n) * sizeof *row);
	fill(&row[vt->cols - n], (size_t)n, blank(vt));
	fix_row(vt, vt->row);
	vt->pending = false;
}

/* ---- modes ---- */

static void switch_screen(ctui_vt *vt, bool alt, bool save)
{
	if (alt == (vt->cur == 1))
		return;
	if (alt) {
		if (save)
			save_cursor(vt);
		vt->cur = 1;
		fill(vt->buf[1], (size_t)vt->rows * (size_t)vt->cols,
		     blank(vt));
	} else {
		vt->cur = 0;
		if (save)
			restore_cursor_from(vt, &vt->saved[0]);
	}
	vt->modes.alt_screen = alt;
	vt->top = 0;
	vt->bot = vt->rows - 1;
	vt->pending = false;
}

static void set_private(ctui_vt *vt, int mode, bool on)
{
	switch (mode) {
	case 1:
		vt->modes.app_cursor = on;
		break;
	case 6:
		vt->modes.origin = on;
		move_to(vt, 0, 0);
		break;
	case 7:
		vt->modes.autowrap = on;
		break;
	case 25:
		vt->modes.cursor_visible = on;
		break;
	case 47:
	case 1047:
		switch_screen(vt, on, false);
		break;
	case 1049:
		switch_screen(vt, on, true);
		break;
	case 1000:
	case 1002:
	case 1003:
		if (on)
			vt->modes.mouse = mode;
		else if (vt->modes.mouse == mode)
			vt->modes.mouse = 0;
		break;
	case 1004:
		vt->modes.focus_events = on;
		break;
	case 1006:
		vt->modes.mouse_sgr = on;
		break;
	case 2004:
		vt->modes.bracketed_paste = on;
		break;
	default:
		break;
	}
}

/* ---- SGR ---- */

static uint32_t indexed(int n)
{
	return CTUI_VT_INDEXED(n);
}

static void sgr_attr(ctui_vt *vt, int p)
{
	switch (p) {
	case 0:
		vt->fg = vt->bg = 0;
		vt->attrs = 0;
		break;
	case 1:
		vt->attrs |= CTUI_VT_BOLD;
		break;
	case 2:
		vt->attrs |= CTUI_VT_DIM;
		break;
	case 3:
		vt->attrs |= CTUI_VT_ITALIC;
		break;
	case 4:
	case 21:
		vt->attrs |= CTUI_VT_UNDERLINE;
		break;
	case 7:
		vt->attrs |= CTUI_VT_REVERSE;
		break;
	case 8:
		vt->attrs |= CTUI_VT_HIDDEN;
		break;
	case 22:
		vt->attrs &= (uint8_t)~(CTUI_VT_BOLD | CTUI_VT_DIM);
		break;
	case 23:
		vt->attrs &= (uint8_t)~CTUI_VT_ITALIC;
		break;
	case 24:
		vt->attrs &= (uint8_t)~CTUI_VT_UNDERLINE;
		break;
	case 27:
		vt->attrs &= (uint8_t)~CTUI_VT_REVERSE;
		break;
	case 28:
		vt->attrs &= (uint8_t)~CTUI_VT_HIDDEN;
		break;
	case 39:
		vt->fg = 0;
		break;
	case 49:
		vt->bg = 0;
		break;
	default:
		if (p >= 30 && p <= 37)
			vt->fg = indexed(p - 30);
		else if (p >= 40 && p <= 47)
			vt->bg = indexed(p - 40);
		else if (p >= 90 && p <= 97)
			vt->fg = indexed(p - 90 + 8);
		else if (p >= 100 && p <= 107)
			vt->bg = indexed(p - 100 + 8);
		break; /* blink, strike and the rest: ignored */
	}
}

static void set_color(ctui_vt *vt, bool fg, uint32_t color)
{
	if (fg)
		vt->fg = color;
	else
		vt->bg = color;
}

static void do_sgr(ctui_vt *vt)
{
	if (vt->pcount == 0) {
		sgr_attr(vt, 0);
		return;
	}
	int i = 0;
	while (i < vt->pcount) {
		int p = vt->params[i];
		int j = i + 1;
		while (j < vt->pcount && vt->sub[j])
			j++;
		if (j - i > 1) { /* colon form: p:sub:sub... */
			int n = j - i - 1;
			const int *s = &vt->params[i + 1];
			if ((p == 38 || p == 48) && n >= 2 && s[0] == 5) {
				set_color(vt, p == 38,
					  indexed(clampi(s[1], 0, 255)));
			} else if ((p == 38 || p == 48) && n >= 4 &&
				   s[0] == 2) {
				const int *c =
				    s +
				    (n >= 5 ? 2
					    : 1); /* skip the colour space id */
				set_color(vt, p == 38,
					  CTUI_VT_RGB(clampi(c[0], 0, 255),
						      clampi(c[1], 0, 255),
						      clampi(c[2], 0, 255)));
			} else if (p == 4) {
				if (s[0] == 0)
					vt->attrs &=
					    (uint8_t)~CTUI_VT_UNDERLINE;
				else
					vt->attrs |= CTUI_VT_UNDERLINE;
			}
			i = j;
			continue;
		}
		if ((p == 38 || p == 48) && i + 1 < vt->pcount) {
			int kind = vt->params[i + 1];
			if (kind == 5 && i + 2 < vt->pcount) {
				set_color(
				    vt, p == 38,
				    indexed(clampi(vt->params[i + 2], 0, 255)));
				i += 3;
				continue;
			}
			if (kind == 2 && i + 4 < vt->pcount) {
				set_color(
				    vt, p == 38,
				    CTUI_VT_RGB(
					clampi(vt->params[i + 2], 0, 255),
					clampi(vt->params[i + 3], 0, 255),
					clampi(vt->params[i + 4], 0, 255)));
				i += 5;
				continue;
			}
			i = vt->pcount; /* malformed: drop the rest */
			continue;
		}
		sgr_attr(vt, p);
		i++;
	}
}

/* ---- CSI ---- */

static int arg(const ctui_vt *vt, int i, int def)
{
	return (i < vt->pcount && vt->params[i] > 0) ? vt->params[i] : def;
}

static void report_cursor(ctui_vt *vt, bool priv)
{
	int r = vt->row + 1 - (vt->modes.origin ? vt->top : 0);
	char out[32];
	snprintf(out, sizeof out, priv ? "\033[?%d;%dR" : "\033[%d;%dR", r,
		 vt->col + 1);
	respond(vt, out);
}

static void csi_modes(ctui_vt *vt, bool on)
{
	for (int i = 0; i < vt->pcount; i++) {
		int m = vt->params[i];
		if (vt->priv == '?') {
			set_private(vt, m, on);
		} else if (vt->priv == 0) {
			if (m == 4)
				vt->modes.insert = on;
			else if (m == 20)
				vt->modes.newline = on;
		}
	}
}

static void csi_dispatch(ctui_vt *vt, unsigned char f)
{
	if (vt->inter || (vt->priv && vt->priv != '?' && f != 'c'))
		return; /* unsupported intermediates and private markers */
	if (vt->priv == '?' && f != 'h' && f != 'l' && f != 'n')
		return;
	int n = arg(vt, 0, 1);
	switch (f) {
	case 'A':
		vt->row = clampi(vt->row - n, vt->row >= vt->top ? vt->top : 0,
				 vt->rows - 1);
		vt->pending = false;
		break;
	case 'B':
		vt->row = clampi(vt->row + n, 0,
				 vt->row <= vt->bot ? vt->bot : vt->rows - 1);
		vt->pending = false;
		break;
	case 'C':
	case 'a':
		vt->col = clampi(vt->col + n, 0, vt->cols - 1);
		vt->pending = false;
		break;
	case 'D':
		vt->col = clampi(vt->col - n, 0, vt->cols - 1);
		vt->pending = false;
		break;
	case 'E':
		vt->row = clampi(vt->row + n, 0,
				 vt->row <= vt->bot ? vt->bot : vt->rows - 1);
		vt->col = 0;
		vt->pending = false;
		break;
	case 'F':
		vt->row = clampi(vt->row - n, vt->row >= vt->top ? vt->top : 0,
				 vt->rows - 1);
		vt->col = 0;
		vt->pending = false;
		break;
	case 'G':
	case '`':
		vt->col = clampi(n - 1, 0, vt->cols - 1);
		vt->pending = false;
		break;
	case 'H':
	case 'f':
		move_to(vt, arg(vt, 0, 1) - 1, arg(vt, 1, 1) - 1);
		break;
	case 'I':
		next_tab(vt, clampi(n, 1, vt->cols));
		break;
	case 'Z':
		prev_tab(vt, clampi(n, 1, vt->cols));
		break;
	case 'd': {
		int r = n - 1;
		if (vt->modes.origin)
			r = clampi(r + vt->top, vt->top, vt->bot);
		vt->row = clampi(r, 0, vt->rows - 1);
		vt->pending = false;
		break;
	}
	case 'e':
		vt->row = clampi(vt->row + n, 0, vt->rows - 1);
		vt->pending = false;
		break;
	case 'J':
		erase_display(vt, vt->pcount ? vt->params[0] : 0);
		break;
	case 'K':
		erase_line(vt, vt->pcount ? vt->params[0] : 0);
		break;
	case 'X':
		clear_cells(vt, vt->row, vt->col,
			    vt->col + clampi(n, 1, vt->cols));
		break;
	case '@':
		insert_chars(vt, n);
		break;
	case 'P':
		delete_chars(vt, n);
		break;
	case 'L':
	case 'M':
		if (vt->row >= vt->top && vt->row <= vt->bot) {
			scroll_region(vt, vt->row, vt->bot, n, f == 'M', false);
			vt->col = 0;
			vt->pending = false;
		}
		break;
	case 'S':
		scroll_region(vt, vt->top, vt->bot, n, true, true);
		break;
	case 'T':
		if (vt->pcount <= 1)
			scroll_region(vt, vt->top, vt->bot, n, false, false);
		break;
	case 'g': {
		int m = vt->pcount ? vt->params[0] : 0;
		if (m == 0)
			vt->tabs[vt->col] = false;
		else if (m == 3)
			memset(vt->tabs, 0, (size_t)vt->cols);
		break;
	}
	case 'h':
		csi_modes(vt, true);
		break;
	case 'l':
		csi_modes(vt, false);
		break;
	case 'm':
		do_sgr(vt);
		break;
	case 'n': {
		int m = vt->pcount ? vt->params[0] : 0;
		if (m == 5 && vt->priv == 0)
			respond(vt, "\033[0n");
		else if (m == 6)
			report_cursor(vt, vt->priv == '?');
		break;
	}
	case 'c':
		if (vt->pcount && vt->params[0] != 0)
			break;
		if (vt->priv == 0)
			respond(vt, "\033[?1;2c");
		else if (vt->priv == '>')
			respond(vt, "\033[>0;0;0c");
		break;
	case 'r': {
		int t = arg(vt, 0, 1), b = arg(vt, 1, vt->rows);
		if (b > vt->rows)
			b = vt->rows;
		if (t < b) {
			vt->top = t - 1;
			vt->bot = b - 1;
			move_to(vt, 0, 0);
		}
		break;
	}
	case 's':
		if (vt->priv == 0)
			save_cursor(vt);
		break;
	case 'u':
		if (vt->priv == 0)
			restore_cursor_from(vt, &vt->saved[vt->cur]);
		break;
	default:
		break;
	}
}

/* ---- escape sequences and controls ---- */

static void reset_all(ctui_vt *vt)
{
	vt->cur = 0;
	for (int s = 0; s < 2; s++) {
		vt->cur = s;
		vt->fg = vt->bg = 0;
		fill(vt->buf[s], (size_t)vt->rows * (size_t)vt->cols, BLANK);
		vt->saved[s].valid = false;
	}
	vt->cur = 0;
	vt->row = vt->col = 0;
	vt->pending = false;
	vt->top = 0;
	vt->bot = vt->rows - 1;
	vt->modes = (ctui_vt_modes){.autowrap = true, .cursor_visible = true};
	vt->attrs = 0;
	vt->g[0] = vt->g[1] = 0;
	vt->gl = 0;
	for (int c = 0; c < vt->cols; c++)
		vt->tabs[c] = c % 8 == 0;
	vt->title[0] = '\0';
	history_clear(&vt->hist);
}

static void esc_dispatch(ctui_vt *vt, unsigned char f)
{
	if (vt->inter == '(' || vt->inter == ')') {
		vt->g[vt->inter == ')' ? 1 : 0] = f == '0' ? 1 : 0;
		return;
	}
	if (vt->inter == '#') {
		if (f == '8')
			for (int r = 0; r < vt->rows; r++)
				for (int c = 0; c < vt->cols; c++) {
					ctui_vt_cell e = BLANK;
					e.cp = 'E';
					*cell(vt, r, c) = e;
				}
		return;
	}
	if (vt->inter)
		return;
	switch (f) {
	case '7':
		save_cursor(vt);
		break;
	case '8':
		restore_cursor_from(vt, &vt->saved[vt->cur]);
		break;
	case 'D':
		linefeed(vt);
		break;
	case 'E':
		vt->col = 0;
		linefeed(vt);
		break;
	case 'M':
		reverse_index(vt);
		break;
	case 'H':
		vt->tabs[vt->col] = true;
		break;
	case 'c':
		reset_all(vt);
		break;
	case '=':
		vt->modes.app_keypad = true;
		break;
	case '>':
		vt->modes.app_keypad = false;
		break;
	default:
		break;
	}
}

static void control(ctui_vt *vt, unsigned char b)
{
	switch (b) {
	case 0x08:
		if (vt->col > 0)
			vt->col--;
		vt->pending = false;
		break;
	case 0x09:
		next_tab(vt, 1);
		break;
	case 0x0A:
	case 0x0B:
	case 0x0C:
		if (vt->modes.newline)
			vt->col = 0;
		linefeed(vt);
		break;
	case 0x0D:
		vt->col = 0;
		vt->pending = false;
		break;
	case 0x0E:
		vt->gl = 1;
		break;
	case 0x0F:
		vt->gl = 0;
		break;
	default:
		break;
	}
}

static void csi_start(ctui_vt *vt)
{
	vt->state = ST_CSI;
	vt->pcount = 0;
	vt->pcur = vt->next_sub = vt->overflow = false;
	vt->priv = vt->inter = 0;
}

static void ensure_param(ctui_vt *vt)
{
	if (vt->pcount < MAX_PARAMS) {
		vt->params[vt->pcount] = 0;
		vt->sub[vt->pcount] = vt->next_sub;
		vt->pcount++;
	} else {
		vt->overflow = true;
	}
	vt->next_sub = false;
	vt->pcur = true;
}

static void csi_byte(ctui_vt *vt, unsigned char b)
{
	if (b >= '0' && b <= '9') {
		if (vt->state != ST_CSI) {
			vt->state = ST_CSI_IGNORE;
			return;
		}
		if (!vt->pcur)
			ensure_param(vt);
		if (!vt->overflow) {
			int *p = &vt->params[vt->pcount - 1];
			*p = *p * 10 + (b - '0');
			if (*p > MAX_PARAM_VALUE)
				*p = MAX_PARAM_VALUE;
		}
	} else if (b == ';' || b == ':') {
		if (vt->state != ST_CSI) {
			vt->state = ST_CSI_IGNORE;
			return;
		}
		if (!vt->pcur)
			ensure_param(vt);
		vt->pcur = false;
		vt->next_sub = b == ':';
		if (vt->next_sub && vt->overflow)
			vt->next_sub = false;
	} else if (b >= 0x3C && b <= 0x3F) {
		if (vt->state == ST_CSI && vt->pcount == 0 && !vt->pcur &&
		    !vt->priv)
			vt->priv = (char)b;
		else
			vt->state = ST_CSI_IGNORE;
	} else if (b >= 0x20 && b <= 0x2F) {
		if (vt->state != ST_CSI_IGNORE)
			vt->state = ST_CSI_INTER;
		if (!vt->inter)
			vt->inter = (char)b;
	} else if (b >= 0x40 && b <= 0x7E) {
		if (vt->state != ST_CSI_IGNORE) {
			/* a trailing empty parameter or sub-parameter */
			if ((vt->pcount > 0 && !vt->pcur) || vt->next_sub)
				ensure_param(vt);
			csi_dispatch(vt, b);
		}
		vt->state = ST_GROUND;
	}
	/* 0x7F and 8-bit bytes: ignored */
}

static void print_byte(ctui_vt *vt, unsigned char b)
{
	uint32_t cp = b;
	if (vt->g[vt->gl] == 1 && b >= 0x5F && b <= 0x7E)
		cp = GRAPHICS[b - 0x5F];
	print_cp(vt, cp);
}

static void finish_utf8(ctui_vt *vt)
{
	uint32_t cp = vt->u_cp;
	if (cp < vt->u_min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		cp = 0xFFFD;
	if (cp >= 0x80 && cp <= 0x9F)
		return; /* C1 controls are not supported */
	print_cp(vt, cp);
}

static void string_end(ctui_vt *vt)
{
	if (vt->str_osc)
		osc_dispatch(vt);
	vt->state = ST_GROUND;
}

static void put_byte(ctui_vt *vt, unsigned char b)
{
	if (vt->u_need) {
		if (((unsigned)b & 0xC0U) == 0x80U) {
			vt->u_cp = vt->u_cp << 6U | (b & 0x3FU);
			if (--vt->u_need == 0)
				finish_utf8(vt);
			return;
		}
		vt->u_need = 0;
		print_cp(vt, 0xFFFD);
	}
	if (b == 0x18 || b == 0x1A) {
		vt->state = ST_GROUND;
		return;
	}
	if (b == 0x1B) {
		if (vt->state == ST_OSC || vt->state == ST_STRING) {
			vt->state = ST_STR_ESC;
		} else {
			vt->state = ST_ESC;
			vt->inter = 0;
		}
		return;
	}
	switch (vt->state) {
	case ST_STR_ESC:
		if (b == '\\') {
			string_end(vt);
			return;
		}
		vt->state = ST_ESC;
		vt->inter = 0;
		put_byte(vt, b);
		return;
	case ST_OSC:
		if (b == 0x07) {
			string_end(vt);
		} else if (b >= 0x20 && vt->osc_len < CTUI_VT_OSC_MAX - 1) {
			vt->osc[vt->osc_len++] = (char)b;
		}
		return;
	case ST_STRING:
		return;
	case ST_ESC:
	case ST_ESC_INTER:
		if (b < 0x20) {
			control(vt, b);
		} else if (b <= 0x2F) {
			vt->state = ST_ESC_INTER;
			if (!vt->inter)
				vt->inter = (char)b;
		} else if (b <= 0x7E) {
			vt->state = ST_GROUND;
			if (vt->inter == 0 && b == '[') {
				csi_start(vt);
			} else if (vt->inter == 0 && b == ']') {
				vt->state = ST_OSC;
				vt->str_osc = true;
				vt->osc_len = 0;
			} else if (vt->inter == 0 && (b == 'P' || b == 'X' ||
						      b == '^' || b == '_')) {
				vt->state = ST_STRING;
				vt->str_osc = false;
			} else {
				esc_dispatch(vt, b);
			}
		}
		return;
	case ST_CSI:
	case ST_CSI_INTER:
	case ST_CSI_IGNORE:
		if (b < 0x20)
			control(vt, b);
		else
			csi_byte(vt, b);
		return;
	case ST_GROUND:
		break;
	}
	if (b < 0x20) {
		control(vt, b);
	} else if (b < 0x7F) {
		print_byte(vt, b);
	} else if (b == 0x7F) {
		/* DEL: ignored */
		// NOLINTNEXTLINE(bugprone-branch-clone)
	} else if (b < 0xC0) {
		print_cp(vt, 0xFFFD);
	} else if (b < 0xE0 && b >= 0xC2) {
		vt->u_need = 1, vt->u_cp = b & 0x1FU, vt->u_min = 0x80;
	} else if (b < 0xF0 && b >= 0xE0) {
		vt->u_need = 2, vt->u_cp = b & 0x0FU, vt->u_min = 0x800;
	} else if (b < 0xF5 && b >= 0xF0) {
		vt->u_need = 3, vt->u_cp = b & 0x07U, vt->u_min = 0x10000;
	} else {
		print_cp(vt, 0xFFFD);
	}
}

/* ---- public API ---- */

static bool alloc_screens(int rows, int cols, ctui_vt_cell *out[2])
{
	size_t n = (size_t)rows * (size_t)cols;
	out[0] = malloc(n * sizeof **out);
	out[1] = malloc(n * sizeof **out);
	if (!out[0] || !out[1]) {
		free(out[0]);
		free(out[1]);
		return false;
	}
	fill(out[0], n, BLANK);
	fill(out[1], n, BLANK);
	return true;
}

ctui_vt *ctui_vt_new(int rows, int cols, size_t scrollback)
{
	if (rows < 1 || rows > CTUI_VT_MAX_ROWS || cols < 1 ||
	    cols > CTUI_VT_MAX_COLS)
		return NULL;
	if (scrollback > CTUI_VT_MAX_SCROLLBACK)
		scrollback = CTUI_VT_MAX_SCROLLBACK;
	ctui_vt *vt = calloc(1, sizeof *vt);
	if (!vt)
		return NULL;
	vt->rows = rows;
	vt->cols = cols;
	vt->tabs = calloc((size_t)cols, sizeof *vt->tabs);
	if (scrollback) {
		vt->hist.lines =
		    (ctui_vt_cell **)calloc(scrollback, sizeof *vt->hist.lines);
		vt->hist.lens = calloc(scrollback, sizeof *vt->hist.lens);
		if (vt->hist.lines && vt->hist.lens)
			vt->hist.cap = scrollback;
	}
	if (!vt->tabs || !alloc_screens(rows, cols, vt->buf) ||
	    (scrollback && !vt->hist.cap)) {
		free(vt->tabs);
		free((void *)vt->hist.lines);
		free(vt->hist.lens);
		free(vt);
		return NULL;
	}
	reset_all(vt);
	return vt;
}

void ctui_vt_free(ctui_vt *vt)
{
	if (!vt)
		return;
	history_free_lines(&vt->hist);
	free((void *)vt->hist.lines);
	free(vt->hist.lens);
	free(vt->buf[0]);
	free(vt->buf[1]);
	free(vt->tabs);
	free(vt);
}

void ctui_vt_feed(ctui_vt *vt, const void *bytes, size_t len)
{
	const unsigned char *p = bytes;
	if (!vt || !p)
		return;
	if (len)
		vt->gen++;
	for (size_t i = 0; i < len; i++)
		put_byte(vt, p[i]);
}

bool ctui_vt_resize(ctui_vt *vt, int rows, int cols)
{
	if (!vt || rows < 1 || rows > CTUI_VT_MAX_ROWS || cols < 1 ||
	    cols > CTUI_VT_MAX_COLS)
		return false;
	if (rows == vt->rows && cols == vt->cols)
		return true;
	ctui_vt_cell *nb[2];
	bool *tabs = calloc((size_t)cols, sizeof *tabs);
	if (!tabs)
		return false;
	if (!alloc_screens(rows, cols, nb)) {
		free(tabs);
		return false;
	}
	int shift = 0;
	if (vt->cur == 0 && vt->row >= rows) {
		shift = vt->row - rows + 1;
		for (int i = 0; i < shift; i++)
			history_push(vt,
				     &vt->buf[0][(size_t)i * (size_t)vt->cols]);
	}
	for (int s = 0; s < 2; s++) {
		int sh = s == 0 ? shift : 0;
		for (int r = 0; r < rows && r + sh < vt->rows; r++)
			for (int c = 0; c < cols && c < vt->cols; c++)
				nb[s][(size_t)r * (size_t)cols + (size_t)c] =
				    vt->buf[s][(size_t)(r + sh) *
						   (size_t)vt->cols +
					       (size_t)c];
	}
	for (int c = 0; c < cols; c++)
		tabs[c] = c < vt->cols ? vt->tabs[c] : c % 8 == 0;
	free(vt->buf[0]);
	free(vt->buf[1]);
	free(vt->tabs);
	vt->buf[0] = nb[0];
	vt->buf[1] = nb[1];
	vt->tabs = tabs;
	vt->rows = rows;
	vt->cols = cols;
	vt->row = clampi(vt->row - (vt->cur == 0 ? shift : 0), 0, rows - 1);
	vt->col = clampi(vt->col, 0, cols - 1);
	vt->pending = false;
	vt->top = 0;
	vt->bot = rows - 1;
	for (int s = 0; s < 2; s++) {
		int save = vt->cur;
		vt->cur = s;
		for (int r = 0; r < rows; r++)
			fix_row(vt, r);
		vt->cur = save;
		vt->saved[s].row = clampi(vt->saved[s].row, 0, rows - 1);
		vt->saved[s].col = clampi(vt->saved[s].col, 0, cols - 1);
	}
	vt->gen++;
	return true;
}

int ctui_vt_rows(const ctui_vt *vt)
{
	return vt->rows;
}
int ctui_vt_cols(const ctui_vt *vt)
{
	return vt->cols;
}

const ctui_vt_cell *ctui_vt_cell_at(const ctui_vt *vt, int row, int col)
{
	if (!vt || row < 0 || row >= vt->rows || col < 0 || col >= vt->cols)
		return NULL;
	return cell(vt, row, col);
}

static size_t encode_utf8(uint32_t cp, char *out)
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xC0U | cp >> 6U);
		out[1] = (char)(0x80U | (cp & 0x3FU));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xE0U | cp >> 12U);
		out[1] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
		out[2] = (char)(0x80U | (cp & 0x3FU));
		return 3;
	}
	out[0] = (char)(0xF0U | cp >> 18U);
	out[1] = (char)(0x80U | ((cp >> 12U) & 0x3FU));
	out[2] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
	out[3] = (char)(0x80U | (cp & 0x3FU));
	return 4;
}

static size_t line_text(const ctui_vt_cell *cells, int n, char *buf, size_t cap)
{
	size_t len = 0, keep = 0, copied = 0, keep_copied = 0;
	bool full = false;
	char tmp[8];
	for (int c = 0; c < n; c++) {
		if (cells[c].width == 0)
			continue;
		size_t k = encode_utf8(cells[c].cp ? cells[c].cp : ' ', tmp);
		if (cells[c].comb)
			k += encode_utf8(cells[c].comb, tmp + k);
		if (!full && cap && copied + k < cap) {
			memcpy(buf + copied, tmp, k);
			copied += k;
		} else {
			full = true;
		}
		len += k;
		if (cells[c].cp) {
			keep = len;
			keep_copied = copied;
		}
	}
	if (cap)
		buf[keep_copied] = '\0';
	return keep;
}

size_t ctui_vt_row_text(const ctui_vt *vt, int row, char *buf, size_t cap)
{
	if (!vt || row < 0 || row >= vt->rows) {
		if (cap)
			buf[0] = '\0';
		return 0;
	}
	return line_text(cell(vt, row, 0), vt->cols, buf, cap);
}

int ctui_vt_cursor_row(const ctui_vt *vt)
{
	return vt->row;
}
int ctui_vt_cursor_col(const ctui_vt *vt)
{
	return vt->col;
}
const ctui_vt_modes *ctui_vt_get_modes(const ctui_vt *vt)
{
	return &vt->modes;
}
const char *ctui_vt_title(const ctui_vt *vt)
{
	return vt->title;
}
size_t ctui_vt_history_count(const ctui_vt *vt)
{
	return vt->hist.count;
}
size_t ctui_vt_history_pushed(const ctui_vt *vt)
{
	return vt->hist.pushed;
}
unsigned long ctui_vt_generation(const ctui_vt *vt)
{
	return vt->gen;
}

const ctui_vt_cell *ctui_vt_history_cell(const ctui_vt *vt, size_t age, int col)
{
	const history *h = &vt->hist;
	if (age >= h->count || col < 0 || col >= vt->cols)
		return NULL;
	size_t idx = (h->start + h->count - 1 - age) % h->cap;
	if (col >= h->lens[idx])
		return &BLANK;
	return &h->lines[idx][col];
}

size_t ctui_vt_history_text(const ctui_vt *vt, size_t age, char *buf,
			    size_t cap)
{
	const history *h = &vt->hist;
	if (age >= h->count) {
		if (cap)
			buf[0] = '\0';
		return 0;
	}
	size_t idx = (h->start + h->count - 1 - age) % h->cap;
	return line_text(h->lines[idx], h->lens[idx], buf, cap);
}

size_t ctui_vt_take_response(ctui_vt *vt, char *buf, size_t cap)
{
	size_t n = vt->resp_len < cap ? vt->resp_len : cap;
	memcpy(buf, vt->resp, n);
	memmove(vt->resp, vt->resp + n, vt->resp_len - n);
	vt->resp_len -= n;
	return n;
}
