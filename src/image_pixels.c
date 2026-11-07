/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * image_pixels.c - Kitty and Sixel graphics for images (image.h).
 */
#include "image_pixels.h"

#include <curses.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define KITTY_CHUNK 4096 /* base64 characters in a chunk */
#define KITTY_ID 7
#define DEFAULT_CELL_W 8
#define DEFAULT_CELL_H 16
#define SIXEL_COLORS 256

/* ---- detection -------------------------------------------------- */

ctui_image_protocol ctui_image_protocol_for(const char *term,
					    const char *term_program,
					    bool kitty_window)
{
	if (kitty_window)
		return CTUI_PROTOCOL_KITTY;
	if (term && (strstr(term, "kitty") || strstr(term, "ghostty")))
		return CTUI_PROTOCOL_KITTY;
	if (term_program && (strcmp(term_program, "WezTerm") == 0 ||
			     strcmp(term_program, "ghostty") == 0))
		return CTUI_PROTOCOL_KITTY;
	if (term && (strstr(term, "sixel") || strncmp(term, "foot", 4) == 0 ||
		     strstr(term, "mlterm") || strstr(term, "contour")))
		return CTUI_PROTOCOL_SIXEL;
	if (term_program && strcmp(term_program, "iTerm.app") == 0)
		return CTUI_PROTOCOL_SIXEL;
	return CTUI_PROTOCOL_NONE;
}

ctui_image_protocol ctui_image_protocol_detect(void)
{
	const char *forced = getenv("CTUI_IMAGE_PROTOCOL");
	if (forced && *forced) {
		if (strcmp(forced, "kitty") == 0)
			return CTUI_PROTOCOL_KITTY;
		if (strcmp(forced, "sixel") == 0)
			return CTUI_PROTOCOL_SIXEL;
		return CTUI_PROTOCOL_NONE;
	}
	/* a multiplexer swallows the sequences */
	if (getenv("TMUX") || getenv("STY"))
		return CTUI_PROTOCOL_NONE;
	return ctui_image_protocol_for(getenv("TERM"), getenv("TERM_PROGRAM"),
				       getenv("KITTY_WINDOW_ID") != NULL);
}

void ctui_image_cell_pixels(int *width, int *height)
{
	*width = DEFAULT_CELL_W;
	*height = DEFAULT_CELL_H;
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col &&
	    ws.ws_row && ws.ws_xpixel && ws.ws_ypixel) {
		int w = ws.ws_xpixel / ws.ws_col, h = ws.ws_ypixel / ws.ws_row;
		if (w >= 4 && w <= 64 && h >= 4 && h <= 128) {
			*width = w;
			*height = h;
		}
	}
}

/* ---- a growing string ------------------------------------------- */

typedef struct sb {
	char *data;
	size_t len, cap;
	bool failed;
} sb;

static void sb_put(sb *b, const char *s, size_t n)
{
	if (b->failed)
		return;
	if (b->len + n + 1 > b->cap) {
		size_t cap = b->cap ? b->cap : 4096;
		while (cap < b->len + n + 1)
			cap *= 2;
		char *p = realloc(b->data, cap);
		if (!p) {
			b->failed = true;
			return;
		}
		b->data = p;
		b->cap = cap;
	}
	memcpy(b->data + b->len, s, n);
	b->len += n;
	b->data[b->len] = '\0';
}

static void sb_str(sb *b, const char *s)
{
	sb_put(b, s, strlen(s));
}

static char *sb_take(sb *b, size_t *len)
{
	if (b->failed) {
		free(b->data);
		return NULL;
	}
	if (len)
		*len = b->len;
	return b->data;
}

