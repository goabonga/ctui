/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <ctui/richtext.h>
#include <ctui/text.h>

#define CAP 512

static char g_text[CAP];
static ctui_span g_spans[CAP];
static ctui_rt_line g_lines[CAP];

static ctui_rt_doc parse(const char *markup)
{
	ctui_rt_doc d;
	ctui_rt_doc_init(&d, g_text, sizeof g_text, g_spans, CAP, g_lines, CAP);
	ctui_rt_parse(&d, markup);
	return d;
}

static bool span_is(const ctui_rt_doc *d, size_t i, const char *text)
{
	return i < d->span_count && d->spans[i].len == strlen(text) &&
	       !memcmp(d->spans[i].text, text, d->spans[i].len);
}

static void test_parse(void)
{
	ctui_rt_doc d = parse("a [b]bold[/b] [fg=err]red[/] [link=7]go[/link]");
	CHECK(d.line_count == 1 && d.span_count == 6);
	CHECK(span_is(&d, 0, "a ") && d.spans[0].attrs == 0);
	CHECK(span_is(&d, 1, "bold") && d.spans[1].attrs == CTUI_RT_BOLD);
	CHECK(span_is(&d, 3, "red") && d.spans[3].pair == CTUI_PAIR_ERROR);
	CHECK(span_is(&d, 5, "go") && d.spans[5].link == 7);
	CHECK(span_is(&d, 4, " ") && d.spans[4].pair == 0 &&
	      d.spans[4].link == 0);

	d = parse("[b][i]x[/i]y[/b]z");
	CHECK(d.spans[0].attrs == (CTUI_RT_BOLD | CTUI_RT_ITALIC));
	CHECK(d.spans[1].attrs == CTUI_RT_BOLD && d.spans[2].attrs == 0);

	d = parse("one\n\ntwo\n");
	CHECK(d.line_count == 3 && d.lines[1].count == 0 &&
	      span_is(&d, 1, "two"));
	d = parse("");
	CHECK(d.line_count == 0);
	d = parse("\n");
	CHECK(d.line_count == 1 && d.lines[0].count == 0);
	d = parse("a\r\nb");
	CHECK(d.line_count == 2 && span_is(&d, 0, "a"));
}

static void test_parse_literal(void)
{
	ctui_rt_doc d = parse("\\[b] \\\\ \\x");
	CHECK(d.span_count == 1 && span_is(&d, 0, "[b] \\ \\x"));
	d = parse("[x]a[/b][fg=nope][link=0][link=12x][/]");
	CHECK(d.span_count == 1 &&
	      span_is(&d, 0, "[x]a[/b][fg=nope][link=0][link=12x][/]"));
	d = parse("[b]a[/i]b[/b]"); /* wrong closer is text */
	CHECK(d.span_count == 1 && span_is(&d, 0, "a[/i]b") &&
	      d.spans[0].attrs == CTUI_RT_BOLD);
	d = parse("[b]open to the end");
	CHECK(d.span_count == 1 && d.spans[0].attrs == CTUI_RT_BOLD);
	d = parse("tail [");
	CHECK(span_is(&d, 0, "tail ["));
	d = parse("[[b]x[/b]]");
	CHECK(span_is(&d, 0, "[") && span_is(&d, 1, "x") &&
	      span_is(&d, 2, "]"));
	d = parse("[link=1234567890]x"); /* too long an id */
	CHECK(span_is(&d, 0, "[link=1234567890]x"));
	/* Nesting past the limit is literal. */
	char deep[256] = "";
	size_t used = 0;
	for (int i = 0; i < CTUI_RT_DEPTH + 2; i++)
		used +=
		    (size_t)snprintf(deep + used, sizeof deep - used, "[b]");
	deep[used] = 'x';
	d = parse(deep);
	CHECK(d.span_count == 1 && d.spans[0].len == 6 + 1);
	CHECK(parse(NULL).line_count == 0);
}

