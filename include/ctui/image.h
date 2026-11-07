/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * image.h - display a caller-supplied pixel buffer in the terminal.
 *
 * The pixels are 8-bit RGB or RGBA rows (width, height, channels, stride in
 * bytes); no decoder is involved. This part holds the pure functions:
 * colour quantising, scaling and the layout in cells, none of which needs
 * curses.
 *
 * Colours map to the xterm-256 palette (indexes 16..231 are a 6x6x6 cube
 * whose levels are 0, 95, 135, 175, 215, 255; 232..255 are 24 greys 8 +
 * 10 * i). The distance between two colours is the squared Euclidean
 * distance in RGB; ties go to the lower index.
 */
#ifndef CTUI_IMAGE_H
#define CTUI_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define CTUI_IMAGE_MAX_DIM                                                     \
	65536 /* largest width or height of a source image                     \
	       */
#define CTUI_IMAGE_MAX_CELLS                                                   \
	512 /* the region drawn is clamped to this many cells each way */
#define CTUI_IMAGE_ZOOM_MAX 64.0

typedef struct ctui_rgb {
	unsigned char r, g, b;
} ctui_rgb;

typedef struct ctui_image {
	const unsigned char *pixels; /* rows of stride bytes */
	int width, height;	     /* in pixels, 1..CTUI_IMAGE_MAX_DIM */
	int channels;		     /* 3 = RGB, 4 = RGBA (straight alpha) */
	size_t stride; /* bytes between rows, >= width * channels */
} ctui_image;

/* True when the fields are consistent (the pixel pointer is not NULL, the
 * sizes are in range and the stride is large enough). The buffer itself
 * cannot be checked: it must hold stride * (height - 1) + width * channels
 * bytes. */
bool ctui_image_valid(const ctui_image *img);

/* The pixel (x, y) blended onto background (alpha 255 = the pixel). Outside
 * the image, the background. */
ctui_rgb ctui_image_pixel(const ctui_image *img, int x, int y,
			  ctui_rgb background);

/* Palettes. */
ctui_rgb ctui_xterm_rgb(int index);  /* colour of an index 0..255 */
int ctui_xterm_nearest(ctui_rgb c);  /* nearest of 16..255 */
int ctui_ansi16_nearest(ctui_rgb c); /* nearest of 0..15 */
int ctui_ansi8_nearest(ctui_rgb c);  /* nearest of 0..7 */
/* Nearest index in a palette of depth 256, 16 or 8 colours. */
int ctui_palette_nearest(ctui_rgb c, int depth);

/* Snap every channel to one of `levels` (2..255) evenly spaced values;
 * other values leave the colour as it is. */
ctui_rgb ctui_rgb_posterize(ctui_rgb c, int levels);

/* Brightness 0..255 (Rec. 601 weights). */
int ctui_rgb_luma(ctui_rgb c);

typedef enum ctui_image_renderer {
	CTUI_IMAGE_AUTO, /* half blocks, else Braille without colours, else
			    ASCII */
	CTUI_IMAGE_HALF, /* U+2580 with the pixel above as foreground, below as
			    background */
	CTUI_IMAGE_BRAILLE, /* 2 x 4 dots per cell, one colour per cell */
	CTUI_IMAGE_ASCII,   /* one character of a brightness ramp per cell */
	CTUI_IMAGE_PIXELS   /* the terminal's own graphics, see below; half
			       blocks where it has none */
} ctui_image_renderer;

typedef enum ctui_image_scaler {
	CTUI_IMAGE_BOX, /* average of the source pixels under each target pixel
			 */
	CTUI_IMAGE_NEAREST /* the source pixel at the centre */
} ctui_image_scaler;

typedef enum ctui_image_fit {
	CTUI_IMAGE_FIT,	   /* keep the aspect ratio, assuming cells are 1:2 */
	CTUI_IMAGE_STRETCH /* fill the region */
} ctui_image_fit;

typedef struct ctui_image_view {
	ctui_image_renderer renderer;
	ctui_image_scaler scaler;
	ctui_image_fit fit;
	double zoom;	     /* 1 = whole image, up to CTUI_IMAGE_ZOOM_MAX */
	double pan_x, pan_y; /* shift of the zoomed window from the centre, in
				source pixels */
	ctui_rgb background; /* what transparent pixels blend onto */
} ctui_image_view;

