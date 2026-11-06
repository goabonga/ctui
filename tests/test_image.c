/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_image.c - palette mapping, scaling, layout and alpha of the image
 * component, and the PPM / PAM reader.
 */
#include "check.h"
#include "screen.h"
#include <ctui/image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool same(ctui_rgb a, ctui_rgb b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

static ctui_image rgb_image(const unsigned char *px, int w, int h)
{
	return (ctui_image){.pixels = px,
			    .width = w,
			    .height = h,
			    .channels = 3,
			    .stride = (size_t)w * 3};
}

static void test_palette(void)
{
	CHECK(ctui_xterm_nearest((ctui_rgb){0, 0, 0}) == 16);
	CHECK(ctui_xterm_nearest((ctui_rgb){255, 255, 255}) == 231);
	CHECK(ctui_xterm_nearest((ctui_rgb){255, 0, 0}) == 196);
	CHECK(ctui_xterm_nearest((ctui_rgb){0, 255, 0}) == 46);
	CHECK(ctui_xterm_nearest((ctui_rgb){0, 0, 255}) == 21);
	CHECK(ctui_xterm_nearest((ctui_rgb){95, 135, 175}) == 67);
	CHECK(ctui_xterm_nearest((ctui_rgb){128, 128, 128}) ==
	      244); /* grey 128 beats the cube */
	CHECK(ctui_xterm_nearest((ctui_rgb){8, 8, 8}) == 232);
	CHECK(ctui_xterm_nearest((ctui_rgb){238, 238, 238}) == 255);
	CHECK(ctui_xterm_nearest((ctui_rgb){90, 140, 170}) ==
	      67); /* snaps to the cube */
	CHECK(same(ctui_xterm_rgb(196), (ctui_rgb){255, 0, 0}));
	CHECK(same(ctui_xterm_rgb(244), (ctui_rgb){128, 128, 128}));
	CHECK(same(ctui_xterm_rgb(-5), ctui_xterm_rgb(0)) &&
	      same(ctui_xterm_rgb(999), ctui_xterm_rgb(255)));
	for (int i = 16; i < 256; i++) /* every palette colour maps to itself */
		CHECK(
		    same(ctui_xterm_rgb(ctui_xterm_nearest(ctui_xterm_rgb(i))),
			 ctui_xterm_rgb(i)));

	CHECK(ctui_ansi8_nearest((ctui_rgb){250, 10, 10}) == 1);
	CHECK(ctui_ansi8_nearest((ctui_rgb){0, 0, 0}) == 0);
	CHECK(ctui_ansi8_nearest((ctui_rgb){255, 255, 255}) == 7);
	CHECK(ctui_ansi16_nearest((ctui_rgb){255, 255, 255}) == 15);
	CHECK(ctui_ansi16_nearest((ctui_rgb){255, 0, 0}) == 9);
	CHECK(ctui_palette_nearest((ctui_rgb){255, 0, 0}, 256) == 196);
	CHECK(ctui_palette_nearest((ctui_rgb){255, 0, 0}, 16) == 9);
	CHECK(ctui_palette_nearest((ctui_rgb){255, 0, 0}, 8) == 1);
	CHECK(ctui_palette_nearest((ctui_rgb){255, 0, 0}, 0) == 1);

	CHECK(same(ctui_rgb_posterize((ctui_rgb){200, 100, 3}, 2),
		   (ctui_rgb){255, 0, 0}));
	CHECK(same(ctui_rgb_posterize((ctui_rgb){200, 128, 3}, 3),
		   (ctui_rgb){255, 127, 0}));
	CHECK(same(ctui_rgb_posterize((ctui_rgb){12, 34, 56}, 256),
		   (ctui_rgb){12, 34, 56}));
	CHECK(same(ctui_rgb_posterize((ctui_rgb){12, 34, 56}, 1),
		   (ctui_rgb){12, 34, 56}));
	CHECK(ctui_rgb_luma((ctui_rgb){0, 0, 0}) == 0 &&
	      ctui_rgb_luma((ctui_rgb){255, 255, 255}) == 255);
	CHECK(ctui_rgb_luma((ctui_rgb){0, 255, 0}) >
	      ctui_rgb_luma((ctui_rgb){255, 0, 0}));
}