static void test_parse_overflow(void)
{
	ctui_rt_doc d;
	char text[4];
	ctui_rt_doc_init(&d, text, sizeof text, g_spans, CAP, g_lines, CAP);
	CHECK(!ctui_rt_parse(&d, "abcdefgh") && d.overflow && d.text_len == 4);
	ctui_span one[1];
	ctui_rt_doc_init(&d, g_text, sizeof g_text, one, 1, g_lines, CAP);
	CHECK(!ctui_rt_parse(&d, "a[b]b[/b]c") && d.span_count <= 1);
	ctui_rt_line none[1];
	ctui_rt_doc_init(&d, g_text, sizeof g_text, g_spans, CAP, none, 1);
	CHECK(!ctui_rt_parse(&d, "a\nb\nc") && d.line_count == 1);
	ctui_rt_doc_init(&d, g_text, sizeof g_text, g_spans, CAP, g_lines, CAP);
	CHECK(ctui_rt_parse(&d, "fits") && !d.overflow);
}

static unsigned g_seed = 12345;
static unsigned rnd(void)
{
	g_seed = g_seed * 1103515245u + 12345u;
	return (g_seed >> 16U) & 0x7fffU;
}

static void check_invariants(const ctui_rt_doc *d)
{
	size_t next = 0;
	for (size_t l = 0; l < d->line_count; l++) {
		CHECK(d->lines[l].first == next);
		next += d->lines[l].count;
	}
	CHECK(next <= d->span_count);
	for (size_t i = 0; i < d->span_count; i++)
		CHECK(d->spans[i].text >= d->text &&
		      d->spans[i].text + d->spans[i].len <=
			  d->text + d->text_len &&
		      d->spans[i].len > 0);
}

static void test_fuzz(void)
{
	static const char alphabet[] =
	    "[]/\\\nbdiurfgl=0123456789 xyz\xc3\xa9\xe6\x97\xa5\xff-";
	char in[200];
	for (int round = 0; round < 3000; round++) {
		size_t n = rnd() % (sizeof in - 1);
		for (size_t i = 0; i < n; i++)
			in[i] = alphabet[rnd() % (sizeof alphabet - 1)];
		in[n] = '\0';
		ctui_rt_doc d;
		ctui_rt_doc_init(&d, g_text, n + 1, g_spans, n + 1, g_lines,
				 n + 1);
		CHECK(ctui_rt_parse(&d, in)); /* strlen + 1 is always enough */
		check_invariants(&d);
		int width = 1 + (int)(rnd() % 20);
		bool wrap = (rnd() & 1U) != 0;
		size_t rows = ctui_rt_height(&d, width, wrap);
		CHECK(rows >= d.line_count);
		char row[CAP];
		size_t total = 0;
		for (size_t r = 0; r < rows; r++) {
			CHECK(ctui_rt_row_text(&d, width, wrap, r, row,
					       sizeof row));
			total += strlen(row);
		}
		CHECK(total <= d.text_len);
		CHECK(
		    !ctui_rt_row_text(&d, width, wrap, rows, row, sizeof row));
		/* Tiny storage must never overrun. */
		ctui_rt_doc small;
		ctui_rt_doc_init(&small, g_text, n / 2, g_spans, n / 3, g_lines,
				 n / 4);
		ctui_rt_parse(&small, in);
		check_invariants(&small);
	}
}

static void rows_are(const ctui_rt_doc *d, int width, const char *const *want,
		     size_t n)
{
	CHECK(ctui_rt_height(d, width, true) == n);
	char row[CAP];
	for (size_t i = 0; i < n; i++) {
		CHECK(ctui_rt_row_text(d, width, true, i, row, sizeof row));
		CHECK_STR(row, want[i]);
	}
}