/* Auto renderer, box filter, fit, no zoom, black background. */
ctui_image_view ctui_image_view_default(void);

/* The renderer actually used: ASCII when the locale is not UTF-8,
 * otherwise the requested one; AUTO picks half blocks with at least 8
 * colours and Braille without. */
ctui_image_renderer ctui_image_renderer_for(ctui_image_renderer want,
					    bool unicode, int colors);

/* Pixels per cell, horizontally and vertically (half 1x2, Braille 2x4,
 * ASCII 1x1); AUTO counts as half. */
void ctui_image_grid(ctui_image_renderer r, int *per_col, int *per_row);

typedef struct ctui_image_layout {
	int col, row;	/* top left of the drawn area in the region, in cells */
	int cols, rows; /* size of the drawn area, in cells */
	int px_w,
	    px_h; /* size of the pixel grid: cols * per_col, rows * per_row */
	double sx, sy, sw,
	    sh; /* the part of the source shown, in source pixels */
} ctui_image_layout;

/* Where an image of w x h pixels goes in a region of cols x rows cells
 * (clamped to CTUI_IMAGE_MAX_CELLS) for the concrete renderer r. The zoom
 * window is pan-clamped to stay inside the image and, in fit mode, the area
 * is the largest one of the window's aspect ratio (rounded to whole cells),
 * centred. False for an empty image or region. */
bool ctui_image_layout_for(int w, int h, int cols, int rows,
			   const ctui_image_view *view, ctui_image_renderer r,
			   ctui_image_layout *out);

/* The same for pixel graphics (CTUI_IMAGE_PIXELS): cells of cell_w x cell_h
 * pixels, whose real aspect keeps the picture in proportion, and a pixel
 * grid of at most CTUI_IMAGE_PIXELS_MAX each way. */
#define CTUI_IMAGE_PIXELS_MAX 2048

bool ctui_image_layout_pixels(int w, int h, int cols, int rows, int cell_w,
			      int cell_h, const ctui_image_view *view,
			      ctui_image_layout *out);

/* Resample the layout's source window to its px_w x px_h grid, row by row
 * into out (px_w * px_h entries). False when the image or layout is not
 * valid. */
bool ctui_image_resample(const ctui_image *img, const ctui_image_layout *lay,
			 ctui_image_scaler scaler, ctui_rgb background,
			 ctui_rgb *out);

/* One terminal cell of a rendered image. */
typedef struct ctui_image_cell {
	char glyph[5]; /* UTF-8, NUL-terminated */
	ctui_rgb fg, bg;
	bool has_bg; /* false: the terminal's background */
} ctui_image_cell;

/* Cells for the pixel grid px (px_w x px_h, a multiple of the renderer's
 * grid; r is a concrete renderer), into out (px_w / per_col * px_h /
 * per_row entries).
 *   half:    U+2580, foreground the upper pixel, background the lower one;
 *   Braille: the dots of the pixels brighter than the cell's mean (all
 *            dots when the cell is nearly flat), foreground their average,
 *            terminal background;
 *   ASCII:   a character of " .:-=+*#%@" by brightness, foreground the
 *            pixel colour. */
void ctui_image_cells(const ctui_rgb *px, int px_w, int px_h,
		      ctui_image_renderer r, ctui_image_cell *out);

/* ---- pixel graphics ----
 *
 * Some terminals draw real pixels: the Kitty graphics protocol (kitty,
 * WezTerm, ghostty) and Sixel (foot, mlterm, contour, iTerm2). With
 * CTUI_IMAGE_PIXELS ctui_image_draw() resamples the image to the pixels of
 * the cells it covers, leaves those cells blank and keeps the escape
 * sequence that draws the picture; the program calls ctui_image_flush()
 * after each refresh() to write it (once: nothing is sent again while the
 * picture stays the same) and to take it off the screen when the image is no
 * longer drawn. Call ctui_image_pixels_reset() when the screen was cleared
 * under it, after a resize for instance. The terminal is found from the
 * environment (CTUI_IMAGE_PROTOCOL=kitty, sixel or none overrides it; inside
 * tmux or screen nothing is sent), so on any other terminal the pixel
 * renderer is half blocks. The encoders and the detection are pure. */
typedef enum ctui_image_protocol {
	CTUI_PROTOCOL_NONE = 0,
	CTUI_PROTOCOL_KITTY,
	CTUI_PROTOCOL_SIXEL
} ctui_image_protocol;

