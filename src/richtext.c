/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * richtext.c - spans, markup parser, wrapping, alignment and hit testing
 * (richtext.h). Wrapping, drawing and hit testing share one row walker so
 * that what is clicked is what was drawn.
 */
#include <ctui/richtext.h>

#include <curses.h>
#include <limits.h>
#include <string.h>

#include <ctui/text.h>

/* ---- document and markup ---- */

void ctui_rt_doc_init(ctui_rt_doc *doc, char *text, size_t text_cap,
		      ctui_span *spans, size_t span_cap, ctui_rt_line *lines,
		      size_t line_cap)
{
	*doc = (ctui_rt_doc){.spans = spans,
			     .span_cap = span_cap,
			     .lines = lines,
			     .line_cap = line_cap,
			     .text = text,
			     .text_cap = text_cap};
}

typedef struct style {
	unsigned attrs;
	ctui_pair pair;
	int link;
} style;

typedef struct frame {
	char kind;
	style saved;
} frame;

typedef struct parser {
	ctui_rt_doc *doc;
	size_t seg_start, line_first;
	style cur;
	frame stack[CTUI_RT_DEPTH];
	int depth;
	style seg_style;
} parser;

static const struct {
	const char *name;
	ctui_pair pair;
} COLOURS[] = {
    {"body", CTUI_PAIR_BODY},	      {"muted", CTUI_PAIR_MUTED},
    {"heading", CTUI_PAIR_HEADING},   {"ok", CTUI_PAIR_GAUGE_GREEN},
    {"warn", CTUI_PAIR_GAUGE_YELLOW}, {"err", CTUI_PAIR_ERROR},
    {"info", CTUI_PAIR_CURSOR},	      {"accent", CTUI_PAIR_BORDER_SELECTED},
    {"sel", CTUI_PAIR_SELECTION},
};

static bool flush_span(parser *p)
{
	ctui_rt_doc *d = p->doc;
	if (d->text_len > p->seg_start) {
		if (d->span_count >= d->span_cap)
			return false;
		d->spans[d->span_count++] =
		    (ctui_span){.text = d->text + p->seg_start,
				.len = d->text_len - p->seg_start,
				.pair = p->seg_style.pair,
				.attrs = p->seg_style.attrs,
				.link = p->seg_style.link};
		p->seg_start = d->text_len;
	}
	p->seg_style = p->cur;
	return true;
}

static bool end_line(parser *p)
{
	ctui_rt_doc *d = p->doc;
	if (!flush_span(p) || d->line_count >= d->line_cap)
		return false;
	d->lines[d->line_count++] =
	    (ctui_rt_line){p->line_first, d->span_count - p->line_first};
	p->line_first = d->span_count;
	return true;
}

static bool put_byte(parser *p, char c)
{
	ctui_rt_doc *d = p->doc;
	if (d->text_len >= d->text_cap)
		return false;
	d->text[d->text_len++] = c;
	return true;
}

/* Recognise the inside of a tag (between the brackets). kind 0 with
 * closing is "[/]". */
static bool parse_tag(const char *c, size_t n, bool *closing, char *kind,
		      ctui_pair *pair, int *link)
{
	*kind = 0;
	*closing = n > 0 && c[0] == '/';
	if (*closing) {
		c++;
		n--;
		if (n == 0)
			return true;
	}
	if (n == 1 && strchr("bdiur", c[0])) {
		*kind = c[0];
		return true;
	}
	if (*closing) {
		if (n == 2 && !strncmp(c, "fg", 2))
			return *kind = 'f', true;
		if (n == 4 && !strncmp(c, "link", 4))
			return *kind = 'l', true;
		return false;
	}
	if (n > 3 && !strncmp(c, "fg=", 3)) {
		for (size_t i = 0; i < sizeof COLOURS / sizeof COLOURS[0]; i++)
			if (strlen(COLOURS[i].name) == n - 3 &&
			    !strncmp(c + 3, COLOURS[i].name, n - 3)) {
				*pair = COLOURS[i].pair;
				return *kind = 'f', true;
			}
		return false;
	}
	if (n > 5 && n <= 5 + 9 && !strncmp(c, "link=", 5)) {
		long v = 0;
		for (size_t i = 5; i < n; i++) {
			if (c[i] < '0' || c[i] > '9')
				return false;
			v = v * 10 + (c[i] - '0');
		}
		if (v <= 0)
			return false;
		*link = (int)v;
		return *kind = 'l', true;
	}
	return false;
}