static void test_wrap(void)
{
	ctui_rt_doc d = parse("the [b]quick brown[/b] fox");
	const char *a[] = {"the quick", "brown fox"};
	rows_are(&d, 12, a, 2);
	const char *b[] = {"the", "quick", "brown", "fox"};
	rows_are(&d, 5, b, 4);
	CHECK(ctui_rt_height(&d, 80, true) == 1);
	CHECK(ctui_rt_height(&d, 0, true) == 0);
	CHECK(ctui_rt_height(&d, 3, false) == 1);

	d = parse("abcdefghij");
	const char *c[] = {"abcd", "efgh", "ij"};
	rows_are(&d, 4, c, 3);

	d = parse("[b]ab[/b]cd[i]ef[/i]gh"); /* a word spanning three spans */
	const char *e[] = {"abcde", "fgh"};
	rows_are(&d, 5, e, 2);

	d = parse("one   two"); /* a space run at a break disappears */
	const char *f[] = {"one", "two"};
	rows_are(&d, 5, f, 2);
	d = parse("  indented"); /* leading spaces of a paragraph stay */
	char row[CAP];
	ctui_rt_row_text(&d, 20, true, 0, row, sizeof row);
	CHECK_STR(row, "  indented");
	d = parse("a\n\nb");
	CHECK(ctui_rt_height(&d, 10, true) == 3);
	d = parse("trailing   ");
	CHECK(ctui_rt_height(&d, 8, true) ==
	      1); /* 11 columns, wrapped: "trailing" then nothing */
}

static void test_wrap_wide(void)
{
	/* Each ideograph is two columns. */
	ctui_rt_doc d =
	    parse("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae");
	const char *a[] = {"\xe6\x97\xa5\xe6\x9c\xac",
			   "\xe8\xaa\x9e\xe3\x81\xae"};
	rows_are(&d, 4, a, 2);
	rows_are(&d, 5, a, 2); /* the fifth column stays empty */
	CHECK(ctui_rt_height(&d, 1, true) ==
	      4); /* wider than the row: taken alone */
	d = parse("e\xcc\x81"
		  "e\xcc\x81"
		  "e\xcc\x81"); /* combining marks take no column */
	CHECK(ctui_rt_height(&d, 3, true) == 1);
	CHECK(ctui_rt_height(&d, 2, true) == 2);
	d = parse("[b]\xe6\x97\xa5[/b]\xe6\x97\xa5 x");
	char row[CAP];
	CHECK(ctui_rt_height(&d, 4, true) == 2);
	ctui_rt_row_text(&d, 4, true, 1, row, sizeof row);
	CHECK_STR(row, "x");
	d = parse("\xff\xfe\x01"); /* invalid bytes are one column each */
	CHECK(ctui_rt_height(&d, 2, true) == 2);
}

static void test_scroll(void)
{
	ctui_rt_doc d = parse("l1\nl2\nl3\nl4\nl5\nl6");
	ctui_rt_view v = {.wrap = true};
	CHECK(ctui_rt_max_top(&d, 10, true, 4) == 2);
	CHECK(ctui_rt_max_top(&d, 10, true, 10) == 0);
	CHECK(ctui_rt_scroll(&v, &d, 10, 4, 1) && v.top == 1);
	CHECK(ctui_rt_scroll(&v, &d, 10, 4, 50) && v.top == 2);
	CHECK(!ctui_rt_scroll(&v, &d, 10, 4, 1));
	CHECK(ctui_rt_scroll(&v, &d, 10, 4, -99) && v.top == 0);
	CHECK(!ctui_rt_scroll(&v, &d, 10, 4, -1));
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_rt_doc d = parse("[fg=err]red[/] plain [b]bold[/b]\nsecond line");
	ctui_rt_view v = {.wrap = true};
	screen_clear();
	CHECK(ctui_rt_draw(&d, &v, 0, 0, 40, 5) == 2);
	CHECK(screen_has_text("red plain bold"));
	CHECK(screen_has_text("second line"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_ERROR, false)));
	v.top = 1;
	screen_clear();
	CHECK(ctui_rt_draw(&d, &v, 0, 0, 40, 5) == 1);
	CHECK(!screen_has_text("red") && screen_has_text("second line"));
	CHECK(ctui_rt_draw(&d, &v, 0, 0, 0, 5) == 0 &&
	      ctui_rt_draw(&d, &v, 0, 0, 5, 0) == 0);
	CHECK(ctui_rt_draw(NULL, &v, 0, 0, 5, 5) == 0);

	/* Clipped to the rectangle. */
	screen_clear();
	v.top = 0;
	v.wrap = false;
	ctui_rt_draw(&d, &v, 0, 0, 5, 1);
	CHECK(screen_has_text("red p") && !screen_has_text("red pl"));
}

