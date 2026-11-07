/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_image_pixels.c - pixel graphics for images: finding the protocol of
 * the terminal, the layout in real pixels, the Kitty and Sixel encoders and
 * the draw / flush cycle.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/image.h>

static void test_protocol_for(void)
{
	CHECK(ctui_image_protocol_for("xterm-kitty", NULL, false) ==
	      CTUI_PROTOCOL_KITTY);
	CHECK(ctui_image_protocol_for("xterm-256color", NULL, true) ==
	      CTUI_PROTOCOL_KITTY);
	CHECK(ctui_image_protocol_for("xterm-ghostty", NULL, false) ==
	      CTUI_PROTOCOL_KITTY);
	CHECK(ctui_image_protocol_for("xterm-256color", "WezTerm", false) ==
	      CTUI_PROTOCOL_KITTY);
	CHECK(ctui_image_protocol_for("xterm-256color", "ghostty", false) ==
	      CTUI_PROTOCOL_KITTY);
	CHECK(ctui_image_protocol_for("foot", NULL, false) ==
	      CTUI_PROTOCOL_SIXEL);
	CHECK(ctui_image_protocol_for("foot-extra", NULL, false) ==
	      CTUI_PROTOCOL_SIXEL);
	CHECK(ctui_image_protocol_for("mlterm", NULL, false) ==
	      CTUI_PROTOCOL_SIXEL);
	CHECK(ctui_image_protocol_for("xterm-sixel", NULL, false) ==
	      CTUI_PROTOCOL_SIXEL);
	CHECK(ctui_image_protocol_for("xterm-256color", "iTerm.app", false) ==
	      CTUI_PROTOCOL_SIXEL);
	CHECK(ctui_image_protocol_for("xterm-256color", "Apple_Terminal",
				      false) == CTUI_PROTOCOL_NONE);
	CHECK(ctui_image_protocol_for("linux", NULL, false) ==
	      CTUI_PROTOCOL_NONE);
	CHECK(ctui_image_protocol_for(NULL, NULL, false) == CTUI_PROTOCOL_NONE);
}

static void test_protocol_detect(void)
{
	unsetenv("TMUX");
	unsetenv("STY");
	unsetenv("KITTY_WINDOW_ID");
	unsetenv("TERM_PROGRAM");
	unsetenv("CTUI_IMAGE_PROTOCOL");
	setenv("TERM", "xterm-kitty", 1);
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_KITTY);
	setenv("TMUX", "/tmp/tmux-1/default,1,0", 1); /* swallows them */
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_NONE);
	setenv("CTUI_IMAGE_PROTOCOL", "sixel", 1); /* but the user knows */
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_SIXEL);
	setenv("CTUI_IMAGE_PROTOCOL", "kitty", 1);
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_KITTY);
	setenv("CTUI_IMAGE_PROTOCOL", "none", 1);
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_NONE);
	setenv("CTUI_IMAGE_PROTOCOL", "bogus", 1);
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_NONE);
	unsetenv("CTUI_IMAGE_PROTOCOL");
	unsetenv("TMUX");
	setenv("TERM", "linux", 1);
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_NONE);
	setenv("KITTY_WINDOW_ID", "3", 1);
	CHECK(ctui_image_protocol_detect() == CTUI_PROTOCOL_KITTY);
	unsetenv("KITTY_WINDOW_ID");
}

static void test_cell_pixels(void)
{
	int w = 0, h = 0;
	ctui_image_cell_pixels(&w, &h);
	CHECK(w >= 4 && w <= 64 && h >= 4 && h <= 128);
}

