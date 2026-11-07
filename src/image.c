/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * image.c - pixel buffer display (image.h).
 */
#include <ctui/image.h>

#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_pixels.h"
#include <ctui/text.h>
#include <ctui/theme.h>

bool ctui_image_valid(const ctui_image *img)
{
	if (!img || !img->pixels || img->width < 1 || img->height < 1 ||
	    img->width > CTUI_IMAGE_MAX_DIM ||
	    img->height > CTUI_IMAGE_MAX_DIM ||
	    (img->channels != 3 && img->channels != 4))
		return false;
	return img->stride >= (size_t)img->width * (size_t)img->channels;
}

ctui_rgb ctui_image_pixel(const ctui_image *img, int x, int y, ctui_rgb bg)
{
	if (x < 0 || y < 0 || x >= img->width || y >= img->height)
		return bg;
	const unsigned char *p = img->pixels + (size_t)y * img->stride +
				 (size_t)x * (size_t)img->channels;
	if (img->channels == 3 || p[3] == 255)
		return (ctui_rgb){p[0], p[1], p[2]};
	unsigned a = p[3];
	return (ctui_rgb){
	    (unsigned char)((p[0] * a + bg.r * (255 - a) + 127) / 255),
	    (unsigned char)((p[1] * a + bg.g * (255 - a) + 127) / 255),
	    (unsigned char)((p[2] * a + bg.b * (255 - a) + 127) / 255)};
}

static const int CUBE[6] = {0, 95, 135, 175, 215, 255};
static const ctui_rgb ANSI16[16] = {
    {0, 0, 0},	     {205, 0, 0},   {0, 205, 0},   {205, 205, 0},
    {0, 0, 238},     {205, 0, 205}, {0, 205, 205}, {229, 229, 229},
    {127, 127, 127}, {255, 0, 0},   {0, 255, 0},   {255, 255, 0},
    {92, 92, 255},   {255, 0, 255}, {0, 255, 255}, {255, 255, 255}};

ctui_rgb ctui_xterm_rgb(int index)
{
	if (index < 0)
		index = 0;
	if (index > 255)
		index = 255;
	if (index < 16)
		return ANSI16[index];
	if (index >= 232) {
		unsigned char v = (unsigned char)(8 + 10 * (index - 232));
		return (ctui_rgb){v, v, v};
	}
	int i = index - 16;
	return (ctui_rgb){(unsigned char)CUBE[i / 36],
			  (unsigned char)CUBE[(i / 6) % 6],
			  (unsigned char)CUBE[i % 6]};
}

static int distance(ctui_rgb a, ctui_rgb b)
{
	int dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
	return dr * dr + dg * dg + db * db;
}

/* Index of the cube level nearest to v (the lower one on a tie). */
static int cube_level(int v)
{
	int best = 0;
	for (int i = 1; i < 6; i++)
		if (abs(v - CUBE[i]) < abs(v - CUBE[best]))
			best = i;
	return best;
}

int ctui_xterm_nearest(ctui_rgb c)
{
	int cube =
	    16 + 36 * cube_level(c.r) + 6 * cube_level(c.g) + cube_level(c.b);
	int grey =
	    ((c.r + c.g + c.b) / 3 - 8 + 5) / 10; /* nearest of 8 + 10 i */
	grey = grey < 0 ? 0 : grey > 23 ? 23 : grey;
	int g = 232 + grey;
	return distance(c, ctui_xterm_rgb(g)) <
		       distance(c, ctui_xterm_rgb(cube))
		   ? g
		   : cube;
}

static int nearest_of(ctui_rgb c, int count)
{
	int best = 0, best_d = distance(c, ANSI16[0]);
	for (int i = 1; i < count; i++) {
		int d = distance(c, ANSI16[i]);
		if (d < best_d) {
			best = i;
			best_d = d;
		}
	}
	return best;
}

int ctui_ansi16_nearest(ctui_rgb c)
{
	return nearest_of(c, 16);
}