static int cell_x(const char *needle)
{
	char row[256];
	for (int y = 0; y < LINES; y++)
		if (mvinnstr(y, 0, row, sizeof row - 1) != ERR) {
			char *at = strstr(row, needle);
			if (at)
				return (int)(at - row);
		}
	return -1;
}

static void test_align(void)
{
	screen_open(24, 80);
	ctui_rt_doc d = parse("abcd");
	ctui_rt_view v = {.wrap = true, .align = CTUI_RT_LEFT};
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 10, 1);
	CHECK(cell_x("abcd") == 0);
	v.align = CTUI_RT_CENTER;
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 10, 1);
	CHECK(cell_x("abcd") == 3);
	v.align = CTUI_RT_RIGHT;
	screen_clear();
	ctui_rt_draw(&d, &v, 2, 5, 10, 1);
	CHECK(cell_x("abcd") == 11);

	/* Justify: the first row fills the width, the last stays left. */
	d = parse("aa bb cc dd ee");
	v.align = CTUI_RT_JUSTIFY;
	screen_clear();
	CHECK(ctui_rt_draw(&d, &v, 0, 0, 8, 3) == 2);
	CHECK(screen_has_text("aa bb cc") && screen_has_text("dd ee"));
	d = parse("aa bb cc dd ee ff");
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 9, 3);
	char row[64] = {0};
	mvinnstr(0, 0, row, 9);
	CHECK(strncmp(row, "aa  bb  c", 9) != 0 && row[0] == 'a' &&
	      row[8] == 'c');
	mvinnstr(1, 0, row, 9);
	CHECK(strncmp(row, "dd ee ff", 8) == 0);
	/* No wrap: justify behaves as left. */
	v.wrap = false;
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 20, 1);
	CHECK(cell_x("aa bb cc dd ee") == 0);
}

static void test_ellipsis(void)
{
	screen_open(24, 80);
	ctui_rt_doc d = parse("0123456789abcdef");
	ctui_rt_view v = {.wrap = false, .ellipsis = true};
	const char *ell = ctui_theme_glyphs()->ellipsis;
	size_t ew = ctui_utf8_columns(ell);
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 10, 1);
	char want[32];
	size_t keep = 10 - ew;
	memcpy(want, "0123456789", keep);
	memcpy(want + keep, ell, strlen(ell) + 1);
	char row[64];
	mvinnstr(0, 0, row, 40);
	CHECK(strncmp(row, want, keep + strlen(ell)) == 0);
	CHECK(!screen_has_text("abcdef"));
	/* No ellipsis when it fits or when it is off. */
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 16, 1);
	CHECK(screen_has_text("0123456789abcdef"));
	v.ellipsis = false;
	screen_clear();
	ctui_rt_draw(&d, &v, 0, 0, 10, 1);
	CHECK(screen_has_text("0123456789") && !screen_has_text(ell));
	/* Wrapped: the last visible row announces the rest. */
	d = parse("one two three four five six");
	v = (ctui_rt_view){.wrap = true, .ellipsis = true};
	screen_clear();
	CHECK(ctui_rt_draw(&d, &v, 0, 0, 9, 2) == 2);
	CHECK(screen_has_text("one two") && screen_has_text(ell));
	screen_clear();
	v.top = ctui_rt_max_top(&d, 9, true, 2);
	ctui_rt_draw(&d, &v, 0, 0, 9, 2);
	CHECK(!screen_has_text(ell) && screen_has_text("six"));
}