/* Handle a tag whose inside is c[0..n); false when it is literal text. */
static bool apply_tag(parser *p, const char *c, size_t n)
{
	bool closing;
	char kind;
	ctui_pair pair = CTUI_PAIR_BODY;
	int link = 0;
	if (!parse_tag(c, n, &closing, &kind, &pair, &link))
		return false;
	if (closing) {
		if (p->depth == 0 ||
		    (kind && p->stack[p->depth - 1].kind != kind))
			return false;
		flush_span(p);
		p->cur = p->stack[--p->depth].saved;
	} else {
		if (p->depth >= CTUI_RT_DEPTH)
			return false;
		flush_span(p);
		p->stack[p->depth++] = (frame){kind, p->cur};
		switch (kind) {
		case 'b':
			p->cur.attrs |= CTUI_RT_BOLD;
			break;
		case 'd':
			p->cur.attrs |= CTUI_RT_DIM;
			break;
		case 'i':
			p->cur.attrs |= CTUI_RT_ITALIC;
			break;
		case 'u':
			p->cur.attrs |= CTUI_RT_UNDERLINE;
			break;
		case 'r':
			p->cur.attrs |= CTUI_RT_REVERSE;
			break;
		case 'f':
			p->cur.pair = pair;
			break;
		default:
			p->cur.link = link;
			break;
		}
	}
	p->seg_style = p->cur;
	return true;
}

bool ctui_rt_parse(ctui_rt_doc *doc, const char *markup)
{
	parser p = {.doc = doc};
	doc->span_count = doc->line_count = doc->text_len = 0;
	doc->overflow = false;
	if (!markup)
		return true;
	bool ok = true, pending = false;
	for (const char *s = markup; *s && ok; s++) {
		if (*s == '\n') {
			ok = end_line(&p);
			pending = false;
			continue;
		}
		if (*s == '\r')
			continue;
		if (*s == '[') {
			const char *e = s + 1;
			while (*e && *e != ']' && *e != '[' && *e != '\n' &&
			       e - s < 40)
				e++;
			if (*e == ']' &&
			    apply_tag(&p, s + 1, (size_t)(e - s - 1))) {
				s = e;
				continue;
			}
		}
		char c = *s;
		if (c == '\\' && (s[1] == '[' || s[1] == '\\'))
			c = *++s;
		ok = put_byte(&p, c);
		pending = true;
	}
	if (ok && pending)
		ok = end_line(&p);
	else if (!ok)
		end_line(&p);
	doc->overflow = !ok;
	return ok;
}

int ctui_rt_attr(const ctui_span *span)
{
	int attr = ctui_attr(span->pair ? span->pair : CTUI_PAIR_BODY,
			     (span->attrs & CTUI_RT_BOLD) != 0);
	if (span->attrs & CTUI_RT_DIM)
		attr = (int)((unsigned)attr | (unsigned)A_DIM);
#ifdef A_ITALIC
	if (span->attrs & CTUI_RT_ITALIC)
		attr = (int)((unsigned)attr | (unsigned)A_ITALIC);
#endif
	if (span->attrs & CTUI_RT_UNDERLINE)
		attr = (int)((unsigned)attr | (unsigned)A_UNDERLINE);
	if (span->attrs & CTUI_RT_REVERSE)
		attr = (int)((unsigned)attr | (unsigned)A_REVERSE);
	return attr;
}

/* ---- layout ---- */

typedef struct pos {
	size_t span, off;
} pos;

typedef struct row {
	size_t line;
	pos from, to;
	int cols;
	bool last; /* last row of its line */
} row;

typedef struct iter {
	const ctui_rt_doc *d;
	bool wrap;
	int width;
	size_t line;
	pos at;
	bool started;
} iter;