static void test_layout_pixels(void)
{
	ctui_image_view v = ctui_image_view_default();
	ctui_image_layout l;
	/* a 200 x 100 image in 40 x 20 cells of 10 x 20 pixels: it spans the
	 * 40 columns and half the rows, in proportion */
	CHECK(ctui_image_layout_pixels(200, 100, 40, 20, 10, 20, &v, &l));
	CHECK(l.cols == 40 && l.rows == 10 && l.col == 0 && l.row == 5);
	CHECK(l.px_w == 400 && l.px_h == 200);
	/* square cells: it spans the 40 columns and all the 20 rows */
	CHECK(ctui_image_layout_pixels(200, 100, 40, 20, 8, 8, &v, &l));
	CHECK(l.cols == 40 && l.rows == 20 && l.px_w == 320 && l.px_h == 160);
	/* stretched */
	v.fit = CTUI_IMAGE_STRETCH;
	CHECK(ctui_image_layout_pixels(200, 100, 30, 12, 10, 20, &v, &l));
	CHECK(l.cols == 30 && l.rows == 12 && l.px_w == 300 && l.px_h == 240);
	/* the pixel grid is bounded */
	CHECK(ctui_image_layout_pixels(200, 100, 500, 500, 10, 20, &v, &l));
	CHECK(l.px_w <= CTUI_IMAGE_PIXELS_MAX &&
	      l.px_h <= CTUI_IMAGE_PIXELS_MAX);
	CHECK(!ctui_image_layout_pixels(200, 100, 40, 20, 0, 20, &v, &l));
	CHECK(!ctui_image_layout_pixels(0, 100, 40, 20, 10, 20, &v, &l));
	CHECK(!ctui_image_layout_pixels(200, 100, 0, 20, 10, 20, &v, &l));
}

static int b64_value(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	return c == '+' ? 62 : c == '/' ? 63 : -1;
}

/* Decode the base64 text into out; the number of bytes. */
static size_t b64_decode(const char *s, size_t n, unsigned char *out)
{
	size_t o = 0;
	for (size_t i = 0; i + 3 < n; i += 4) {
		int a = b64_value(s[i]), b = b64_value(s[i + 1]);
		int c = s[i + 2] == '=' ? 0 : b64_value(s[i + 2]);
		int d = s[i + 3] == '=' ? 0 : b64_value(s[i + 3]);
		unsigned v = (unsigned)a << 18U | (unsigned)b << 12U |
			     (unsigned)c << 6U | (unsigned)d;
		out[o++] = (unsigned char)(v >> 16U);
		if (s[i + 2] != '=')
			out[o++] = (unsigned char)(v >> 8U);
		if (s[i + 3] != '=')
			out[o++] = (unsigned char)v;
	}
	return o;
}

static void test_kitty(void)
{
	size_t len = 0;
	const ctui_rgb two[2] = {{1, 2, 3}, {4, 5, 6}};
	char *s = ctui_image_encode_kitty(two, 2, 1, 2, 1, 9, &len);
	CHECK(s != NULL && len == strlen(s));
	CHECK_STR(s, "\033_Ga=T,f=24,s=2,v=1,c=2,r=1,i=9,C=1,q=2,m=0;AQIDBAUG"
		     "\033\\");
	free(s);

	/* 100 x 100 pixels: 30000 bytes, 40000 characters, ten chunks */
	ctui_rgb *big = malloc((size_t)100 * 100 * sizeof *big);
	CHECK(big != NULL);
	if (!big)
		return;
	for (unsigned i = 0; i < 100 * 100; i++)
		big[i] = (ctui_rgb){(unsigned char)i, (unsigned char)(i >> 3U),
				    (unsigned char)(i * 7U)};
	s = ctui_image_encode_kitty(big, 100, 100, 20, 10, 7, &len);
	CHECK(s != NULL);
	if (s) {
		int chunks = 0, more_ok = 1;
		char *payload = malloc(len);
		size_t pn = 0;
		const char *p = s;
		while ((p = strstr(p, "\033_G")) != NULL) {
			const char *semi = strchr(p, ';');
			const char *end = strstr(p, "\033\\");
			const char *m = strstr(p, "m=");
			chunks++;
			/* every chunk but the last says m=1 */
			if (m && m < semi)
				more_ok =
				    more_ok &&
				    (m[2] == (end + 2 == s + len ? '0' : '1'));
			CHECK(end - semi - 1 <= 4096);
			memcpy(payload + pn, semi + 1,
			       (size_t)(end - semi - 1));
			pn += (size_t)(end - semi - 1);
			p = end;
		}
		CHECK(chunks == 10 && more_ok);
		CHECK(pn == 40000);
		unsigned char *raw = malloc(30000);
		CHECK(raw && b64_decode(payload, pn, raw) == 30000);
		CHECK(raw && raw[0] == big[0].r &&
		      raw[3 * 4321 + 1] == big[4321].g &&
		      raw[29999] == big[9999].b);
		free(raw);
		free(payload);
		free(s);
	}
	free(big);
	CHECK(ctui_image_encode_kitty(NULL, 2, 1, 2, 1, 1, &len) == NULL);
	CHECK(ctui_image_encode_kitty(two, 0, 1, 2, 1, 1, &len) == NULL);
	CHECK(ctui_image_encode_kitty(two, 2, 1, 0, 1, 1, &len) == NULL);
	CHECK(ctui_image_encode_kitty(two, 5000, 1, 2, 1, 1, &len) == NULL);
}