int ctui_ansi8_nearest(ctui_rgb c)
{
	return nearest_of(c, 8);
}

int ctui_palette_nearest(ctui_rgb c, int depth)
{
	return depth >= 256  ? ctui_xterm_nearest(c)
	       : depth >= 16 ? ctui_ansi16_nearest(c)
			     : ctui_ansi8_nearest(c);
}

static unsigned char snap(unsigned char v, int levels)
{
	int step = levels - 1;
	return (unsigned char)(((v * step + 127) / 255) * 255 / step);
}

ctui_rgb ctui_rgb_posterize(ctui_rgb c, int levels)
{
	if (levels < 2 || levels > 255)
		return c;
	return (ctui_rgb){snap(c.r, levels), snap(c.g, levels),
			  snap(c.b, levels)};
}

int ctui_rgb_luma(ctui_rgb c)
{
	return (299 * c.r + 587 * c.g + 114 * c.b + 500) / 1000;
}

ctui_image_view ctui_image_view_default(void)
{
	return (ctui_image_view){.renderer = CTUI_IMAGE_AUTO,
				 .scaler = CTUI_IMAGE_BOX,
				 .fit = CTUI_IMAGE_FIT,
				 .zoom = 1,
				 .background = {0, 0, 0}};
}

ctui_image_renderer ctui_image_renderer_for(ctui_image_renderer want,
					    bool unicode, int colors)
{
	if (!unicode)
		return CTUI_IMAGE_ASCII;
	if (want == CTUI_IMAGE_HALF || want == CTUI_IMAGE_BRAILLE ||
	    want == CTUI_IMAGE_ASCII)
		return want; /* PIXELS is AUTO here: the cells of a terminal
				without graphics */
	return colors >= 8 ? CTUI_IMAGE_HALF : CTUI_IMAGE_BRAILLE;
}

void ctui_image_grid(ctui_image_renderer r, int *per_col, int *per_row)
{
	switch (r) {
	case CTUI_IMAGE_BRAILLE:
		*per_col = 2;
		*per_row = 4;
		break;
	case CTUI_IMAGE_ASCII:
		*per_col = 1;
		*per_row = 1;
		break;
	default:
		*per_col = 1;
		*per_row = 2;
	}
}

static int clamp_round(double v, int lo, int hi)
{
	if (!(v > lo))
		return lo;
	if (v > hi)
		return hi;
	return (int)lround(v);
}

/* The layout of a w x h image in a region of cols x rows cells whose pixel
 * grid is pc x pr per cell and whose cells are `ratio` times taller than
 * wide. */
static bool layout(int w, int h, int cols, int rows,
		   const ctui_image_view *view, int pc, int pr, double ratio,
		   ctui_image_layout *out)
{
	*out = (ctui_image_layout){0};
	if (w < 1 || h < 1 || cols < 1 || rows < 1)
		return false;
	if (cols > CTUI_IMAGE_MAX_CELLS)
		cols = CTUI_IMAGE_MAX_CELLS;
	if (rows > CTUI_IMAGE_MAX_CELLS)
		rows = CTUI_IMAGE_MAX_CELLS;

	double zoom = view->zoom;
	if (!(zoom > 1))
		zoom = 1;
	if (zoom > CTUI_IMAGE_ZOOM_MAX)
		zoom = CTUI_IMAGE_ZOOM_MAX;
	double sw = w / zoom, sh = h / zoom;
	double px = isfinite(view->pan_x) ? view->pan_x : 0,
	       py = isfinite(view->pan_y) ? view->pan_y : 0;
	double sx = (w - sw) / 2 + px, sy = (h - sh) / 2 + py;
	sx = sx < 0 ? 0 : sx > w - sw ? w - sw : sx;
	sy = sy < 0 ? 0 : sy > h - sh ? h - sh : sy;

	int dc = cols, dr = rows;
	if (view->fit == CTUI_IMAGE_FIT) {
		/* Unit: one cell width; a cell is `ratio` units tall. */
		double scale = fmin(cols / sw, ratio * rows / sh);
		dc = clamp_round(sw * scale, 1, cols);
		dr = clamp_round(sh * scale / ratio, 1, rows);
	}
	out->cols = dc;
	out->rows = dr;
	out->col = (cols - dc) / 2;
	out->row = (rows - dr) / 2;
	out->px_w = dc * pc;
	out->px_h = dr * pr;
	out->sx = sx;
	out->sy = sy;
	out->sw = sw;
	out->sh = sh;
	return true;
}