static size_t line_end(const ctui_rt_doc *d, size_t line)
{
	size_t end = d->lines[line].first + d->lines[line].count;
	return end > d->span_count ? d->span_count : end;
}

/* Skip exhausted spans; true at the end of the line. */
static bool norm(const ctui_rt_doc *d, size_t end, pos *p)
{
	while (p->span < end && p->off >= d->spans[p->span].len) {
		p->span++;
		p->off = 0;
	}
	return p->span >= end;
}

/* Decode the character at p (not at the end): its width and byte length. */
static size_t step(const ctui_rt_doc *d, pos p, unsigned long *cp, int *w)
{
	const ctui_span *s = &d->spans[p.span];
	char tmp[5] = {0};
	size_t room = s->len - p.off;
	memcpy(tmp, s->text + p.off, room < 4 ? room : 4);
	size_t n = ctui_utf8_decode(tmp, cp);
	if (n > room) {
		*cp = 0xFFFD;
		n = 1;
	}
	*w = ctui_codepoint_width(*cp);
	return n;
}

static bool before(pos a, pos b)
{
	return a.span < b.span || (a.span == b.span && a.off < b.off);
}

static void advance(pos *p, size_t n)
{
	p->off += n;
}

static void skip_spaces(const ctui_rt_doc *d, size_t end, pos *p)
{
	while (!norm(d, end, p) && d->spans[p->span].text[p->off] == ' ')
		advance(p, 1);
}

static void iter_init(iter *it, const ctui_rt_doc *d, bool wrap, int width)
{
	*it = (iter){.d = d, .wrap = wrap && width > 0, .width = width};
}

static bool iter_next(iter *it, row *r)
{
	const ctui_rt_doc *d = it->d;
	if (!d || it->width <= 0 || it->line >= d->line_count)
		return false;
	size_t end = line_end(d, it->line);
	if (!it->started) {
		it->at = (pos){d->lines[it->line].first, 0};
		it->started = true;
	}
	pos p = it->at, brk = p, next, to;
	int col = 0, brk_col = 0, cols;
	bool have_break = false, have_word = false, in_space = false, more;
	for (;;) {
		if (norm(d, end, &p)) {
			to = next = p;
			cols = col;
			more = false;
			break;
		}
		unsigned long cp;
		int w;
		size_t n = step(d, p, &cp, &w);
		if (it->wrap && col + w > it->width) {
			if (have_break && (cp != ' ' || in_space)) {
				to = brk;
				cols = brk_col;
				next = brk;
			} else if (col == 0) { /* a character wider than the
						  row: take it */
				advance(&p, n);
				col += w;
				norm(d, end, &p);
				to = next = p;
				cols = col;
			} else {
				to = next = p;
				cols = col;
			}
			skip_spaces(d, end, &next);
			more = !norm(d, end, &next);
			break;
		}
		if (cp == ' ') {
			if (have_word && !in_space) {
				have_break = true;
				brk = p;
				brk_col = col;
			}
			in_space = true;
		} else {
			have_word = true;
			in_space = false;
		}
		advance(&p, n);
		col += w;
	}
	norm(d, end, &to);
	*r = (row){.line = it->line,
		   .from = it->at,
		   .to = to,
		   .cols = cols,
		   .last = !more};
	if (more) {
		it->at = next;
	} else {
		it->line++;
		it->started = false;
	}
	return true;
}

typedef bool (*run_fn)(void *user, int ry, size_t span, const char *text,
		       size_t len, int col, int cols, int pad);

/* Walk the runs of a row aligned in width columns, none going past limit.
 * Returns the column after the last run. */