static void test_sixel(void)
{
	size_t len = 0;
	/* one column of six white pixels: palette 231, all six bits set */
	ctui_rgb white[6];
	for (int i = 0; i < 6; i++)
		white[i] = (ctui_rgb){255, 255, 255};
	char *s = ctui_image_encode_sixel(white, 1, 6, &len);
	CHECK(s != NULL && len == strlen(s));
	CHECK_STR(s, "\033Pq\"1;1;1;6#231;2;100;100;100#231~-\033\\");
	free(s);

	/* five columns, black over white: two colours in one band, and the run
	 * of five equal sixels is run length encoded */
	ctui_rgb two[30];
	for (int y = 0; y < 6; y++)
		for (int x = 0; x < 5; x++)
			two[y * 5 + x] = y < 3 ? (ctui_rgb){0, 0, 0}
					       : (ctui_rgb){255, 255, 255};
	s = ctui_image_encode_sixel(two, 5, 6, &len);
	CHECK(s != NULL);
	if (s)
		CHECK_STR(s, "\033Pq\"1;1;5;6#16;2;0;0;0#231;2;100;100;100"
			     "#16!5F$#231!5w-\033\\");
	free(s);

	/* a second band for the seventh row, shorter than six */
	ctui_rgb tall[7];
	for (int i = 0; i < 7; i++)
		tall[i] = (ctui_rgb){255, 255, 255};
	s = ctui_image_encode_sixel(tall, 1, 7, &len);
	CHECK(s != NULL);
	if (s)
		CHECK_STR(s, "\033Pq\"1;1;1;7#231;2;100;100;100#231~-#231@-"
			     "\033\\");
	free(s);

	CHECK(ctui_image_encode_sixel(NULL, 1, 1, &len) == NULL);
	CHECK(ctui_image_encode_sixel(white, 0, 6, &len) == NULL);
	CHECK(ctui_image_encode_sixel(white, 1, 5000, &len) == NULL);
}

static char *slurp(FILE *f)
{
	if (fseek(f, 0, SEEK_END) != 0)
		return NULL;
	long n = ftell(f);
	if (n < 0 || fseek(f, 0, SEEK_SET) != 0)
		return NULL;
	char *text = malloc((size_t)n + 1);
	if (!text)
		return NULL;
	size_t got = fread(text, 1, (size_t)n, f);
	text[got] = '\0';
	fseek(f, 0, SEEK_END); /* keep appending where it was */
	return text;
}

static void flush_to(FILE *f)
{
	ctui_image_flush(f);
}