/* ---- Kitty ------------------------------------------------------ */

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Base64 of the n bytes into out (4 * ceil(n / 3) characters). */
static void base64(const unsigned char *in, size_t n, char *out)
{
	size_t o = 0;
	for (size_t i = 0; i < n; i += 3) {
		unsigned v = (unsigned)in[i] << 16U;
		if (i + 1 < n)
			v |= (unsigned)in[i + 1] << 8U;
		if (i + 2 < n)
			v |= (unsigned)in[i + 2];
		out[o++] = B64[(v >> 18U) & 63U];
		out[o++] = B64[(v >> 12U) & 63U];
		out[o++] = i + 1 < n ? B64[(v >> 6U) & 63U] : '=';
		out[o++] = i + 2 < n ? B64[v & 63U] : '=';
	}
}

char *ctui_image_encode_kitty(const ctui_rgb *px, int pw, int ph, int cols,
			      int rows, int id, size_t *len)
{
	if (!px || pw < 1 || ph < 1 || cols < 1 || rows < 1 ||
	    pw > CTUI_IMAGE_PIXELS_MAX || ph > CTUI_IMAGE_PIXELS_MAX)
		return NULL;
	size_t bytes = (size_t)pw * (size_t)ph * 3;
	unsigned char *raw = malloc(bytes);
	char *text = malloc((bytes + 2) / 3 * 4 + 1);
	if (!raw || !text) {
		free(raw);
		free(text);
		return NULL;
	}
	for (size_t i = 0; i < (size_t)pw * (size_t)ph; i++) {
		raw[i * 3] = px[i].r;
		raw[i * 3 + 1] = px[i].g;
		raw[i * 3 + 2] = px[i].b;
	}
	base64(raw, bytes, text);
	size_t total = (bytes + 2) / 3 * 4;
	text[total] = '\0';
	free(raw);

	sb out = {0};
	char head[128];
	for (size_t off = 0; off < total; off += KITTY_CHUNK) {
		size_t n =
		    total - off < KITTY_CHUNK ? total - off : KITTY_CHUNK;
		bool more = off + n < total;
		if (off == 0)
			snprintf(head, sizeof head,
				 "\033_Ga=T,f=24,s=%d,v=%d,c=%d,r=%d,i=%d,C=1,"
				 "q=2,m=%d;",
				 pw, ph, cols, rows, id, more ? 1 : 0);
		else
			snprintf(head, sizeof head, "\033_Gm=%d;",
				 more ? 1 : 0);
		sb_str(&out, head);
		sb_put(&out, text + off, n);
		sb_str(&out, "\033\\");
	}
	free(text);
	return sb_take(&out, len);
}

/* ---- Sixel ------------------------------------------------------ */

/* One run of `count` equal sixel characters. */
static void sixel_run(sb *b, int count, char ch)
{
	char buf[16];
	if (count > 3) {
		int n = snprintf(buf, sizeof buf, "!%d%c", count, ch);
		sb_put(b, buf, (size_t)n);
	} else {
		for (int i = 0; i < count; i++)
			sb_put(b, &ch, 1);
	}
}