bool ctui_image_layout_for(int w, int h, int cols, int rows,
			   const ctui_image_view *view, ctui_image_renderer r,
			   ctui_image_layout *out)
{
	int pc, pr;
	ctui_image_grid(r, &pc, &pr);
	return layout(w, h, cols, rows, view, pc, pr, 2.0, out);
}

bool ctui_image_layout_pixels(int w, int h, int cols, int rows, int cell_w,
			      int cell_h, const ctui_image_view *view,
			      ctui_image_layout *out)
{
	if (cell_w < 1 || cell_h < 1) {
		*out = (ctui_image_layout){0};
		return false;
	}
	/* the pixel grid stays within CTUI_IMAGE_PIXELS_MAX */
	if (cols > CTUI_IMAGE_PIXELS_MAX / cell_w)
		cols = CTUI_IMAGE_PIXELS_MAX / cell_w;
	if (rows > CTUI_IMAGE_PIXELS_MAX / cell_h)
		rows = CTUI_IMAGE_PIXELS_MAX / cell_h;
	return layout(w, h, cols, rows, view, cell_w, cell_h,
		      (double)cell_h / cell_w, out);
}

#define MAX_TAPS 64 /* source pixels averaged per axis and target pixel */

static int floor_clamp(double v, int n)
{
	if (!(v > 0))
		return 0;
	if (v >= n)
		return n - 1;
	return (int)v;
}

bool ctui_image_resample(const ctui_image *img, const ctui_image_layout *lay,
			 ctui_image_scaler scaler, ctui_rgb bg, ctui_rgb *out)
{
	if (!ctui_image_valid(img) || lay->px_w < 1 || lay->px_h < 1 ||
	    !(lay->sw > 0) || !(lay->sh > 0))
		return false;
	double stepx = lay->sw / lay->px_w, stepy = lay->sh / lay->px_h;
	for (int j = 0; j < lay->px_h; j++) {
		double y0 = lay->sy + j * stepy, y1 = y0 + stepy;
		for (int i = 0; i < lay->px_w; i++) {
			double x0 = lay->sx + i * stepx, x1 = x0 + stepx;
			ctui_rgb *dst =
			    &out[(size_t)j * (size_t)lay->px_w + (size_t)i];
			if (scaler == CTUI_IMAGE_NEAREST) {
				*dst = ctui_image_pixel(
				    img, floor_clamp((x0 + x1) / 2, img->width),
				    floor_clamp((y0 + y1) / 2, img->height),
				    bg);
				continue;
			}
			int ax = floor_clamp(x0, img->width),
			    bx = floor_clamp(x1 - 1e-9, img->width);
			int ay = floor_clamp(y0, img->height),
			    by = floor_clamp(y1 - 1e-9, img->height);
			int skipx = (bx - ax) / MAX_TAPS + 1,
			    skipy = (by - ay) / MAX_TAPS + 1;
			double sr = 0, sg = 0, sb = 0, sum = 0;
			for (int y = ay; y <= by; y += skipy) {
				double wy =
				    fmin(y1, y + 1.0) - fmax(y0, (double)y);
				if (wy <= 0)
					wy = 1e-6;
				for (int x = ax; x <= bx; x += skipx) {
					double wx = fmin(x1, x + 1.0) -
						    fmax(x0, (double)x);
					if (wx <= 0)
						wx = 1e-6;
					ctui_rgb p =
					    ctui_image_pixel(img, x, y, bg);
					double wgt = wx * wy;
					sr += p.r * wgt;
					sg += p.g * wgt;
					sb += p.b * wgt;
					sum += wgt;
				}
			}
			*dst = (ctui_rgb){(unsigned char)lround(sr / sum),
					  (unsigned char)lround(sg / sum),
					  (unsigned char)lround(sb / sum)};
		}
	}
	return true;
}