static void test_draw_and_flush(void)
{
	screen_open(24, 80);
	unsigned char px[8 * 8 * 3];
	for (size_t i = 0; i < sizeof px; i++)
		px[i] = (unsigned char)(i * 5);
	ctui_image img = {
	    .pixels = px, .width = 8, .height = 8, .channels = 3, .stride = 24};
	ctui_image_view v = ctui_image_view_default();
	v.renderer = CTUI_IMAGE_PIXELS;
	unsetenv("TMUX");
	unsetenv("STY");

	/* no protocol: the cells of a plain terminal, nothing is sent */
	setenv("CTUI_IMAGE_PROTOCOL", "none", 1);
	FILE *f = tmpfile();
	CHECK(ctui_image_draw(&img, &v, 2, 3, 20, 10) == CTUI_IMAGE_HALF);
	flush_to(f);
	CHECK(ftell(f) == 0);
	fclose(f);

	/* Kitty: the cells are blank, the picture is sent once */
	setenv("CTUI_IMAGE_PROTOCOL", "kitty", 1);
	f = tmpfile();
	screen_clear();
	CHECK(ctui_image_draw(&img, &v, 2, 3, 20, 10) == CTUI_IMAGE_PIXELS);
	CHECK(!screen_has_text("\xe2\x96\x80")); /* no half blocks */
	flush_to(f);
	char *sent = slurp(f);
	CHECK(sent && strstr(sent, "\033_Ga=T,f=24,") != NULL);
	CHECK(sent && strstr(sent, "\0337\033[") != NULL &&
	      strstr(sent, "\0338") != NULL);
	free(sent);
	fclose(f);

	/* the same picture again: nothing more is sent */
	f = tmpfile();
	CHECK(ctui_image_draw(&img, &v, 2, 3, 20, 10) == CTUI_IMAGE_PIXELS);
	flush_to(f);
	CHECK(ftell(f) == 0);
	fclose(f);

	/* a zoom is another picture */
	f = tmpfile();
	v.zoom = 2;
	ctui_image_draw(&img, &v, 2, 3, 20, 10);
	flush_to(f);
	sent = slurp(f);
	CHECK(sent && strstr(sent, "a=T") != NULL &&
	      strstr(sent, "a=d") == NULL);
	free(sent);
	fclose(f);

	/* a reset sends it again */
	f = tmpfile();
	ctui_image_pixels_reset();
	ctui_image_draw(&img, &v, 2, 3, 20, 10);
	flush_to(f);
	sent = slurp(f);
	CHECK(sent && strstr(sent, "a=d") != NULL &&
	      strstr(sent, "a=T") != NULL);
	free(sent);
	fclose(f);

	/* a frame without the image takes it off, once */
	f = tmpfile();
	flush_to(f);
	sent = slurp(f);
	CHECK(sent && strstr(sent, "\033_Ga=d,d=I,i=7,q=2\033\\") != NULL);
	free(sent);
	flush_to(f);
	CHECK(ftell(f) > 0);
	long after_first = ftell(f);
	flush_to(f);
	CHECK(ftell(f) == after_first);
	fclose(f);

	/* Sixel: a DCS sequence, and a move redraws the old rows */
	setenv("CTUI_IMAGE_PROTOCOL", "sixel", 1);
	v.zoom = 1;
	f = tmpfile();
	ctui_image_draw(&img, &v, 2, 3, 20, 10);
	flush_to(f);
	sent = slurp(f);
	CHECK(sent && strstr(sent, "\033Pq\"1;1;") != NULL &&
	      strstr(sent, "\033\\") != NULL);
	free(sent);
	fclose(f);
	f = tmpfile();
	ctui_image_draw(&img, &v, 4, 5, 20, 10); /* moved */
	flush_to(f);
	sent = slurp(f);
	CHECK(sent && strstr(sent, "\033Pq") != NULL);
	free(sent);
	flush_to(f); /* gone */
	fclose(f);

	/* an invalid image draws nothing and sends nothing */
	f = tmpfile();
	ctui_image bad = {0};
	CHECK(ctui_image_draw(&bad, &v, 0, 0, 10, 10) == CTUI_IMAGE_AUTO);
	flush_to(f);
	CHECK(ftell(f) == 0);
	fclose(f);
	unsetenv("CTUI_IMAGE_PROTOCOL");
	endwin();
}

int main(void)
{
	RUN(test_protocol_for);
	RUN(test_protocol_detect);
	RUN(test_cell_pixels);
	RUN(test_layout_pixels);
	RUN(test_kitty);
	RUN(test_sixel);
	RUN(test_draw_and_flush);
	return CHECK_DONE();
}