char *ctui_image_encode_sixel(const ctui_rgb *px, int pw, int ph, size_t *len)
{
	if (!px || pw < 1 || ph < 1 || pw > CTUI_IMAGE_PIXELS_MAX ||
	    ph > CTUI_IMAGE_PIXELS_MAX)
		return NULL;
	size_t n = (size_t)pw * (size_t)ph;
	unsigned char *idx = malloc(n);
	if (!idx)
		return NULL;
	bool used[SIXEL_COLORS] = {false};
	for (size_t i = 0; i < n; i++) {
		idx[i] = (unsigned char)ctui_xterm_nearest(px[i]);
		used[idx[i]] = true;
	}
	sb out = {0};
	char buf[64];
	sb_str(&out, "\033Pq");
	snprintf(buf, sizeof buf, "\"1;1;%d;%d", pw, ph);
	sb_str(&out, buf);
	for (int c = 0; c < SIXEL_COLORS; c++) {
		if (!used[c])
			continue;
		ctui_rgb rgb = ctui_xterm_rgb(c);
		snprintf(buf, sizeof buf, "#%d;2;%d;%d;%d", c,
			 (rgb.r * 100 + 127) / 255, (rgb.g * 100 + 127) / 255,
			 (rgb.b * 100 + 127) / 255);
		sb_str(&out, buf);
	}
	for (int band = 0; band * 6 < ph; band++) {
		bool first = true;
		for (int c = 0; c < SIXEL_COLORS; c++) {
			if (!used[c])
				continue;
			/* the sixels of this colour in the band */
			char run = 0;
			int count = 0;
			bool any = false;
			sb line = {0};
			for (int x = 0; x < pw; x++) {
				unsigned bits = 0;
				for (int r = 0; r < 6; r++) {
					int y = band * 6 + r;
					if (y < ph &&
					    idx[(size_t)y * (size_t)pw +
						(size_t)x] == c)
						bits |= 1U << (unsigned)r;
				}
				char ch = (char)(63 + bits);
				any = any || bits != 0;
				if (count && ch == run) {
					count++;
					continue;
				}
				if (count)
					sixel_run(&line, count, run);
				run = ch;
				count = 1;
			}
			if (count)
				sixel_run(&line, count, run);
			if (any && !line.failed) {
				if (!first)
					sb_str(&out, "$");
				first = false;
				snprintf(buf, sizeof buf, "#%d", c);
				sb_str(&out, buf);
				sb_put(&out, line.data, line.len);
			}
			free(line.data);
		}
		sb_str(&out, "-");
	}
	sb_str(&out, "\033\\");
	free(idx);
	return sb_take(&out, len);
}

/* ---- drawing and flushing --------------------------------------- */

static struct {
	bool shown_now;	      /* drawn since the last flush */
	bool on_screen;	      /* a picture was sent and is still there */
	bool force;	      /* send again whatever the signature */
	uint64_t sig;	      /* of the picture drawn now */
	uint64_t sent_sig;    /* of the one on screen */
	int y, x, cols, rows; /* where it is now, in cells */
	int sent_y, sent_x, sent_cols, sent_rows;
	ctui_image_protocol protocol, sent_protocol;
	char *seq; /* its escape sequence */
	size_t len;
} g;

static uint64_t mix(uint64_t h, uint64_t v)
{
	h ^= v;
	return h * 1099511628211ULL;
}

static uint64_t mix_double(uint64_t h, double v)
{
	int64_t scaled = (int64_t)(v * 1000.0);
	return mix(h, (uint64_t)scaled);
}

/* What decides how the picture looks: the image (its address, size and a
 * sample of its bytes), the view and the place. */
static uint64_t signature(const ctui_image *img, const ctui_image_view *view,
			  const ctui_image_layout *lay, int y, int x,
			  ctui_image_protocol protocol, int cw, int ch)
{
	uint64_t h = 14695981039346656037ULL;
	h = mix(h, (uint64_t)(uintptr_t)img->pixels);
	h = mix(h, (uint64_t)img->width);
	h = mix(h, (uint64_t)img->height);
	h = mix(h, (uint64_t)img->channels);
	size_t total = (size_t)img->height * img->stride;
	for (size_t i = 0; i < 128; i++)
		h = mix(h, img->pixels[total / 128 * i]);
	h = mix(h, (uint64_t)view->scaler);
	h = mix(h, (uint64_t)view->fit);
	h = mix_double(h, view->zoom);
	h = mix_double(h, view->pan_x);
	h = mix_double(h, view->pan_y);
	h = mix(h, (uint64_t)view->background.r |
		       (uint64_t)view->background.g << 8U |
		       (uint64_t)view->background.b << 16U);
	h = mix(h, (uint64_t)y + (uint64_t)lay->row);
	h = mix(h, (uint64_t)x + (uint64_t)lay->col);
	h = mix(h, (uint64_t)lay->cols);
	h = mix(h, (uint64_t)lay->rows);
	h = mix(h, (uint64_t)protocol);
	h = mix(h, (uint64_t)cw * 256U + (uint64_t)ch);
	return h;
}