static void test_hit(void)
{
	screen_open(24, 80);
	ctui_rt_doc d =
	    parse("see [link=3]the docs[/link] now\n[link=9]next[/]");
	ctui_rt_view v = {.wrap = true};
	size_t s = 99;
	CHECK(ctui_rt_hit(&d, &v, 30, 5, 0, 0, &s) && s == 0 &&
	      d.spans[s].link == 0);
	CHECK(ctui_rt_link_at(&d, &v, 30, 5, 0, 4) == 3);
	CHECK(ctui_rt_link_at(&d, &v, 30, 5, 0, 11) == 3);
	CHECK(ctui_rt_link_at(&d, &v, 30, 5, 0, 12) == 0);
	CHECK(ctui_rt_link_at(&d, &v, 30, 5, 1, 0) == 9);
	CHECK(!ctui_rt_hit(&d, &v, 30, 5, 0, 29, &s)); /* past the text */
	CHECK(!ctui_rt_hit(&d, &v, 30, 5, 2, 0, &s));  /* below it */
	CHECK(!ctui_rt_hit(&d, &v, 30, 5, -1, 0, &s) &&
	      !ctui_rt_hit(&d, &v, 30, 5, 0, 30, &s));
	/* Wrapped and scrolled: the same cell as drawn. */
	CHECK(ctui_rt_link_at(&d, &v, 8, 5, 1, 0) == 3); /* "the docs" */
	v.top = 1;
	CHECK(ctui_rt_link_at(&d, &v, 8, 5, 0, 0) == 3);
	/* Alignment moves the hit area. */
	v = (ctui_rt_view){.wrap = true, .align = CTUI_RT_RIGHT};
	d = parse("ab [link=4]cd[/]");
	CHECK(ctui_rt_link_at(&d, &v, 10, 1, 0, 7) == 0);
	CHECK(ctui_rt_link_at(&d, &v, 10, 1, 0, 8) == 4);
	CHECK(ctui_rt_link_at(&d, &v, 10, 1, 0, 9) == 4);
	/* Wide characters: the link starts after two columns. */
	v = (ctui_rt_view){.wrap = true};
	d = parse("\xe6\x97\xa5[link=2]x[/]");
	CHECK(ctui_rt_link_at(&d, &v, 10, 1, 0, 1) == 0 &&
	      ctui_rt_link_at(&d, &v, 10, 1, 0, 2) == 2);
	/* Justified padding stays in the link of the gap. */
	d = parse("[link=5]aa bb[/] cc dd");
	v.align = CTUI_RT_JUSTIFY;
	CHECK(ctui_rt_link_at(&d, &v, 9, 2, 0, 3) == 5);
	CHECK(ctui_rt_link_at(&d, &v, 9, 2, 1, 0) == 0);
}

static void test_attr(void)
{
	screen_open(24, 80);
	ctui_span s = {.text = "x",
		       .len = 1,
		       .attrs = CTUI_RT_BOLD | CTUI_RT_UNDERLINE |
				CTUI_RT_REVERSE | CTUI_RT_DIM};
	int a = ctui_rt_attr(&s);
	CHECK(((unsigned)a & A_BOLD) && ((unsigned)a & A_UNDERLINE) &&
	      ((unsigned)a & A_REVERSE) && ((unsigned)a & A_DIM));
	s.attrs = 0;
	s.pair = CTUI_PAIR_ERROR;
	CHECK(PAIR_NUMBER((chtype)ctui_rt_attr(&s)) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_ERROR, false)));
	/* A hand-built document works like a parsed one. */
	ctui_span spans[2] = {{.text = "ab ", .len = 3},
			      {.text = "cd", .len = 2, .link = 1}};
	ctui_rt_line lines[1] = {{0, 2}};
	ctui_rt_doc d = {
	    .spans = spans, .span_count = 2, .lines = lines, .line_count = 1};
	ctui_rt_view v = {.wrap = true};
	CHECK(ctui_rt_height(&d, 3, true) == 2 &&
	      ctui_rt_link_at(&d, &v, 10, 1, 0, 3) == 1);
}

int main(void)
{
	RUN(test_parse);
	RUN(test_parse_literal);
	RUN(test_parse_overflow);
	RUN(test_fuzz);
	RUN(test_wrap);
	RUN(test_wrap_wide);
	RUN(test_scroll);
	RUN(test_draw);
	RUN(test_align);
	RUN(test_ellipsis);
	RUN(test_hit);
	RUN(test_attr);
	return CHECK_DONE();
}