static void test_valid_and_alpha(void)
{
	unsigned char px[8] = {10, 20, 30, 128, 255, 255, 255, 0};
	ctui_image img = {
	    .pixels = px, .width = 2, .height = 1, .channels = 4, .stride = 8};
	CHECK(ctui_image_valid(&img));
	ctui_rgb black = {0, 0, 0}, white = {255, 255, 255};
	ctui_rgb a = ctui_image_pixel(&img, 0, 0, black);
	CHECK(a.r == 5 && a.g == 10 && a.b == 15); /* 128/255 of the pixel */
	CHECK(same(ctui_image_pixel(&img, 1, 0, white),
		   white)); /* fully transparent */
	CHECK(same(ctui_image_pixel(&img, 1, 0, black), black));
	CHECK(same(ctui_image_pixel(&img, 2, 0, white), white)); /* outside */
	CHECK(same(ctui_image_pixel(&img, -1, 0, white), white));
	px[3] = 255;
	CHECK(
	    same(ctui_image_pixel(&img, 0, 0, white), (ctui_rgb){10, 20, 30}));

	ctui_image bad = img;
	bad.pixels = NULL;
	CHECK(!ctui_image_valid(&bad) && !ctui_image_valid(NULL));
	bad = img;
	bad.width = 0;
	CHECK(!ctui_image_valid(&bad));
	bad = img;
	bad.height = -3;
	CHECK(!ctui_image_valid(&bad));
	bad = img;
	bad.channels = 2;
	CHECK(!ctui_image_valid(&bad));
	bad = img;
	bad.stride = 7;
	CHECK(!ctui_image_valid(&bad));
	bad = img;
	bad.width = CTUI_IMAGE_MAX_DIM + 1;
	bad.stride = (size_t)-1;
	CHECK(!ctui_image_valid(&bad));
}

static void test_renderer_choice(void)
{
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_AUTO, true, 256) ==
	      CTUI_IMAGE_HALF);
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_AUTO, true, 8) ==
	      CTUI_IMAGE_HALF);
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_AUTO, true, 0) ==
	      CTUI_IMAGE_BRAILLE);
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_AUTO, false, 256) ==
	      CTUI_IMAGE_ASCII);
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_HALF, false, 256) ==
	      CTUI_IMAGE_ASCII);
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_BRAILLE, true, 256) ==
	      CTUI_IMAGE_BRAILLE);
	CHECK(ctui_image_renderer_for(CTUI_IMAGE_ASCII, true, 256) ==
	      CTUI_IMAGE_ASCII);
	int pc, pr;
	ctui_image_grid(CTUI_IMAGE_HALF, &pc, &pr);
	CHECK(pc == 1 && pr == 2);
	ctui_image_grid(CTUI_IMAGE_BRAILLE, &pc, &pr);
	CHECK(pc == 2 && pr == 4);
	ctui_image_grid(CTUI_IMAGE_ASCII, &pc, &pr);
	CHECK(pc == 1 && pr == 1);
}