/* ---- cells ---- */

static void set_glyph(ctui_image_cell *c, const char *g)
{
	strncpy(c->glyph, g, sizeof c->glyph - 1);
	c->glyph[sizeof c->glyph - 1] = 0;
}

static void cell_half(const ctui_rgb *top, const ctui_rgb *bottom,
		      ctui_image_cell *c)
{
	set_glyph(c, "\xe2\x96\x80");
	c->fg = *top;
	c->bg = *bottom;
	c->has_bg = true;
}

static void cell_braille(const ctui_rgb *px, int stride, ctui_image_cell *c)
{
	static const unsigned char BITS[4][2] = {
	    {0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
	int luma[4][2], sum = 0, lo = 255, hi = 0;
	for (int dy = 0; dy < 4; dy++)
		for (int dx = 0; dx < 2; dx++) {
			int l = luma[dy][dx] =
			    ctui_rgb_luma(px[dy * stride + dx]);
			sum += l;
			lo = l < lo ? l : lo;
			hi = l > hi ? l : hi;
		}
	bool flat = hi - lo < 24;
	unsigned dots = 0, n = 0, r = 0, g = 0, b = 0;
	for (int dy = 0; dy < 4; dy++)
		for (int dx = 0; dx < 2; dx++)
			if (flat || luma[dy][dx] * 8 > sum) {
				ctui_rgb p = px[dy * stride + dx];
				dots |= BITS[dy][dx];
				r += p.r;
				g += p.g;
				b += p.b;
				n++;
			}
	c->glyph[0] = (char)0xE2; /* U+2800 + dots */
	c->glyph[1] = (char)(0xA0U | (dots >> 6U));
	c->glyph[2] = (char)(0x80U | (dots & 0x3FU));
	c->glyph[3] = 0;
	c->glyph[4] = 0;
	c->fg = (ctui_rgb){(unsigned char)((r + n / 2) / n),
			   (unsigned char)((g + n / 2) / n),
			   (unsigned char)((b + n / 2) / n)};
	c->bg = (ctui_rgb){0, 0, 0};
	c->has_bg = false;
}

static const char RAMP[] = " .:-=+*#%@";

static void cell_ascii(ctui_rgb p, ctui_image_cell *c)
{
	c->glyph[0] = RAMP[ctui_rgb_luma(p) * (int)(sizeof RAMP - 2) / 255];
	c->glyph[1] = 0;
	c->fg = p;
	c->bg = (ctui_rgb){0, 0, 0};
	c->has_bg = false;
}

void ctui_image_cells(const ctui_rgb *px, int px_w, int px_h,
		      ctui_image_renderer r, ctui_image_cell *out)
{
	int pc, pr;
	ctui_image_grid(r, &pc, &pr);
	int cols = px_w / pc, rows = px_h / pr;
	for (int j = 0; j < rows; j++)
		for (int i = 0; i < cols; i++) {
			const ctui_rgb *p = px +
					    (size_t)(j * pr) * (size_t)px_w +
					    (size_t)(i * pc);
			ctui_image_cell *c =
			    &out[(size_t)j * (size_t)cols + (size_t)i];
			if (r == CTUI_IMAGE_BRAILLE)
				cell_braille(p, px_w, c);
			else if (r == CTUI_IMAGE_ASCII)
				cell_ascii(*p, c);
			else
				cell_half(&p[0], &p[px_w], c);
		}
}

void ctui_image_cell_mono(ctui_image_cell *c, ctui_image_renderer r,
			  bool unicode)
{
	if (r != CTUI_IMAGE_HALF && r != CTUI_IMAGE_AUTO)
		return;
	bool top = ctui_rgb_luma(c->fg) >= 128,
	     bottom = ctui_rgb_luma(c->bg) >= 128;
	if (unicode)
		set_glyph(c, top && bottom ? "\xe2\x96\x88"
			     : top	   ? "\xe2\x96\x80"
			     : bottom	   ? "\xe2\x96\x84"
					   : " ");
	else
		set_glyph(c, top && bottom ? "#"
			     : top	   ? "\""
			     : bottom	   ? ","
					   : " ");
}

/* ---- colour budget ---- */

static unsigned key_of(const ctui_image_cell *c, int depth, int levels)
{
	unsigned fg = (unsigned)ctui_palette_nearest(
	    ctui_rgb_posterize(c->fg, levels), depth);
	unsigned bg = c->has_bg ? (unsigned)ctui_palette_nearest(
				      ctui_rgb_posterize(c->bg, levels), depth)
				: 0xFFFFu;
	return fg << 16U | bg;
}

static int compare_keys(const void *a, const void *b)
{
	unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
	return x < y ? -1 : x > y;
}

/* Sorted distinct keys of the cells (malloc()ed; *m of them). */
static unsigned *unique_keys(const ctui_image_cell *cells, size_t n, int depth,
			     int levels, size_t *m)
{
	unsigned *keys = malloc((n ? n : 1) * sizeof *keys);
	if (!keys)
		return NULL;
	for (size_t i = 0; i < n; i++)
		keys[i] = key_of(&cells[i], depth, levels);
	qsort(keys, n, sizeof *keys, compare_keys);
	size_t k = 0;
	for (size_t i = 0; i < n; i++)
		if (i == 0 || keys[i] != keys[i - 1])
			keys[k++] = keys[i];
	*m = k;
	return keys;
}

int ctui_image_degrade(ctui_image_cell *cells, size_t n, int depth,
		       size_t max_pairs)
{
	static const int LEVELS[] = {0, 8, 5, 3, 2};
	for (size_t l = 0; l < sizeof LEVELS / sizeof LEVELS[0]; l++) {
		size_t m = 0;
		unsigned *keys = unique_keys(cells, n, depth, LEVELS[l], &m);
		if (!keys)
			return -1;
		free(keys);
		if (m <= max_pairs) {
			if (LEVELS[l])
				for (size_t i = 0; i < n; i++) {
					cells[i].fg = ctui_rgb_posterize(
					    cells[i].fg, LEVELS[l]);
					cells[i].bg = ctui_rgb_posterize(
					    cells[i].bg, LEVELS[l]);
				}
			return LEVELS[l];
		}
	}
	return -1;
}

/* ---- drawing ---- */

#define POOL_PAIRS 2048
#define EMPTY 0xFFFFFFFFu

static struct {
	unsigned generation;
	bool ready;
	int first, count, used;
	unsigned *keys; /* open addressing, 2 * count slots */
	short *pairs;
	size_t slots;
} g_pool;

static void pool_free(void)
{
	free(g_pool.keys);
	free(g_pool.pairs);
	g_pool.keys = NULL;
	g_pool.pairs = NULL;
	g_pool.count = 0;
}

static void pool_reset(void)
{
	g_pool.used = 0;
	for (size_t i = 0; i < g_pool.slots; i++)
		g_pool.keys[i] = EMPTY;
}

/* True when the pool is usable for this theme generation. */
static bool pool_ready(void)
{
	if (g_pool.ready && g_pool.generation == ctui_theme_generation())
		return g_pool.count > 0;
	pool_free();
	g_pool.ready = true;
	g_pool.generation = ctui_theme_generation();
	g_pool.first = ctui_theme_pairs_reserve(POOL_PAIRS, &g_pool.count);
	if (g_pool.count <= 0) {
		g_pool.count = 0;
		return false;
	}
	g_pool.slots = (size_t)g_pool.count * 2;
	g_pool.keys = malloc(g_pool.slots * sizeof *g_pool.keys);
	g_pool.pairs = malloc(g_pool.slots * sizeof *g_pool.pairs);
	if (!g_pool.keys || !g_pool.pairs) {
		pool_free();
		return false;
	}
	pool_reset();
	return true;
}

static size_t slot_of(unsigned key)
{
	size_t s = (size_t)(key * 2654435761U) % g_pool.slots;
	while (g_pool.keys[s] != EMPTY && g_pool.keys[s] != key)
		s = (s + 1) % g_pool.slots;
	return s;
}

static int pool_pair(unsigned key)
{
	size_t s = slot_of(key);
	return g_pool.keys[s] == key ? g_pool.pairs[s] : -1;
}

static void pool_add(unsigned key)
{
	size_t s = slot_of(key);
	if (g_pool.keys[s] == key)
		return;
	short id = (short)(g_pool.first + g_pool.used++);
	g_pool.keys[s] = key;
	g_pool.pairs[s] = id;
	unsigned fg = key >> 16U, bg = key & 0xFFFFu;
	init_pair(id, (short)fg, bg == 0xFFFFu ? (short)-1 : (short)bg);
}

/* Give the cells' colours pairs; false when it cannot be done. */
static bool assign_pairs(ctui_image_cell *cells, size_t n, int depth)
{
	if (!pool_ready() ||
	    ctui_image_degrade(cells, n, depth, (size_t)g_pool.count) < 0)
		return false;
	size_t m = 0;
	unsigned *keys = unique_keys(cells, n, depth, 0, &m);
	if (!keys)
		return false;
	size_t fresh = 0;
	for (size_t i = 0; i < m; i++)
		if (pool_pair(keys[i]) < 0)
			fresh++;
	if ((size_t)g_pool.used + fresh > (size_t)g_pool.count)
		pool_reset();
	for (size_t i = 0; i < m; i++)
		pool_add(keys[i]);
	free(keys);
	return true;
}

ctui_image_renderer ctui_image_draw(const ctui_image *img,
				    const ctui_image_view *view, int y, int x,
				    int cols, int rows)
{
	if (!ctui_image_valid(img) || cols < 1 || rows < 1)
		return CTUI_IMAGE_AUTO;
	bool unicode = ctui_theme_unicode();
	int depth = !ctui_theme_has_colors() ? 0
		    : COLORS >= 256	     ? 256
		    : COLORS >= 16	     ? 16
		    : COLORS >= 8	     ? 8
					     : 0;
	ctui_image_view def = ctui_image_view_default();
	if (!view)
		view = &def;
	if (view->renderer == CTUI_IMAGE_PIXELS &&
	    ctui_image_pixels_draw(img, view, y, x, cols, rows))
		return CTUI_IMAGE_PIXELS;
	ctui_image_renderer r =
	    ctui_image_renderer_for(view->renderer, unicode, depth);

	ctui_image_layout lay;
	if (!ctui_image_layout_for(img->width, img->height, cols, rows, view, r,
				   &lay))
		return CTUI_IMAGE_AUTO;
	size_t npx = (size_t)lay.px_w * (size_t)lay.px_h,
	       ncells = (size_t)lay.cols * (size_t)lay.rows;
	ctui_rgb *px = malloc(npx * sizeof *px);
	ctui_image_cell *cells = malloc(ncells * sizeof *cells);
	if (!px || !cells ||
	    !ctui_image_resample(img, &lay, view->scaler, view->background,
				 px)) {
		free(px);
		free(cells);
		return CTUI_IMAGE_AUTO;
	}
	ctui_image_cells(px, lay.px_w, lay.px_h, r, cells);
	free(px);

	bool colour = depth > 0 && assign_pairs(cells, ncells, depth);
	int plain = ctui_attr(CTUI_PAIR_BODY, false);
	for (int j = 0; j < lay.rows; j++)
		for (int i = 0; i < lay.cols; i++) {
			ctui_image_cell *c =
			    &cells[(size_t)j * (size_t)lay.cols + (size_t)i];
			int attr = plain;
			if (colour)
				attr = (int)COLOR_PAIR(
				    pool_pair(key_of(c, depth, 0)));
			else
				ctui_image_cell_mono(c, r, unicode);
			ctui_put(y + lay.row + j, x + lay.col + i, 1, c->glyph,
				 attr);
		}
	free(cells);
	return r;
}

/* ---- PPM / PAM reader ---- */

typedef struct pnm_reader {
	const unsigned char *data;
	size_t len, pos;
} pnm_reader;

static bool is_space(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
	       c == '\f';
}

/* Skip white space and (P6 only) comments. */
static void skip_blank(pnm_reader *r)
{
	while (r->pos < r->len) {
		if (is_space(r->data[r->pos]))
			r->pos++;
		else if (r->data[r->pos] == '#')
			while (r->pos < r->len && r->data[r->pos] != '\n')
				r->pos++;
		else
			break;
	}
}

/* A decimal number of at most 9 digits followed by a delimiter. */
static bool read_number(pnm_reader *r, long *value)
{
	skip_blank(r);
	size_t digits = 0;
	long v = 0;
	while (r->pos < r->len && r->data[r->pos] >= '0' &&
	       r->data[r->pos] <= '9') {
		if (++digits > 9)
			return false;
		v = v * 10 + (r->data[r->pos++] - '0');
	}
	if (digits == 0 || (r->pos < r->len && !is_space(r->data[r->pos]) &&
			    r->data[r->pos] != '#'))
		return false;
	*value = v;
	return true;
}

/* One header line of a PAM file, without its end of line and comments. */
static bool pam_line(pnm_reader *r, char *line, size_t cap)
{
	size_t n = 0;
	while (r->pos < r->len && r->data[r->pos] != '\n') {
		if (r->data[r->pos] == '#') {
			while (r->pos < r->len && r->data[r->pos] != '\n')
				r->pos++;
			break;
		}
		if (n + 1 >= cap)
			return false;
		line[n++] = (char)r->data[r->pos++];
	}
	if (r->pos >= r->len)
		return false; /* the header must end with ENDHDR and a newline
			       */
	r->pos++;
	while (n > 0 && is_space((unsigned char)line[n - 1]))
		n--;
	line[n] = 0;
	return true;
}

static bool pam_header(pnm_reader *r, long *w, long *h, long *depth,
		       long *maxval, bool *alpha)
{
	char line[128];
	char tupl[64] = "";
	*w = *h = *depth = *maxval = 0;
	for (int lines = 0; lines < 64; lines++) {
		if (!pam_line(r, line, sizeof line))
			return false;
		char *p = line;
		while (*p && is_space((unsigned char)*p))
			p++;
		if (!*p)
			continue;
		if (strcmp(p, "ENDHDR") == 0) {
			if (!*w || !*h || !*depth || !*maxval)
				return false;
			if (tupl[0] == 0)
				*alpha = *depth == 4;
			else if (strcmp(tupl, "RGB") == 0 && *depth == 3)
				*alpha = false;
			else if (strcmp(tupl, "RGB_ALPHA") == 0 && *depth == 4)
				*alpha = true;
			else
				return false;
			return *depth == 3 || *depth == 4;
		}
		char *arg = strchr(p, ' ');
		if (!arg)
			return false;
		*arg++ = 0;
		while (*arg == ' ')
			arg++;
		if (strcmp(p, "TUPLTYPE") == 0) {
			if (tupl[0] || strlen(arg) >= sizeof tupl)
				return false;
			memcpy(tupl, arg, strlen(arg) + 1);
			continue;
		}
		long *field = strcmp(p, "WIDTH") == 0	 ? w
			      : strcmp(p, "HEIGHT") == 0 ? h
			      : strcmp(p, "DEPTH") == 0	 ? depth
			      : strcmp(p, "MAXVAL") == 0 ? maxval
							 : NULL;
		if (!field || *field)
			return false;
		pnm_reader num = {(const unsigned char *)arg, strlen(arg), 0};
		long v;
		if (!read_number(&num, &v) || num.pos != num.len || v == 0)
			return false;
		*field = v;
	}
	return false;
}

unsigned char *ctui_image_parse_pnm(const unsigned char *data, size_t len,
				    ctui_image *img)
{
	*img = (ctui_image){0};
	if (!data || len < 3 || data[0] != 'P' ||
	    (data[1] != '6' && data[1] != '7'))
		return NULL;
	pnm_reader r = {data, len, 2};
	long w, h, maxval, depth = 3;
	bool alpha = false;
	if (data[1] == '6') {
		if (!read_number(&r, &w) || !read_number(&r, &h) ||
		    !read_number(&r, &maxval))
			return NULL;
		if (r.pos >= len ||
		    !is_space(data[r.pos])) /* exactly one white space byte */
			return NULL;
		r.pos++;
	} else {
		if (!is_space(data[2]) ||
		    !pam_header(&r, &w, &h, &depth, &maxval, &alpha))
			return NULL;
		if (data[2] != '\n')
			return NULL;
	}
	if (w < 1 || h < 1 || maxval < 1 || maxval > 255 ||
	    w > CTUI_IMAGE_LOAD_MAX_DIM || h > CTUI_IMAGE_LOAD_MAX_DIM ||
	    (unsigned long)w * (unsigned long)h > CTUI_IMAGE_LOAD_MAX_PIXELS)
		return NULL;
	size_t total = (size_t)w * (size_t)h * (size_t)depth;
	if (len - r.pos < total)
		return NULL;
	unsigned char *pixels = malloc(total);
	if (!pixels)
		return NULL;
	const unsigned char *src = data + r.pos;
	if (maxval == 255) {
		memcpy(pixels, src, total);
	} else {
		for (size_t i = 0; i < total; i++) {
			if (src[i] >
			    maxval) { /* a sample above MAXVAL is malformed */
				free(pixels);
				return NULL;
			}
			pixels[i] =
			    (unsigned char)(((long)src[i] * 255 + maxval / 2) /
					    maxval);
		}
	}
	*img = (ctui_image){.pixels = pixels,
			    .width = (int)w,
			    .height = (int)h,
			    .channels = (int)depth,
			    .stride = (size_t)w * (size_t)depth};
	return pixels;
}

unsigned char *ctui_image_load_pnm(const char *path, ctui_image *img)
{
	*img = (ctui_image){0};
	FILE *f = path ? fopen(path, "rb") : NULL;
	if (!f)
		return NULL;
	const size_t cap = (size_t)CTUI_IMAGE_LOAD_MAX_PIXELS * 4 + 4096;
	unsigned char *buf = NULL;
	size_t len = 0, room = 0;
	for (;;) {
		if (len == room) {
			size_t bigger = room ? room * 2 : 65536;
			if (bigger > cap + 1)
				bigger = cap + 1;
			if (len >= cap + 1)
				break;
			unsigned char *grown = realloc(buf, bigger);
			if (!grown) {
				free(buf);
				fclose(f);
				return NULL;
			}
			buf = grown;
			room = bigger;
		}
		size_t want = room - len;
		size_t got = fread(buf + len, 1, want, f);
		len += got;
		if (got < want)
			break;
	}
	bool failed = ferror(f) || len > cap;
	fclose(f);
	unsigned char *pixels =
	    failed ? NULL : ctui_image_parse_pnm(buf, len, img);
	free(buf);
	return pixels;
}