static int walk_row(const ctui_rt_doc *d, const row *r, ctui_rt_align align,
		    int width, int limit, int ry, run_fn fn, void *user)
{
	size_t end = line_end(d, r->line);
	int start = 0, gaps = 0, extra = 0;
	if (r->cols <= width) {
		if (align == CTUI_RT_CENTER)
			start = (width - r->cols) / 2;
		else if (align == CTUI_RT_RIGHT)
			start = width - r->cols;
		else if (align == CTUI_RT_JUSTIFY && !r->last) {
			bool word = false, space = false;
			pos p = r->from;
			while (!norm(d, end, &p) && before(p, r->to)) {
				unsigned long cp;
				int w;
				size_t n = step(d, p, &cp, &w);
				if (cp == ' ') {
					if (word && !space)
						gaps++;
					space = true;
				} else {
					word = true;
					space = false;
				}
				advance(&p, n);
			}
			extra = width - r->cols;
			if (gaps == 0)
				extra = 0;
		}
	}
	int col = start, gap = 0;
	bool word = false, space = false;
	pos p = r->from;
	size_t run_span = 0, run_off = 0, run_len = 0;
	int run_col = col, run_cols = 0;
	bool stop = false;
#define FLUSH()                                                                \
	do {                                                                   \
		if (run_len &&                                                 \
		    !fn(user, ry, run_span, d->spans[run_span].text + run_off, \
			run_len, run_col, run_cols, 0))                        \
			stop = true;                                           \
		run_len = 0;                                                   \
		run_cols = 0;                                                  \
	} while (0)
	while (!stop && !norm(d, end, &p) && before(p, r->to)) {
		unsigned long cp;
		int w;
		size_t n = step(d, p, &cp, &w);
		if (col + w > limit)
			break;
		if (run_span != p.span)
			FLUSH(); /* a no-op without a run */
		if (!run_len) {
			run_span = p.span;
			run_off = p.off;
			run_col = col;
		}
		run_len += n;
		run_cols += w;
		col += w;
		advance(&p, n);
		if (cp == ' ') {
			if (word && !space && gaps) {
				int pad = extra / gaps + (gap < extra % gaps);
				gap++;
				FLUSH();
				if (pad > 0 && !stop) {
					fn(user, ry, run_span, NULL, 0, col,
					   pad, pad);
					col += pad;
				}
			}
			space = true;
		} else {
			word = true;
			space = false;
		}
	}
	FLUSH();
#undef FLUSH
	return col;
}

/* Visit the rows shown by view in a width x height rectangle. */
static int visit(const ctui_rt_doc *d, const ctui_rt_view *v, int width,
		 int height, run_fn fn, void *user)
{
	if (!d || !v || width <= 0 || height <= 0)
		return 0;
	const char *glyph = ctui_theme_glyphs()->ellipsis;
	int ew = v->ellipsis && glyph ? (int)ctui_utf8_columns(glyph) : 0;
	if (ew >= width)
		ew = 0;
	iter it;
	iter_init(&it, d, v->wrap, width);
	row r;
	size_t index = 0;
	int ry = 0;
	while (ry < height && iter_next(&it, &r)) {
		if (index++ < v->top)
			continue;
		ctui_rt_align align = v->align;
		int limit = width;
		bool ell = false;
		if (ew > 0) {
			if (!v->wrap)
				ell = r.cols > width;
			else if (ry == height - 1) {
				iter peek = it;
				row next;
				ell = iter_next(&peek, &next);
			}
		}
		if (ell) {
			align = CTUI_RT_LEFT;
			limit = r.cols + ew <= width ? r.cols : width - ew;
		} else if (r.cols > width) {
			align = CTUI_RT_LEFT;
		}
		int used = walk_row(d, &r, align, width, limit, ry, fn, user);
		if (ell)
			fn(user, ry, (size_t)-1, glyph, strlen(glyph), used, ew,
			   0);
		ry++;
	}
	return ry;
}

size_t ctui_rt_height(const ctui_rt_doc *doc, int width, bool wrap)
{
	iter it;
	iter_init(&it, doc, wrap, width);
	row r;
	size_t n = 0;
	while (iter_next(&it, &r))
		n++;
	return n;
}

typedef struct text_out {
	char *out;
	size_t cap, len;
} text_out;

static bool text_run(void *user, int ry, size_t span, const char *text,
		     size_t len, int col, int cols, int pad)
{
	(void)ry;
	(void)span;
	(void)col;
	(void)cols;
	(void)pad;
	text_out *o = user;
	if (text && o->len + len < o->cap) {
		memcpy(o->out + o->len, text, len);
		o->len += len;
	}
	return true;
}