/* The protocol a terminal speaks, from $TERM, $TERM_PROGRAM and whether
 * $KITTY_WINDOW_ID is set. */
ctui_image_protocol ctui_image_protocol_for(const char *term,
					    const char *term_program,
					    bool kitty_window);

/* The same for the environment of the program, with the override and the
 * multiplexer rule above. */
ctui_image_protocol ctui_image_protocol_detect(void);

/* Pixels of a terminal cell (from the window size when the terminal reports
 * it in pixels, else 8 x 16). */
void ctui_image_cell_pixels(int *width, int *height);

/* The escape sequence that draws the pw x ph pixels at the cursor, in
 * `cols` x `rows` cells: Kitty (24-bit RGB in base64 chunks of 4096, with
 * the image id `id`, replies suppressed) or Sixel (the 216 colour cube and
 * 24 greys of the xterm palette, run length encoded). A malloc()ed string to
 * free(), its length in *len; NULL for invalid sizes or no memory. */
char *ctui_image_encode_kitty(const ctui_rgb *px, int pw, int ph, int cols,
			      int rows, int id, size_t *len);
char *ctui_image_encode_sixel(const ctui_rgb *px, int pw, int ph, size_t *len);

/* Write the picture drawn since the last call (see above) to out (stdout
 * for NULL) and flush it. */
void ctui_image_flush(FILE *out);

/* Forget what is on the screen: the next flush sends the picture again. */
void ctui_image_pixels_reset(void);

/* Fit the cells in max_pairs colour pairs: first with the palette of
 * `depth` colours (256, 16 or 8) as is, then with the channels reduced to
 * 8, 5, 3 and 2 levels until the distinct (foreground, background) pairs
 * fit. The cells are changed in place. Returns 0 (as is), the number of
 * levels used, or -1 when even 2 levels do not fit (draw without colour). */
int ctui_image_degrade(ctui_image_cell *cells, size_t n, int depth,
		       size_t max_pairs);

/* Colourless form of a cell: half blocks become a block, upper half, lower
 * half or space by the brightness of their two pixels (ASCII characters
 * when unicode is false); the others stay as they are. */
void ctui_image_cell_mono(ctui_image_cell *c, ctui_image_renderer r,
			  bool unicode);

/* Draw the image in the region of cols x rows cells whose top left is
 * (y, x), after ctui_theme_init(): the renderer comes from
 * ctui_image_renderer_for() with the terminal's locale and colours, the
 * colours from the nearest of the palette (256, 16 or 8 colours by
 * COLORS) through colour pairs reserved from the theme (at most 2048), and
 * without colours the cells are drawn by brightness alone. When the pairs
 * run out the colours are reduced (ctui_image_degrade()); the pairs are
 * shared by every image, so two images in one frame can recolour each
 * other once they are exhausted. Cells outside the image are left alone.
 * Returns the renderer used, or CTUI_IMAGE_AUTO when nothing was drawn
 * (invalid image, empty region or out of memory). */
ctui_image_renderer ctui_image_draw(const ctui_image *img,
				    const ctui_image_view *view, int y, int x,
				    int cols, int rows);

/* A small reader for the two netpbm formats that hold true colour, so that
 * programs and tests need no image library (PNG and JPEG are not read).
 *
 * P6 (binary PPM) and P7 (PAM, with DEPTH 3 or 4, TUPLTYPE RGB or
 * RGB_ALPHA) with a MAXVAL from 1 to 255 are accepted; 16-bit samples, the
 * ASCII variants and every other type are refused, as is a file whose
 * header is malformed, whose size exceeds CTUI_IMAGE_LOAD_MAX_DIM or
 * CTUI_IMAGE_LOAD_MAX_PIXELS, or whose data is truncated. Trailing bytes are
 * ignored. On success the pixels are a malloc()ed buffer to free() and
 * *img describes it (stride = width * channels); on failure NULL is
 * returned and *img is zeroed. */
#define CTUI_IMAGE_LOAD_MAX_DIM 16384
#define CTUI_IMAGE_LOAD_MAX_PIXELS 16777216 /* 2^24 */

unsigned char *ctui_image_parse_pnm(const unsigned char *data, size_t len,
				    ctui_image *img);
unsigned char *ctui_image_load_pnm(const char *path, ctui_image *img);

#endif /* CTUI_IMAGE_H */