static void test_layout_fit(void)
{
	ctui_image_view v = ctui_image_view_default();
	ctui_image_layout l;
	/* A square image in a wide region: height limits, cells are 1:2. */
	CHECK(ctui_image_layout_for(100, 100, 80, 20, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 40 && l.rows == 20 && l.col == 20 && l.row == 0);
	CHECK(l.px_w == 40 && l.px_h == 40);
	/* Same image in a tall region: width limits. */
	CHECK(ctui_image_layout_for(100, 100, 30, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 30 && l.rows == 15 && l.col == 0 && l.row == 12);
	/* Braille grid. */
	CHECK(ctui_image_layout_for(100, 100, 30, 40, &v, CTUI_IMAGE_BRAILLE,
				    &l));
	CHECK(l.px_w == 60 && l.px_h == 60);
	/* A wide image. */
	CHECK(ctui_image_layout_for(200, 100, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 40 && l.rows == 10);
	/* Stretch fills. */
	v.fit = CTUI_IMAGE_STRETCH;
	CHECK(
	    ctui_image_layout_for(200, 100, 40, 40, &v, CTUI_IMAGE_ASCII, &l));
	CHECK(l.cols == 40 && l.rows == 40 && l.col == 0 && l.row == 0);
	/* Odd sizes never leave the region. */
	v.fit = CTUI_IMAGE_FIT;
	CHECK(ctui_image_layout_for(7, 3, 5, 3, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols >= 1 && l.cols <= 5 && l.rows >= 1 && l.rows <= 3);
	CHECK(l.col + l.cols <= 5 && l.row + l.rows <= 3);
	/* One pixel: one cell. */
	CHECK(ctui_image_layout_for(1, 1, 10, 10, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 10 && l.rows == 5);
	CHECK(ctui_image_layout_for(1, 1, 1, 1, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 1 && l.rows == 1 && l.px_w == 1 && l.px_h == 2);
	/* A very thin image still gets a cell. */
	CHECK(ctui_image_layout_for(CTUI_IMAGE_MAX_DIM, 1, 10, 10, &v,
				    CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 10 && l.rows == 1);
	/* Empty. */
	CHECK(!ctui_image_layout_for(0, 5, 10, 10, &v, CTUI_IMAGE_HALF, &l));
	CHECK(!ctui_image_layout_for(5, 0, 10, 10, &v, CTUI_IMAGE_HALF, &l));
	CHECK(!ctui_image_layout_for(5, 5, 0, 10, &v, CTUI_IMAGE_HALF, &l));
	CHECK(!ctui_image_layout_for(5, 5, 10, -1, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.cols == 0 && l.px_w == 0);
	/* A huge region is clamped. */
	CHECK(ctui_image_layout_for(1000, 1000, 0x40000000, 0x40000000, &v,
				    CTUI_IMAGE_BRAILLE, &l));
	CHECK(l.cols <= CTUI_IMAGE_MAX_CELLS && l.rows <= CTUI_IMAGE_MAX_CELLS);
	CHECK(l.px_w == l.cols * 2 && l.px_h == l.rows * 4);
}

static void test_layout_zoom_pan(void)
{
	ctui_image_view v = ctui_image_view_default();
	ctui_image_layout l;
	v.zoom = 2;
	CHECK(ctui_image_layout_for(100, 80, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK_NEAR(l.sw, 50, 1e-9);
	CHECK_NEAR(l.sh, 40, 1e-9);
	CHECK_NEAR(l.sx, 25, 1e-9);
	CHECK_NEAR(l.sy, 20, 1e-9);
	v.pan_x = 1000; /* clamped to the right edge */
	v.pan_y = -1000;
	CHECK(ctui_image_layout_for(100, 80, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK_NEAR(l.sx, 50, 1e-9);
	CHECK_NEAR(l.sy, 0, 1e-9);
	v.pan_x = 10;
	v.pan_y = 0;
	CHECK(ctui_image_layout_for(100, 80, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK_NEAR(l.sx, 35, 1e-9);
	v.zoom = 1e9; /* capped */
	v.pan_x = v.pan_y = 0;
	CHECK(ctui_image_layout_for(100, 80, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK_NEAR(l.sw, 100 / CTUI_IMAGE_ZOOM_MAX, 1e-9);
	v.zoom = 0.2; /* below 1 means the whole image */
	v.pan_x = nan("");
	CHECK(ctui_image_layout_for(100, 80, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK_NEAR(l.sw, 100, 1e-9);
	CHECK_NEAR(l.sx, 0, 1e-9);
	v.zoom = nan("");
	CHECK(ctui_image_layout_for(100, 80, 40, 40, &v, CTUI_IMAGE_HALF, &l));
	CHECK_NEAR(l.sw, 100, 1e-9);
}

/* A 4x2 image: left half red, right half blue, with a green second row. */
static const unsigned char PATTERN[] = {255, 0, 0, 255, 0, 0,	0,   0,
					255, 0, 0, 255, 0, 255, 0,   0,
					255, 0, 0, 255, 0, 0,	255, 0};

static void test_resample(void)
{
	ctui_image img = rgb_image(PATTERN, 4, 2);
	ctui_image_view v = ctui_image_view_default();
	v.fit = CTUI_IMAGE_STRETCH;
	ctui_image_layout l;
	ctui_rgb out[16];
	ctui_rgb black = {0, 0, 0};

	/* Same size: identity with both scalers (4 columns, 1 row of 2 pixels).
	 */
	CHECK(ctui_image_layout_for(4, 2, 4, 1, &v, CTUI_IMAGE_HALF, &l));
	CHECK(l.px_w == 4 && l.px_h == 2);
	for (int s = 0; s < 2; s++) {
		CHECK(ctui_image_resample(
		    &img, &l, s ? CTUI_IMAGE_NEAREST : CTUI_IMAGE_BOX, black,
		    out));
		CHECK(same(out[0], (ctui_rgb){255, 0, 0}) &&
		      same(out[1], (ctui_rgb){255, 0, 0}));
		CHECK(same(out[2], (ctui_rgb){0, 0, 255}) &&
		      same(out[3], (ctui_rgb){0, 0, 255}));
		CHECK(same(out[4], (ctui_rgb){0, 255, 0}) &&
		      same(out[7], (ctui_rgb){0, 255, 0}));
	}

	/* Downsample 4x2 to 2x1: box averages, nearest picks. */
	CHECK(ctui_image_layout_for(4, 2, 2, 1, &v, CTUI_IMAGE_ASCII, &l));
	CHECK(l.px_w == 2 && l.px_h == 1);
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_BOX, black, out));
	CHECK(same(out[0],
		   (ctui_rgb){128, 128, 0})); /* (255,0,0)+(0,255,0) averaged */
	CHECK(same(out[1], (ctui_rgb){0, 128, 128}));
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_NEAREST, black, out));
	CHECK(out[0].r == 255 ||
	      out[0].g == 255); /* one of the four source pixels */

	/* Upsample 4x2 to 8x2 (cells 8x1): each pixel doubles. */
	CHECK(ctui_image_layout_for(4, 2, 8, 1, &v, CTUI_IMAGE_HALF, &l));
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_BOX, black, out));
	CHECK(same(out[0], (ctui_rgb){255, 0, 0}) &&
	      same(out[3], (ctui_rgb){255, 0, 0}));
	CHECK(same(out[4], (ctui_rgb){0, 0, 255}) &&
	      same(out[7], (ctui_rgb){0, 0, 255}));

	/* Zoomed on the right half. */
	v.zoom = 2;
	v.pan_x = 100;
	CHECK(ctui_image_layout_for(4, 2, 2, 1, &v, CTUI_IMAGE_HALF, &l));
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_BOX, black, out));
	CHECK(same(out[0], (ctui_rgb){0, 0, 255}));

	/* 1x1 image anywhere. */
	unsigned char one[3] = {1, 2, 3};
	ctui_image px = rgb_image(one, 1, 1);
	v = ctui_image_view_default();
	ctui_rgb big[8 * 8];
	CHECK(ctui_image_layout_for(1, 1, 4, 4, &v, CTUI_IMAGE_BRAILLE, &l));
	CHECK(ctui_image_resample(&px, &l, CTUI_IMAGE_BOX, black, big));
	CHECK(same(big[0], (ctui_rgb){1, 2, 3}) &&
	      same(big[l.px_w * l.px_h - 1], (ctui_rgb){1, 2, 3}));

	/* Invalid input. */
	ctui_image bad = img;
	bad.pixels = NULL;
	CHECK(!ctui_image_resample(&bad, &l, CTUI_IMAGE_BOX, black, out));
	ctui_image_layout empty = {0};
	CHECK(!ctui_image_resample(&img, &empty, CTUI_IMAGE_BOX, black, out));
}

static void test_resample_alpha(void)
{
	unsigned char px[8] = {
	    255, 0, 0, 0, 0, 0, 255, 255}; /* transparent red, opaque blue */
	ctui_image img = {
	    .pixels = px, .width = 2, .height = 1, .channels = 4, .stride = 8};
	ctui_image_view v = ctui_image_view_default();
	v.fit = CTUI_IMAGE_STRETCH;
	v.background = (ctui_rgb){0, 255, 0};
	ctui_image_layout l;
	ctui_rgb out[4];
	CHECK(ctui_image_layout_for(2, 1, 1, 1, &v, CTUI_IMAGE_ASCII, &l));
	CHECK(l.px_w == 1 && l.px_h == 1);
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_BOX, v.background, out));
	CHECK(same(out[0],
		   (ctui_rgb){0, 128, 128})); /* green and blue averaged */
	CHECK(ctui_image_layout_for(2, 1, 2, 1, &v, CTUI_IMAGE_ASCII, &l));
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_NEAREST, v.background,
				  out));
	CHECK(same(out[0], (ctui_rgb){0, 255, 0}) &&
	      same(out[1], (ctui_rgb){0, 0, 255}));
}

static void test_huge(void)
{
	/* A 65536 x 1 image (one row of the largest width) downsampled to a
	 * few columns: the cost stays bounded and nothing overflows. */
	size_t w = CTUI_IMAGE_MAX_DIM;
	unsigned char *row = malloc(w * 3);
	CHECK(row != NULL);
	if (!row)
		return;
	for (size_t i = 0; i < w; i++) {
		row[i * 3] = (unsigned char)(i * 255 / (w - 1));
		row[i * 3 + 1] = 0;
		row[i * 3 + 2] = 0;
	}
	ctui_image img = {.pixels = row,
			  .width = CTUI_IMAGE_MAX_DIM,
			  .height = 1,
			  .channels = 3,
			  .stride = w * 3};
	CHECK(ctui_image_valid(&img));
	ctui_image_view v = ctui_image_view_default();
	v.fit = CTUI_IMAGE_STRETCH;
	ctui_image_layout l;
	ctui_rgb out[8];
	CHECK(ctui_image_layout_for(img.width, img.height, 4, 1, &v,
				    CTUI_IMAGE_ASCII, &l));
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_BOX, (ctui_rgb){0, 0, 0},
				  out));
	CHECK(out[0].r < out[1].r && out[1].r < out[2].r &&
	      out[2].r < out[3].r);
	CHECK(out[3].r > 150 && out[0].r < 60);
	v.zoom = CTUI_IMAGE_ZOOM_MAX;
	v.pan_x = 1e300;
	CHECK(ctui_image_layout_for(img.width, img.height, 4, 1, &v,
				    CTUI_IMAGE_ASCII, &l));
	CHECK(l.sx + l.sw <= img.width + 1e-6);
	CHECK(ctui_image_resample(&img, &l, CTUI_IMAGE_BOX, (ctui_rgb){0, 0, 0},
				  out));
	CHECK(out[0].r > 240);
	free(row);

	/* A stride past what fits in an int must not wrap. */
	unsigned char tiny[3] = {9, 9, 9};
	ctui_image odd = {.pixels = tiny,
			  .width = 1,
			  .height = 1,
			  .channels = 3,
			  .stride = (size_t)1 << 40U};
	CHECK(ctui_image_valid(&odd));
	CHECK(same(ctui_image_pixel(&odd, 0, 0, (ctui_rgb){0, 0, 0}),
		   (ctui_rgb){9, 9, 9}));
}

/* Parse a NUL-terminated header followed by n data bytes. */
static unsigned char *parse(const char *text, const unsigned char *data,
			    size_t n, ctui_image *img)
{
	size_t h = strlen(text);
	unsigned char *buf = malloc(h + n + 1);
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(buf, text, h);
	if (n)
		memcpy(buf + h, data, n);
	unsigned char *px = ctui_image_parse_pnm(buf, h + n, img);
	free(buf);
	return px;
}

static void test_pnm(void)
{
	const unsigned char rgb[] = {1, 2, 3, 4, 5, 6};
	ctui_image img;
	unsigned char *px = parse("P6\n2 1\n255\n", rgb, 6, &img);
	CHECK(px && img.width == 2 && img.height == 1 && img.channels == 3 &&
	      img.stride == 6);
	CHECK(px && memcmp(px, rgb, 6) == 0 && ctui_image_valid(&img));
	free(px);

	px = parse("P6 # size follows\n# more\n2\t1 255 ", rgb, 6,
		   &img); /* comments, odd blanks */
	CHECK(px && img.width == 2);
	free(px);

	const unsigned char extra[] = {1, 2, 3, 4, 5, 6, 7, 8};
	px = parse("P6\n2 1\n255\n", extra, 8,
		   &img); /* trailing bytes ignored */
	CHECK(px && img.width == 2);
	free(px);

	const unsigned char half[] = {0, 1, 2};
	px = parse("P6\n1 1\n2\n", half, 3, &img); /* maxval 2 scales to 255 */
	CHECK(px && px[0] == 0 && px[1] == 128 && px[2] == 255);
	free(px);
	CHECK(!parse("P6\n1 1\n2\n", (const unsigned char[]){0, 1, 3}, 3,
		     &img)); /* above maxval */

	const unsigned char pam[] = {10, 20, 30, 40, 50, 60, 70, 80};
	px = parse("P7\nWIDTH 2\nHEIGHT 1\nDEPTH 4\nMAXVAL 255\nTUPLTYPE "
		   "RGB_ALPHA\nENDHDR\n",
		   pam, 8, &img);
	CHECK(px && img.channels == 4 && img.stride == 8 &&
	      memcmp(px, pam, 8) == 0);
	free(px);
	px = parse("P7\n# c\nWIDTH 2\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nTUPLTYPE "
		   "RGB\nENDHDR\n",
		   pam, 6, &img);
	CHECK(px && img.channels == 3);
	free(px);
	px = parse("P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nENDHDR\n", pam,
		   3, &img);
	CHECK(px && img.channels == 3);
	free(px);
}

static void test_pnm_malformed(void)
{
	const unsigned char d[64] = {0};
	ctui_image img;
	static const char *const BAD[] = {
	    "",
	    "P",
	    "P6",
	    "P5\n1 1\n255\n",
	    "P3\n1 1\n255\n",
	    "P6\n",
	    "P6\n1\n",
	    "P6\n1 1\n",
	    "P6\n0 1\n255\n",
	    "P6\n1 0\n255\n",
	    "P6\n-1 1\n255\n",
	    "P6\n1 1\n0\n",
	    "P6\n1 1\n256\n",
	    "P6\n1 1\n65535\n",
	    "P6\n1 1\n255", /* no blank */
	    "P6\n1x1\n255\n",
	    "P6\n1 1\n255x",
	    "P6\n99999999999 1\n255\n",
	    "P6\n16385 1\n255\n",
	    "P6\n1 16385\n255\n",
	    "P6\n16384 16384\n255\n",
	    "P6\n4097 4097\n255\n",
	    "P6\n1 1 # c 255\n",
	    "P6\n1 1\n255#\n",
	    "P7\n",
	    "P7\nENDHDR\n",
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nENDHDR",
	    "P7\nWIDTH 1\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nENDHDR\n",
	    // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 2\nMAXVAL 255\nTUPLTYPE "
	    "GRAYSCALE_ALPHA\nENDHDR\n",
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 1\nMAXVAL 255\nTUPLTYPE "
	    "GRAYSCALE\nENDHDR\n",
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nTUPLTYPE "
	    "RGB_ALPHA\nENDHDR\n",
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 65535\nTUPLTYPE "
	    "RGB\nENDHDR\n",
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nENDHDR\n",
	    "P7\nWIDTH x\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nENDHDR\n",
	    "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nBOGUS 3\nENDHDR\n",
	    "P7 WIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nENDHDR\n",
	    "P7\nWIDTH 100000\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nENDHDR\n",
	};
	for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
		unsigned char *px = parse(BAD[i], d, 64, &img);
		if (px) {
			fprintf(stderr, "accepted: %s\n", BAD[i]);
			free(px);
		}
		CHECK(px == NULL && img.pixels == NULL && img.width == 0);
	}
	/* Truncated data. */
	CHECK(!parse("P6\n2 2\n255\n", d, 11, &img));
	CHECK(!parse("P6\n2 2\n255\n", d, 0, &img));
	CHECK(!parse("P7\nWIDTH 1\nHEIGHT 1\nDEPTH 4\nMAXVAL 255\nTUPLTYPE "
		     "RGB_ALPHA\nENDHDR\n",
		     d, 3, &img));
	/* Huge but within the dimension caps: refused before allocating. */
	CHECK(!parse("P6\n16384 16384\n255\n", d, 3, &img));
	CHECK(!ctui_image_parse_pnm(NULL, 10, &img));
	/* A long run of comments and blanks does not loop forever or overrun.
	 */
	char *junk = malloc(100002);
	memset(junk, '#', 100000);
	memcpy(junk, "P6 ", 3);
	junk[100000] = 0;
	CHECK(!parse(junk, d, 0, &img));
	free(junk);
}

static void test_pnm_file(void)
{
	char path[] = "/tmp/ctui-image-XXXXXX";
	int fd = mkstemp(path);
	CHECK(fd >= 0);
	if (fd < 0)
		return;
	const char head[] = "P6\n2 1\n255\n";
	const unsigned char data[6] = {9, 8, 7, 6, 5, 4};
	CHECK(write(fd, head, sizeof head - 1) == (ssize_t)sizeof head - 1);
	CHECK(write(fd, data, 6) == 6);
	close(fd);
	ctui_image img;
	unsigned char *px = ctui_image_load_pnm(path, &img);
	CHECK(px && img.width == 2 && memcmp(px, data, 6) == 0);
	free(px);
	CHECK(!ctui_image_load_pnm("/nonexistent/ctui.ppm", &img) &&
	      img.pixels == NULL);
	CHECK(!ctui_image_load_pnm(NULL, &img));
	CHECK(!ctui_image_load_pnm("/tmp", &img)); /* a directory */
	unlink(path);
}

static void test_cells(void)
{
	ctui_rgb red = {255, 0, 0}, blue = {0, 0, 255}, white = {255, 255, 255},
		 black = {0, 0, 0};
	ctui_image_cell c;
	ctui_rgb half[2] = {red, blue};
	ctui_image_cells(half, 1, 2, CTUI_IMAGE_HALF, &c);
	CHECK_STR(c.glyph, "\xe2\x96\x80");
	CHECK(same(c.fg, red) && same(c.bg, blue) && c.has_bg);

	ctui_rgb flat[8];
	for (int i = 0; i < 8; i++)
		flat[i] = red;
	ctui_image_cells(flat, 2, 4, CTUI_IMAGE_BRAILLE, &c);
	CHECK_STR(c.glyph, "\xe2\xa3\xbf"); /* U+28FF: all dots */
	CHECK(same(c.fg, red) && !c.has_bg);

	ctui_rgb mixed[8] = {white, black, black, black,
			     black, black, black, black};
	ctui_image_cells(mixed, 2, 4, CTUI_IMAGE_BRAILLE, &c);
	CHECK_STR(c.glyph, "\xe2\xa0\x81"); /* U+2801: top left dot only */
	CHECK(same(c.fg, white));
	ctui_rgb right[8] = {black, white, black, white,
			     black, white, black, white};
	ctui_image_cells(right, 2, 4, CTUI_IMAGE_BRAILLE, &c);
	CHECK_STR(c.glyph, "\xe2\xa2\xb8"); /* U+28B8: right column */

	ctui_rgb pair[2] = {black, white};
	ctui_image_cell two[2];
	ctui_image_cells(pair, 2, 1, CTUI_IMAGE_ASCII, two);
	CHECK_STR(two[0].glyph, " ");
	CHECK_STR(two[1].glyph, "@");
	CHECK(same(two[1].fg, white));

	/* mono forms of a half block */
	ctui_rgb hh[2] = {white, black};
	ctui_image_cells(hh, 1, 2, CTUI_IMAGE_HALF, &c);
	ctui_image_cell_mono(&c, CTUI_IMAGE_HALF, true);
	CHECK_STR(c.glyph, "\xe2\x96\x80");
	ctui_image_cells(hh, 1, 2, CTUI_IMAGE_HALF, &c);
	ctui_image_cell_mono(&c, CTUI_IMAGE_HALF, false);
	CHECK_STR(c.glyph, "\"");
	ctui_rgb bb[2] = {black, white};
	ctui_image_cells(bb, 1, 2, CTUI_IMAGE_HALF, &c);
	ctui_image_cell_mono(&c, CTUI_IMAGE_HALF, true);
	CHECK_STR(c.glyph, "\xe2\x96\x84");
	ctui_rgb ww[2] = {white, white}, kk[2] = {black, black};
	ctui_image_cells(ww, 1, 2, CTUI_IMAGE_HALF, &c);
	ctui_image_cell_mono(&c, CTUI_IMAGE_HALF, true);
	CHECK_STR(c.glyph, "\xe2\x96\x88");
	ctui_image_cells(kk, 1, 2, CTUI_IMAGE_HALF, &c);
	ctui_image_cell_mono(&c, CTUI_IMAGE_HALF, true);
	CHECK_STR(c.glyph, " ");
	ctui_image_cells(mixed, 2, 4, CTUI_IMAGE_BRAILLE, &c);
	ctui_image_cell_mono(&c, CTUI_IMAGE_BRAILLE,
			     true); /* Braille is left as it is */
	CHECK_STR(c.glyph, "\xe2\xa0\x81");
}

static void test_degrade(void)
{
	enum { N = 400 };
	ctui_image_cell cells[N], copy[N];
	for (int i = 0; i < N; i++) {
		cells[i] = (ctui_image_cell){.glyph = "x", .has_bg = true};
		cells[i].fg =
		    (ctui_rgb){(unsigned char)(i * 37), (unsigned char)(i * 11),
			       (unsigned char)(i * 5)};
		cells[i].bg =
		    (ctui_rgb){(unsigned char)(i * 3), (unsigned char)(i * 29),
			       (unsigned char)(i * 53)};
	}
	memcpy(copy, cells, sizeof cells);
	CHECK(ctui_image_degrade(cells, N, 256, N) == 0); /* fits as is */
	CHECK(memcmp(cells, copy, sizeof cells) == 0);
	int level = ctui_image_degrade(cells, N, 256, 100);
	CHECK(level == 8 || level == 5 || level == 3 || level == 2);
	CHECK(memcmp(cells, copy, sizeof cells) != 0);
	memcpy(cells, copy, sizeof cells);
	CHECK(ctui_image_degrade(cells, N, 256, 3) == -1 ||
	      ctui_image_degrade(cells, N, 256, 3) == 2);
	memcpy(cells, copy, sizeof cells);
	CHECK(ctui_image_degrade(cells, N, 256, 1) == -1);
	CHECK(memcmp(cells, copy, sizeof cells) ==
	      0); /* untouched on failure */
	CHECK(ctui_image_degrade(cells, 0, 256, 0) ==
	      0); /* nothing to draw always fits */
	/* A smaller palette needs fewer pairs by itself. */
	CHECK(ctui_image_degrade(cells, N, 8, 64) >= 0);
}

static void test_draw(void)
{
	enum { W = 32, H = 16 };
	unsigned char *px = malloc((size_t)W * H * 3);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			unsigned char *p = px + ((size_t)y * W + (size_t)x) * 3;
			p[0] = (unsigned char)(x * 8);
			p[1] = (unsigned char)(y * 16);
			p[2] = 128;
		}
	ctui_image img = rgb_image(px, W, H);
	screen_open(24, 80);
	CHECK(ctui_image_draw(&img, NULL, 0, 0, 0, 5) == CTUI_IMAGE_AUTO);
	ctui_image bad = img;
	bad.width = 0;
	CHECK(ctui_image_draw(&bad, NULL, 0, 0, 10, 10) == CTUI_IMAGE_AUTO);

	ctui_image_view v = ctui_image_view_default();
	CHECK(ctui_image_draw(&img, &v, 1, 2, 20, 10) == CTUI_IMAGE_HALF);
	CHECK(screen_has_text("\xe2\x96\x80"));
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair = 0;
	CHECK(mvin_wch(1 + 3, 2 + 5, &cell) == OK &&
	      getcchar(&cell, wch, &attrs, &pair, NULL) == OK);
	CHECK(wch[0] == 0x2580 &&
	      pair >= CTUI_PAIR_COUNT); /* one of the reserved pairs */
	short f, b;
	CHECK(pair_content(pair, &f, &b) == OK && f >= 16 && b >= 16);

	screen_clear();
	v.renderer = CTUI_IMAGE_BRAILLE;
	CHECK(ctui_image_draw(&img, &v, 0, 0, 20, 10) == CTUI_IMAGE_BRAILLE);
	CHECK(screen_has_text("\xe2\xa0") || screen_has_text("\xe2\xa1") ||
	      screen_has_text("\xe2\xa3"));

	screen_clear();
	v.renderer = CTUI_IMAGE_ASCII;
	v.fit = CTUI_IMAGE_STRETCH;
	CHECK(ctui_image_draw(&img, &v, 0, 0, 20, 10) == CTUI_IMAGE_ASCII);
	CHECK(screen_has_text("%") || screen_has_text("#") ||
	      screen_has_text("+"));

	/* Not UTF-8: ASCII whatever the request. */
	screen_clear();
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	v.renderer = CTUI_IMAGE_HALF;
	CHECK(ctui_image_draw(&img, &v, 0, 0, 20, 10) == CTUI_IMAGE_ASCII);
	CHECK(!screen_has_text("\xe2\x96\x80"));
	unsetenv("CTUI_ASCII");

	/* No colours: Braille by default, plain attributes, no pair used. */
	screen_clear();
	setenv("NO_COLOR", "1", 1);
	ctui_theme_init();
	v.renderer = CTUI_IMAGE_AUTO;
	CHECK(ctui_image_draw(&img, &v, 0, 0, 20, 10) == CTUI_IMAGE_BRAILLE);
	v.renderer = CTUI_IMAGE_HALF;
	CHECK(ctui_image_draw(&img, &v, 0, 0, 20, 10) == CTUI_IMAGE_HALF);
	CHECK(mvin_wch(2, 2, &cell) == OK &&
	      getcchar(&cell, wch, &attrs, &pair, NULL) == OK);
	CHECK(pair < CTUI_PAIR_COUNT);
	unsetenv("NO_COLOR");
	ctui_theme_init();

	/* The same picture twice (pairs are cached), and a region larger than
	 * the screen. */
	v = ctui_image_view_default();
	CHECK(ctui_image_draw(&img, &v, 0, 0, 20, 10) == CTUI_IMAGE_HALF);
	CHECK(ctui_image_draw(&img, &v, 0, 0, 500, 500) == CTUI_IMAGE_HALF);
	CHECK(ctui_image_draw(&img, &v, -5, -5, 20, 10) ==
	      CTUI_IMAGE_HALF); /* partly off screen */
	free(px);
}

/* A photograph-like picture needs far more pairs than a terminal with few
 * of them offers: it must still draw. */
static void test_draw_many_colours(void)
{
	enum { W = 400, H = 400 };
	unsigned char *px = malloc((size_t)W * H * 3);
	unsigned seed = 1;
	for (int i = 0; i < W * H * 3; i++) {
		seed = seed * 1103515245U + 12345U;
		px[i] = (unsigned char)(seed >> 16U);
	}
	ctui_image img = rgb_image(px, W, H);
	ctui_image_view v = ctui_image_view_default();
	v.scaler = CTUI_IMAGE_NEAREST;
	CHECK(ctui_image_draw(&img, &v, 0, 0, 200, 100) == CTUI_IMAGE_HALF);
	CHECK(ctui_image_draw(&img, &v, 0, 0, 200, 100) == CTUI_IMAGE_HALF);
	free(px);
}

static void test_draw_eight_colours(void)
{
	FILE *out = fopen("/dev/null", "w");
	FILE *in = fopen("/dev/null", "r");
	setenv("TERM", "xterm", 1);
	// NOLINTNEXTLINE(clang-analyzer-unix.Stream)
	SCREEN *s = out && in ? newterm("xterm", out, in) : NULL;
	if (!s) {
		fprintf(stderr, "no xterm terminfo, skipping\n");
		return;
	}
	SCREEN *old = set_term(s);
	(void)old;
	ctui_theme_init();
	CHECK(COLORS == 8);
	unsigned char px[4 * 4 * 3];
	for (size_t i = 0; i < sizeof px; i++)
		px[i] = (unsigned char)(i * 53);
	ctui_image img = rgb_image(px, 4, 4);
	ctui_image_view v = ctui_image_view_default();
	CHECK(ctui_image_draw(&img, &v, 0, 0, 10, 5) == CTUI_IMAGE_HALF);
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair = 0, f = 0, b = 0;
	CHECK(mvin_wch(0, 0, &cell) == OK &&
	      getcchar(&cell, wch, &attrs, &pair, NULL) == OK);
	CHECK(pair_content(pair, &f, &b) == OK && f < 8 && b < 8);
	endwin();
	delscreen(s);
}

int main(void)
{
	RUN(test_palette);
	RUN(test_valid_and_alpha);
	RUN(test_renderer_choice);
	RUN(test_layout_fit);
	RUN(test_layout_zoom_pan);
	RUN(test_resample);
	RUN(test_resample_alpha);
	RUN(test_huge);
	RUN(test_pnm);
	RUN(test_pnm_malformed);
	RUN(test_pnm_file);
	RUN(test_cells);
	RUN(test_degrade);
	RUN(test_draw);
	RUN(test_draw_many_colours);
	RUN(test_draw_eight_colours);
	return CHECK_DONE();
}