bool ctui_rt_row_text(const ctui_rt_doc *doc, int width, bool wrap,
		      size_t row_index, char *out, size_t cap)
{
	if (cap == 0)
		return false;
	out[0] = '\0';
	iter it;
	iter_init(&it, doc, wrap, width);
	row r;
	for (size_t i = 0; iter_next(&it, &r); i++) {
		if (i != row_index)
			continue;
		text_out o = {out, cap, 0};
		walk_row(doc, &r, CTUI_RT_LEFT, INT_MAX / 2, INT_MAX / 2, 0,
			 text_run, &o);
		out[o.len] = '\0';
		return true;
	}
	return false;
}

size_t ctui_rt_max_top(const ctui_rt_doc *doc, int width, bool wrap, int height)
{
	size_t rows = ctui_rt_height(doc, width, wrap);
	return height > 0 && rows > (size_t)height ? rows - (size_t)height : 0;
}

bool ctui_rt_scroll(ctui_rt_view *view, const ctui_rt_doc *doc, int width,
		    int height, int delta)
{
	size_t max = ctui_rt_max_top(doc, width, view->wrap, height),
	       old = view->top, top = old;
	if (top > max)
		top = max;
	if (delta < 0)
		top = (size_t)-(long long)delta > top
			  ? 0
			  : top - (size_t)-(long long)delta;
	else
		top = (size_t)delta > max - top ? max : top + (size_t)delta;
	view->top = top;
	return top != old;
}

/* ---- drawing and hit testing ---- */

typedef struct draw_ctx {
	const ctui_rt_doc *d;
	int y, x, width, attr;
} draw_ctx;

static bool draw_run(void *user, int ry, size_t span, const char *text,
		     size_t len, int col, int cols, int pad)
{
	draw_ctx *c = user;
	if (span != (size_t)-1)
		c->attr = ctui_rt_attr(&c->d->spans[span]);
	else if (!c->attr)
		c->attr = ctui_attr(CTUI_PAIR_BODY, false);
	if (pad > 0) {
		ctui_fill(c->y + ry, c->x + col, pad, c->attr);
		return true;
	}
	(void)cols;
	while (len > 0 && col < c->width) {
		char buf[CTUI_TEXT_MAX];
		size_t n = len < sizeof buf - 1 ? len : sizeof buf - 1;
		while (n < len && n > 1 &&
		       ((unsigned)(unsigned char)text[n] & 0xC0U) == 0x80U)
			n--;
		memcpy(buf, text, n);
		buf[n] = '\0';
		col += ctui_put(c->y + ry, c->x + col, c->width - col, buf,
				c->attr);
		text += n;
		len -= n;
	}
	return true;
}

int ctui_rt_draw(const ctui_rt_doc *doc, const ctui_rt_view *view, int y, int x,
		 int width, int height)
{
	draw_ctx c = {.d = doc, .y = y, .x = x, .width = width};
	return visit(doc, view, width, height, draw_run, &c);
}

typedef struct hit_ctx {
	int ry, rx;
	size_t span;
	bool found;
} hit_ctx;

static bool hit_run(void *user, int ry, size_t span, const char *text,
		    size_t len, int col, int cols, int pad)
{
	(void)text;
	(void)len;
	hit_ctx *h = user;
	if (ry == h->ry && span != (size_t)-1 && h->rx >= col &&
	    h->rx < col + cols) {
		(void)pad;
		h->span = span;
		h->found = true;
		return false;
	}
	return true;
}

bool ctui_rt_hit(const ctui_rt_doc *doc, const ctui_rt_view *view, int width,
		 int height, int ry, int rx, size_t *span)
{
	hit_ctx h = {.ry = ry, .rx = rx};
	if (ry < 0 || rx < 0 || ry >= height || rx >= width)
		return false;
	visit(doc, view, width, height, hit_run, &h);
	if (h.found && span)
		*span = h.span;
	return h.found;
}

int ctui_rt_link_at(const ctui_rt_doc *doc, const ctui_rt_view *view, int width,
		    int height, int ry, int rx)
{
	size_t s;
	return ctui_rt_hit(doc, view, width, height, ry, rx, &s)
		   ? doc->spans[s].link
		   : 0;
}