bool ctui_image_pixels_draw(const ctui_image *img, const ctui_image_view *view,
			    int y, int x, int cols, int rows)
{
	ctui_image_protocol protocol = ctui_image_protocol_detect();
	if (protocol == CTUI_PROTOCOL_NONE || !ctui_image_valid(img))
		return false;
	int cw, ch;
	ctui_image_cell_pixels(&cw, &ch);
	ctui_image_layout lay;
	if (!ctui_image_layout_pixels(img->width, img->height, cols, rows, cw,
				      ch, view, &lay))
		return false;
	uint64_t sig = signature(img, view, &lay, y, x, protocol, cw, ch);
	if (!g.seq || sig != g.sig) {
		size_t n = (size_t)lay.px_w * (size_t)lay.px_h;
		ctui_rgb *px = malloc(n * sizeof *px);
		if (!px || !ctui_image_resample(img, &lay, view->scaler,
						view->background, px)) {
			free(px);
			return false;
		}
		size_t len = 0;
		char *seq =
		    protocol == CTUI_PROTOCOL_KITTY
			? ctui_image_encode_kitty(px, lay.px_w, lay.px_h,
						  lay.cols, lay.rows, KITTY_ID,
						  &len)
			: ctui_image_encode_sixel(px, lay.px_w, lay.px_h, &len);
		free(px);
		if (!seq)
			return false;
		free(g.seq);
		g.seq = seq;
		g.len = len;
		g.sig = sig;
	}
	g.shown_now = true;
	g.protocol = protocol;
	g.y = y + lay.row;
	g.x = x + lay.col;
	g.cols = lay.cols;
	g.rows = lay.rows;
	/* the cells under the picture are blank for curses */
	int plain = ctui_attr(CTUI_PAIR_BODY, false);
	for (int j = 0; j < lay.rows; j++)
		for (int i = 0; i < lay.cols; i++)
			ctui_put(g.y + j, g.x + i, 1, " ", plain);
	return true;
}

void ctui_image_pixels_reset(void)
{
	g.force = true;
}

/* Take the picture off the screen: Kitty deletes the image, Sixel pixels go
 * when the cells under them are written again. */
static void take_off(FILE *out)
{
	if (g.sent_protocol == CTUI_PROTOCOL_KITTY) {
		fprintf(out, "\033_Ga=d,d=I,i=%d,q=2\033\\", KITTY_ID);
	} else if (g.sent_protocol == CTUI_PROTOCOL_SIXEL && stdscr) {
		wredrawln(stdscr, g.sent_y, g.sent_rows);
		wrefresh(stdscr);
	}
	g.on_screen = false;
}

void ctui_image_flush(FILE *out)
{
	if (!out)
		out = stdout;
	if (g.shown_now) {
		bool moved = g.on_screen &&
			     (g.sent_y != g.y || g.sent_x != g.x ||
			      g.sent_cols != g.cols || g.sent_rows != g.rows ||
			      g.sent_protocol != g.protocol);
		if (g.force || g.sig != g.sent_sig || !g.on_screen) {
			if (moved || (g.force && g.on_screen))
				take_off(out);
			fprintf(out, "\0337\033[%d;%dH", g.y + 1, g.x + 1);
			fwrite(g.seq, 1, g.len, out);
			fputs("\0338", out);
			g.on_screen = true;
			g.force = false;
			g.sent_sig = g.sig;
			g.sent_y = g.y;
			g.sent_x = g.x;
			g.sent_cols = g.cols;
			g.sent_rows = g.rows;
			g.sent_protocol = g.protocol;
		}
	} else if (g.on_screen) {
		take_off(out);
		g.sent_sig = 0;
	}
	g.shown_now = false;
	fflush(out);
}
