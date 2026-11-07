/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_map.c - the world map: embedded coastline, view, keys, mouse,
 * categories, offline drawing and vector tiles.
 */
#include "check.h"
#include "mvt_encoding.h"
#include "screen.h"

#include <curses.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/map.h>
#include <ctui/text.h>

#define W 80
#define H 24

static void test_coastline(void)
{
	size_t lines = ctui_map_coast_count(), points = 0;
	CHECK(lines == 134);
	int lon_min = 0, lon_max = 0, lat_min = 0, lat_max = 0;
	for (size_t i = 0; i < lines; i++) {
		const int16_t *p = NULL;
		size_t n = ctui_map_coast_line(i, &p);
		CHECK(n >= 2 && p != NULL);
		if (p == NULL)
			continue;
		points += n;
		for (size_t k = 0; k < n; k++) {
			int lon = p[k * 2], lat = p[k * 2 + 1];
			lon_min = lon < lon_min ? lon : lon_min;
			lon_max = lon > lon_max ? lon : lon_max;
			lat_min = lat < lat_min ? lat : lat_min;
			lat_max = lat > lat_max ? lat : lat_max;
		}
	}
	CHECK(points == 5127);
	/* Africa to Siberia, Antarctica to the Arctic: the whole world */
	CHECK(lon_min <= -17000 && lon_max >= 17000);
	CHECK(lat_min <= -8000 && lat_max >= 8000);
	CHECK(lon_min >= -18000 && lon_max <= 18000 && lat_min >= -9000 &&
	      lat_max <= 9000);
	/* the first polyline is the Ross Ice Shelf coast, closed on its start
	 */
	const int16_t *p;
	size_t n = ctui_map_coast_line(0, &p);
	CHECK(n == 11 && p[0] == -16371 && p[1] == -7860);
	CHECK(p[(n - 1) * 2] == p[0] && p[(n - 1) * 2 + 1] == p[1]);
	CHECK(ctui_map_coast_line(lines, &p) == 0 && p == NULL);
}

static int dots_in(const ctui_map *m)
{
	int n = 0;
	for (int y = 0; y < ctui_canvas_height(&m->canvas); y++)
		for (int x = 0; x < ctui_canvas_width(&m->canvas); x++)
			n += ctui_canvas_get(&m->canvas, x, y);
	return n;
}

/* A dot within r dots of the place (lon, lat) on the last drawn canvas, in
 * the copy of the world at `copy` world widths from the centre one. */
static bool dot_near_in_copy(const ctui_map *m, double lon, double lat,
			     int copy, int r)
{
	double wx, wy, px, py;
	ctui_geo_project(lon, lat, &wx, &wy);
	ctui_geo_view_to_pixel(&m->view, wx + copy, wy, m->canvas.cols * 2,
			       m->canvas.rows * 4, &px, &py);
	for (int dy = -r; dy <= r; dy++)
		for (int dx = -r; dx <= r; dx++)
			if (ctui_canvas_get(&m->canvas, (int)lround(px) + dx,
					    (int)lround(py) + dy))
				return true;
	return false;
}

static bool dot_near(const ctui_map *m, double lon, double lat, int r)
{
	for (int copy = -2; copy <= 2; copy++)
		if (dot_near_in_copy(m, lon, lat, copy, r))
			return true;
	return false;
}

static void test_defaults(void)
{
	ctui_map m;
	ctui_map_init(&m);
	CHECK(m.visible[CTUI_MAP_WATER] && m.visible[CTUI_MAP_ROADS] &&
	      m.visible[CTUI_MAP_COASTLINE]);
	CHECK(!m.visible[CTUI_MAP_RAILS] && !m.visible[CTUI_MAP_FERRIES] &&
	      !m.visible[CTUI_MAP_MARITIME]);
	CHECK(m.colors[CTUI_MAP_WATER] == CTUI_ICON_BLUE &&
	      m.colors[CTUI_MAP_COASTLINE] == CTUI_ICON_GREEN);
	CHECK(ctui_map_max_zoom(&m) == CTUI_MAP_OFFLINE_MAX_ZOOM &&
	      !m.dragging);
	CHECK_STR(ctui_map_category_name(CTUI_MAP_RAILS), "rails");
	CHECK_STR(ctui_map_category_name(CTUI_MAP_NOTHING), "");
	CHECK_STR(ctui_map_category_name(CTUI_MAP_CATEGORY_COUNT), "");
	ctui_map_free(&m);
}

static void test_world_view(void)
{
	ctui_map m;
	ctui_map_init(&m);
	CHECK(ctui_map_pan(&m, 0, 0, 1, 1) ==
	      CTUI_MAP_NONE); /* an empty area does nothing */
	ctui_map_reset(&m, W, H);
	/* the whole world: 96 dots high fits 256 pixels at zoom log2(96 / 256)
	 */
	CHECK_NEAR(m.view.zoom, log2(96.0 / 256), 1e-12);
	CHECK_NEAR(m.view.lon, 0, 1e-12);
	CHECK_NEAR(m.view.lat, 0, 1e-12);
	/* no zooming out past the whole world */
	CHECK(ctui_map_zoom(&m, W, H, -1, -1, -1) == CTUI_MAP_NONE);
	CHECK(ctui_map_zoom(&m, W, H, 0, -1, -1) == CTUI_MAP_NONE);
	CHECK(ctui_map_zoom(&m, W, H, 1, -1, -1) == CTUI_MAP_ZOOMED);
	CHECK_NEAR(m.view.zoom, log2(96.0 / 256) + 0.5, 1e-12);
	/* the offline cap */
	for (int i = 0; i < 40; i++)
		ctui_map_zoom(&m, W, H, 1, -1, -1);
	CHECK_NEAR(m.view.zoom, CTUI_MAP_OFFLINE_MAX_ZOOM, 1e-12);
	CHECK(ctui_map_zoom(&m, W, H, 1, 5, 5) == CTUI_MAP_NONE);
	CHECK(ctui_map_zoom(&m, W, H, -2, 5, 5) == CTUI_MAP_ZOOMED);
	ctui_map_reset(&m, W, H);
	CHECK_NEAR(m.view.zoom, log2(96.0 / 256), 1e-12);
	/* a smaller area has a smaller world: the minimum follows the size */
	ctui_map_reset(&m, 40, 12);
	CHECK_NEAR(m.view.zoom, log2(48.0 / 256), 1e-12);
	ctui_map_free(&m);
}

static void test_pan(void)
{
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_reset(&m, W, H);
	ctui_map_zoom(&m, W, H, 6, -1, -1); /* 3 zoom levels in */
	double lon = m.view.lon, lat = m.view.lat;
	CHECK(ctui_map_pan(&m, W, H, 4, 0) == CTUI_MAP_MOVED &&
	      m.view.lon > lon && m.view.lat == lat);
	CHECK(ctui_map_pan(&m, W, H, -8, 0) == CTUI_MAP_MOVED &&
	      m.view.lon < lon);
	CHECK(ctui_map_pan(&m, W, H, 4, 3) == CTUI_MAP_MOVED &&
	      m.view.lat < lat); /* south */
	/* keys */
	ctui_map_reset(&m, W, H);
	ctui_map_zoom(&m, W, H, 6, -1, -1);
	CHECK(ctui_map_key(&m, KEY_RIGHT, W, H) == CTUI_MAP_MOVED &&
	      m.view.lon > 0);
	CHECK(ctui_map_key(&m, 'h', W, H) == CTUI_MAP_MOVED &&
	      fabs(m.view.lon) < 1e-9);
	CHECK(ctui_map_key(&m, 'l', W, H) == CTUI_MAP_MOVED && m.view.lon > 0);
	CHECK(ctui_map_key(&m, KEY_LEFT, W, H) == CTUI_MAP_MOVED);
	CHECK(ctui_map_key(&m, KEY_UP, W, H) == CTUI_MAP_MOVED &&
	      m.view.lat > 0);
	CHECK(ctui_map_key(&m, 'j', W, H) == CTUI_MAP_MOVED &&
	      fabs(m.view.lat) < 1e-9);
	CHECK(ctui_map_key(&m, KEY_DOWN, W, H) == CTUI_MAP_MOVED &&
	      m.view.lat < 0);
	CHECK(ctui_map_key(&m, 'k', W, H) == CTUI_MAP_MOVED);
	/* around the antimeridian and up to the poles */
	for (int i = 0; i < 400; i++)
		ctui_map_key(&m, 'l', W, H);
	CHECK(m.view.lon >= -180 && m.view.lon < 180);
	for (int i = 0; i < 400; i++)
		ctui_map_key(&m, 'k', W, H);
	CHECK(m.view.lat > 80 && m.view.lat <= CTUI_GEO_MAX_LAT + 1e-9);
	CHECK(ctui_map_key(&m, 'k', W, H) ==
	      CTUI_MAP_NONE); /* stuck at the pole */
	ctui_map_free(&m);
}

static void test_keys(void)
{
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_reset(&m, W, H);
	CHECK(ctui_map_key(&m, '+', W, H) == CTUI_MAP_ZOOMED);
	CHECK(ctui_map_key(&m, '=', W, H) == CTUI_MAP_ZOOMED);
	CHECK(ctui_map_key(&m, '-', W, H) == CTUI_MAP_ZOOMED);
	CHECK(ctui_map_key(&m, '_', W, H) == CTUI_MAP_ZOOMED);
	CHECK_NEAR(m.view.zoom, log2(96.0 / 256), 1e-12);
	ctui_map_key(&m, '+', W, H);
	ctui_map_key(&m, 'l', W, H);
	CHECK(ctui_map_key(&m, KEY_HOME, W, H) == CTUI_MAP_RESET &&
	      m.view.lon == 0);
	CHECK_NEAR(m.view.zoom, log2(96.0 / 256), 1e-12);
	ctui_map_key(&m, '+', W, H);
	CHECK(ctui_map_key(&m, '0', W, H) == CTUI_MAP_RESET);
	/* categories */
	CHECK(ctui_map_key(&m, 'r', W, H) == CTUI_MAP_TOGGLED &&
	      m.visible[CTUI_MAP_RAILS]);
	CHECK(ctui_map_key(&m, 'r', W, H) == CTUI_MAP_TOGGLED &&
	      !m.visible[CTUI_MAP_RAILS]);
	CHECK(ctui_map_key(&m, 'c', W, H) == CTUI_MAP_TOGGLED &&
	      !m.visible[CTUI_MAP_COASTLINE]);
	CHECK(ctui_map_key(&m, 'q', W, H) == CTUI_MAP_NONE &&
	      ctui_map_key(&m, KEY_F(2), W, H) == CTUI_MAP_NONE);
	CHECK(ctui_map_key(&m, 'l', 0, 0) == CTUI_MAP_NONE);
	const char *keys = "wdrfbmuacx";
	for (int i = 0; i < CTUI_MAP_CATEGORY_COUNT; i++)
		CHECK(ctui_map_category_for_key(keys[i]) ==
		      (ctui_map_category)i);
	CHECK(ctui_map_category_for_key('z') == CTUI_MAP_NOTHING &&
	      ctui_map_category_for_key(KEY_UP) == CTUI_MAP_NOTHING);
	CHECK(ctui_map_set_visible(&m, CTUI_MAP_WATER, false) ==
	      CTUI_MAP_TOGGLED);
	CHECK(ctui_map_set_visible(&m, CTUI_MAP_WATER, false) == CTUI_MAP_NONE);
	CHECK(ctui_map_set_visible(&m, CTUI_MAP_NOTHING, true) ==
	      CTUI_MAP_NONE);
	CHECK(ctui_map_set_visible(&m, CTUI_MAP_CATEGORY_COUNT, true) ==
	      CTUI_MAP_NONE);
	ctui_map_free(&m);
}

static void test_colors(void)
{
	ctui_map m;
	ctui_map_init(&m);
	CHECK(ctui_map_color(&m, CTUI_MAP_WATER, NULL) == CTUI_ICON_BLUE);
	CHECK(ctui_map_color(&m, CTUI_MAP_WATER, "motorway") == CTUI_ICON_BLUE);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, NULL) == CTUI_ICON_GRAY);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "service") == CTUI_ICON_GRAY);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "motorway") ==
	      CTUI_ICON_ORANGE);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "motorway_construction") ==
	      CTUI_ICON_ORANGE);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "trunk") == CTUI_ICON_ORANGE);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "primary") ==
	      CTUI_ICON_YELLOW);
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "tertiary") ==
	      CTUI_ICON_YELLOW);
	m.colors[CTUI_MAP_ROADS] =
	    CTUI_ICON_CYAN; /* a chosen colour covers every class */
	CHECK(ctui_map_color(&m, CTUI_MAP_ROADS, "motorway") == CTUI_ICON_CYAN);
	CHECK(ctui_map_color(&m, CTUI_MAP_NOTHING, NULL) == CTUI_ICON_GRAY);
	ctui_map_free(&m);
}

static void test_hit_and_position(void)
{
	ctui_map m;
	ctui_map_init(&m);
	CHECK(ctui_map_hit(W, H, 0, 0) && ctui_map_hit(W, H, H - 1, W - 1));
	CHECK(!ctui_map_hit(W, H, -1, 0) && !ctui_map_hit(W, H, 0, W) &&
	      !ctui_map_hit(W, H, H, 0));
	double lon, lat;
	CHECK(!ctui_map_lonlat_at(&m, W, H, -1, 3, &lon, &lat));
	ctui_map_reset(&m, W, H);
	CHECK(ctui_map_lonlat_at(&m, W, H, H / 2, W / 2, &lon, &lat));
	CHECK(fabs(lon) < 8 && fabs(lat) < 8);
	CHECK(ctui_map_lonlat_at(&m, W, H, 0, W / 2, &lon, &lat) && lat > 80);
	CHECK(ctui_map_lonlat_at(&m, W, H, H - 1, W / 2, &lon, &lat) &&
	      lat < -80);
	/* the world is 96 dots wide in 160: the left edge shows the copy before
	 */
	CHECK(ctui_map_lonlat_at(&m, W, H, H / 2, 0, &lon, &lat) &&
	      lon >= -180 && lon < 180);
	ctui_map_free(&m);
}

static void test_mouse(void)
{
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_reset(&m, W, H);
	ctui_map_zoom(&m, W, H, 6, -1, -1);
	CHECK(ctui_map_drag(&m, W, H, 5, 5) ==
	      CTUI_MAP_NONE); /* nothing pressed */
	CHECK(!ctui_map_release(&m));
	CHECK(!ctui_map_press(&m, W, H, -1, 5) && !m.dragging);
	CHECK(ctui_map_press(&m, W, H, 10, 40) && m.dragging);
	double lon0, lat0;
	CHECK(ctui_map_lonlat_at(&m, W, H, 10, 40, &lon0, &lat0));
	CHECK(ctui_map_drag(&m, W, H, 10, 50) == CTUI_MAP_MOVED);
	double lon, lat;
	ctui_map_lonlat_at(&m, W, H, 10, 50, &lon, &lat);
	CHECK_NEAR(lon, lon0, 1e-6); /* the point stays under the pointer */
	CHECK_NEAR(lat, lat0, 1e-6);
	CHECK(ctui_map_drag(&m, W, H, 14, 50) == CTUI_MAP_MOVED);
	ctui_map_lonlat_at(&m, W, H, 14, 50, &lon, &lat);
	CHECK_NEAR(lat, lat0, 1e-6);
	CHECK(ctui_map_drag(&m, W, H, 14, 50) == CTUI_MAP_NONE); /* no move */
	CHECK(ctui_map_drag(&m, W, H, -5, 200) ==
	      CTUI_MAP_MOVED); /* outside is fine */
	CHECK(ctui_map_release(&m) && !m.dragging);
	CHECK(ctui_map_drag(&m, W, H, 3, 3) == CTUI_MAP_NONE);
	/* the wheel zooms where it points: the place under the pointer stays */
	CHECK(ctui_map_lonlat_at(&m, W, H, 8, 60, &lon0, &lat0));
	double zoom = m.view.zoom;
	CHECK(ctui_map_wheel(&m, W, H, 8, 60, 1) == CTUI_MAP_ZOOMED &&
	      m.view.zoom > zoom);
	ctui_map_lonlat_at(&m, W, H, 8, 60, &lon, &lat);
	CHECK_NEAR(lon, lon0, 0.5);
	CHECK_NEAR(lat, lat0, 0.5);
	CHECK(ctui_map_wheel(&m, W, H, 8, 60, -1) == CTUI_MAP_ZOOMED);
	CHECK(ctui_map_wheel(&m, W, H, 8, 60, 0) == CTUI_MAP_NONE);
	ctui_map_free(&m);
}

static void test_draw_world(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(dots_in(&m) > 400);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	/* Braille characters are U+2800 .. U+28FF: UTF-8 e2 a0 .. e2 a3 */
	CHECK(screen_has_text("\xe2\xa0") || screen_has_text("\xe2\xa1") ||
	      screen_has_text("\xe2\xa2") || screen_has_text("\xe2\xa3"));
	/* West Africa, southern Africa and Japan have dots, the Pacific and
	 * inland Africa none */
	CHECK(dot_near(&m, -17.4, 14.7, 3));
	CHECK(dot_near(&m, 18.4, -34.3, 3));
	CHECK(dot_near(&m, 139.7, 35.6, 3));
	CHECK(!dot_near(&m, -150, 0, 1));
	CHECK(!dot_near(&m, 25, 5, 2));
	/* the world is 96 dots wide in a 160-dot area: copies of it on both
	 * sides */
	int left = 0, right = 0, mid = 0;
	for (int x = 0; x < 160; x++)
		for (int y = 0; y < 96; y++) {
			bool d = ctui_canvas_get(&m.canvas, x, y);
			if (x < 30)
				left += d;
			else if (x >= 130)
				right += d;
			else
				mid += d;
		}
	CHECK(left > 20 && right > 20 && mid > 200);
	/* hiding the coastline leaves the area blank */
	ctui_map_set_visible(&m, CTUI_MAP_COASTLINE, false);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(dots_in(&m) == 0 &&
	      !screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	/* a colour of its own */
	ctui_map_set_visible(&m, CTUI_MAP_COASTLINE, true);
	m.colors[CTUI_MAP_COASTLINE] = CTUI_ICON_ORANGE;
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	ctui_map_free(&m);
}

/* Every vertex of the coastline that falls inside the picture has a dot. */
static int missing_vertices(const ctui_map *m, int *visible)
{
	int missing = 0, pw = m->canvas.cols * 2, ph = m->canvas.rows * 4;
	*visible = 0;
	for (size_t i = 0; i < ctui_map_coast_count(); i++) {
		const int16_t *p;
		size_t n = ctui_map_coast_line(i, &p);
		for (size_t k = 0; k < n; k++) {
			double lon = p[k * 2] / 100.0,
			       lat = p[k * 2 + 1] / 100.0;
			if (lat < -85 || lat > 85)
				continue;
			for (int copy = -2; copy <= 2; copy++) {
				double wx, wy, px, py;
				ctui_geo_project(lon, lat, &wx, &wy);
				ctui_geo_view_to_pixel(&m->view, wx + copy, wy,
						       pw, ph, &px, &py);
				if (px < 2 || py < 2 || px > pw - 3 ||
				    py > ph - 3)
					continue;
				(*visible)++;
				if (!dot_near_in_copy(m, lon, lat, copy, 1))
					missing++;
			}
		}
	}
	return missing;
}

static void test_draw_zoom_and_wrap(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_draw(&m, 0, 0, W, H);
	int world = dots_in(&m);
	int visible;
	CHECK(missing_vertices(&m, &visible) == 0 && visible > 1000);
	/* one world to the east is the same picture */
	ctui_map_pan(&m, W, H, ctui_geo_view_scale(&m.view) / 2, 0);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(abs(dots_in(&m) - world) < world / 50);
	/* Europe at the zoom cap */
	ctui_map_reset(&m, W, H);
	m.view.lon = 10;
	m.view.lat = 50;
	ctui_geo_view_set_zoom(&m.view, 100);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(m.view.zoom == CTUI_MAP_OFFLINE_MAX_ZOOM);
	CHECK(dots_in(&m) > 20 && dots_in(&m) < world);
	int miss = missing_vertices(&m, &visible);
	CHECK(miss == 0 && visible >= 1);
	/* across the antimeridian: Siberia on one side, Alaska on the other,
	 * both seen */
	ctui_map_reset(&m, W, H);
	m.view.lon = 180;
	m.view.lat = 65;
	ctui_geo_view_set_zoom(&m.view, 2);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(missing_vertices(&m, &visible) == 0 && visible > 20);
	bool east = false, west = false;
	for (int x = 0; x < 160; x++)
		for (int y = 0; y < 96; y++)
			if (ctui_canvas_get(&m.canvas, x, y)) {
				east = east || x < 80;
				west = west || x >= 80;
			}
	CHECK(east && west);
	ctui_map_free(&m);
}

static void test_draw_misc(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_draw(&m, 0, 0, 0, 0); /* nothing */
	ctui_map_draw(&m, 0, 0, 1, 1);
	ctui_map_draw(&m, 0, 0, 3, 2);
	/* a resize: the canvas follows and the zoom stays above the new minimum
	 */
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(m.canvas.cols == W && m.canvas.rows == H);
	ctui_map_zoom(&m, W, H, 4, -1, -1);
	ctui_map_draw(&m, 0, 0, 120, 40);
	CHECK(m.canvas.cols == 120 && m.canvas.rows == 40);
	CHECK(m.view.zoom >= ctui_geo_fit_zoom(240, 160) - 1e-12);
	ctui_map_draw(&m, 0, 0, 20, 6);
	CHECK(m.canvas.cols == 20 &&
	      m.view.zoom >= ctui_geo_fit_zoom(40, 24) - 1e-12);
	/* attribution, bottom right */
	ctui_map_draw(&m, 0, 0, W, H);
	m.attribution = "(c) Natural Earth";
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_text("(c) Natural Earth"));
	char row[256];
	mvinnstr(H - 1, W - 17, row, 17);
	CHECK_STR(row, "(c) Natural Earth");
	m.attribution = "a very long attribution that cannot fit";
	screen_clear();
	ctui_map_draw(&m, 0, 0, 12, 4);
	CHECK(screen_has_text("a very long "));
	m.attribution = NULL;
	ctui_map_free(&m);
}

/* ---- vector tiles ---- */

typedef struct source {
	const mb_buf *tile;   /* what every tile is, NULL = nothing available */
	int serve_x, serve_y; /* when >= 0, only this tile is available */
	int calls;
	int z[128], x[128], y[128];
} source;

static bool fake_tile(void *ctx, int z, int x, int y, const uint8_t **data,
		      size_t *len)
{
	source *s = ctx;
	if (s->calls < 128) {
		s->z[s->calls] = z;
		s->x[s->calls] = x;
		s->y[s->calls] = y;
	}
	s->calls++;
	if (!s->tile ||
	    (s->serve_x >= 0 && (x != s->serve_x || y != s->serve_y)))
		return false;
	*data = s->tile->p;
	*len = s->tile->len;
	return true;
}

static bool asked(const source *s, int z, int x, int y)
{
	for (int i = 0; i < s->calls && i < 128; i++)
		if (s->z[i] == z && s->x[i] == x && s->y[i] == y)
			return true;
	return false;
}

/* A tile with an ocean ring along its borders and a small lake, a motorway
 * and a railway crossing it, a place label (never drawn) and a shop point. */
static mb_buf *world_tile(void)
{
	static const int BORDER[][2] = {
	    {0, 0}, {4096, 0}, {4096, 4096}, {0, 4096}};
	static const int LAKE[][2] = {{1500, 1500}, {2600, 1500}, {2600, 2600}};
	static const int MOTORWAY[][2] = {{0, 0}, {4096, 4096}};
	static const int RAIL[][2] = {{0, 4096}, {4096, 0}};
	static const uint64_t CLASS_MOTORWAY[] = {0, 0}, CLASS_RAIL[] = {0, 1};
	const char *keys[] = {"class"};
	mb_buf *classes[] = {mb_value_string("motorway"),
			     mb_value_string("rail")};
	mb_buf *ocean = mb_path_feature(1, 3, BORDER, 4, NULL, 0);
	mb_buf *lake = mb_path_feature(2, 3, LAKE, 3, NULL, 0);
	mb_buf *water_features[] = {ocean, lake};
	mb_buf *road = mb_path_feature(3, 2, MOTORWAY, 2, CLASS_MOTORWAY, 2);
	mb_buf *rail = mb_path_feature(4, 2, RAIL, 2, CLASS_RAIL, 2);
	mb_buf *roads[] = {road, rail};
	mb_buf *label = mb_point_feature(5, 2048, 2048);
	mb_buf *layers[] = {
	    mb_layer("water", 4096, NULL, 0, NULL, 0, water_features, 2),
	    mb_layer("transportation", 4096, keys, 1, classes, 2, roads, 2),
	    mb_layer("place", 4096, NULL, 0, NULL, 0, &label, 1),
	};
	return mb_tile(layers, 3);
}

/* The view at the middle of tile (5, 16, 10), at zoom 5. */
static void look_at_tile(ctui_map *m, int w, int h)
{
	ctui_map_reset(m, w, h);
	ctui_geo_from_tile(16.5, 10.5, 5, &m->view.lon, &m->view.lat);
	ctui_geo_view_set_zoom(&m->view, 5);
}

static void test_tile_categories(void)
{
	mb_buf *tile = world_tile();
	ctui_mvt_tile t;
	CHECK(ctui_mvt_decode(tile->p, tile->len, &t) == CTUI_MVT_OK);
	CHECK(ctui_map_categorize(&t.layers[0], &t.layers[0].features[0]) ==
	      CTUI_MAP_WATER);
	CHECK(ctui_map_categorize(&t.layers[1], &t.layers[1].features[0]) ==
	      CTUI_MAP_ROADS);
	CHECK(ctui_map_categorize(&t.layers[1], &t.layers[1].features[1]) ==
	      CTUI_MAP_RAILS);
	CHECK(ctui_map_categorize(&t.layers[2], &t.layers[2].features[0]) ==
	      CTUI_MAP_NOTHING);
	ctui_mvt_free(&t);
	mb_release();

	/* the layer name decides, and a few properties */
	static const struct {
		const char *layer, *key, *text;
		int number;
		ctui_map_category want;
	} CASES[] = {
	    {"waterway", NULL, NULL, 0, CTUI_MAP_WATER},
	    {"boundary", NULL, NULL, 0, CTUI_MAP_BOUNDARIES},
	    {"boundary", "maritime", NULL, 1, CTUI_MAP_MARITIME},
	    {"boundary", "maritime", NULL, 0, CTUI_MAP_BOUNDARIES},
	    {"transportation", "class", "ferry", 0, CTUI_MAP_FERRIES},
	    {"transportation", "class", "transit", 0, CTUI_MAP_RAILS},
	    {"transportation", "class", "path", 0, CTUI_MAP_ROADS},
	    {"transportation", NULL, NULL, 0, CTUI_MAP_ROADS},
	    {"building", NULL, NULL, 0, CTUI_MAP_BUILDINGS},
	    {"aeroway", NULL, NULL, 0, CTUI_MAP_AEROWAY},
	    {"housenumber", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"landcover", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"landuse", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"park", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"globallandcover", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"water_name", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"poi", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"mountain_peak", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"aerodrome_label", NULL, NULL, 0, CTUI_MAP_NOTHING},
	    {"mystery", NULL, NULL, 0, CTUI_MAP_OTHER},
	};
	for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
		static const int LINE[][2] = {{0, 0}, {9, 9}};
		const char *keys[] = {CASES[i].key ? CASES[i].key : "unused"};
		mb_buf *value = CASES[i].text
				    ? mb_value_string(CASES[i].text)
				    : mb_value_int(4, CASES[i].number);
		uint64_t tags[] = {0, 0};
		mb_buf *feat =
		    mb_path_feature(1, 2, LINE, 2, CASES[i].key ? tags : NULL,
				    CASES[i].key ? 2 : 0);
		mb_buf *layer = mb_layer(CASES[i].layer, 4096, keys, 1, &value,
					 1, &feat, 1);
		mb_buf *one = mb_tile(&layer, 1);
		CHECK(ctui_mvt_decode(one->p, one->len, &t) == CTUI_MVT_OK);
		CHECK(ctui_map_categorize(&t.layers[0],
					  &t.layers[0].features[0]) ==
		      CASES[i].want);
		ctui_mvt_free(&t);
		mb_release();
	}
}

static void test_offline_ignores_tiles(void)
{
	ctui_map m;
	ctui_map_init(&m);
	mb_buf *tile = world_tile();
	source s = {tile, -1, -1, 0, {0}, {0}, {0}};
	m.tiles = fake_tile;
	m.tiles_ctx = &s;
	CHECK(!m.online && ctui_map_max_zoom(&m) == CTUI_MAP_OFFLINE_MAX_ZOOM);
	ctui_map_reset(&m, W, H);
	for (int i = 0; i < 40; i++)
		ctui_map_zoom(&m, W, H, 1, -1, -1);
	CHECK(m.view.zoom == CTUI_MAP_OFFLINE_MAX_ZOOM);
	screen_open(H, W);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(s.calls == 0 && ctui_map_cached(&m) == 0 && dots_in(&m) >= 0);
	/* online without a callback is still capped */
	ctui_map_free(&m);
	ctui_map_init(&m);
	ctui_map_set_online(&m, true);
	CHECK(ctui_map_max_zoom(&m) == CTUI_MAP_OFFLINE_MAX_ZOOM);
	ctui_map_reset(&m, W, H);
	for (int i = 0; i < 40; i++)
		ctui_map_zoom(&m, W, H, 1, -1, -1);
	CHECK(m.view.zoom == CTUI_MAP_OFFLINE_MAX_ZOOM);
	ctui_map_free(&m);
	mb_release();
}

static void test_online_tiles(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	mb_buf *tile = world_tile();
	source s = {tile, 16, 10, 0, {0}, {0}, {0}};
	m.tiles = fake_tile;
	m.tiles_ctx = &s;
	ctui_map_set_online(&m, true);
	CHECK(ctui_map_max_zoom(&m) == CTUI_MAP_ONLINE_MAX_ZOOM);
	look_at_tile(&m, W, H);
	CHECK(m.view.zoom == 5);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(asked(&s, 5, 16, 10));
	CHECK(ctui_map_cached(&m) == 1 && m.missing == 0);
	/* the motorway crosses the middle of the picture, in the road colour */
	CHECK(ctui_canvas_get(&m.canvas, 80, 48) ||
	      ctui_canvas_get(&m.canvas, 79, 47) ||
	      ctui_canvas_get(&m.canvas, 81, 49));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	CHECK(!screen_has_color(
	    ctui_attr_icon(CTUI_ICON_PURPLE, false))); /* rails are off */
	/* the lake is blue */
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_BLUE, false)));
	/* a second draw is served by the cache */
	int calls = s.calls;
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(s.calls == calls);
	/* rails on */
	CHECK(ctui_map_key(&m, 'r', W, H) == CTUI_MAP_TOGGLED);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_PURPLE, false)));
	/* water and roads off */
	ctui_map_key(&m, 'w', W, H);
	ctui_map_key(&m, 'd', W, H);
	ctui_map_key(&m, 'r', W, H);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(dots_in(&m) == 0);
	/* the coastline is not drawn over tiles */
	ctui_map_key(&m, 'w', W, H);
	ctui_map_key(&m, 'c', W, H);
	ctui_map_key(&m, 'c', W, H);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(dots_in(&m) > 0 && dots_in(&m) < 400);
	/* turning online off brings the zoom down and the coastline back */
	ctui_map_set_online(&m, false);
	CHECK(m.view.zoom == CTUI_MAP_OFFLINE_MAX_ZOOM);
	calls = s.calls;
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(s.calls == calls && ctui_map_cached(&m) == 1);
	ctui_map_free(&m);
	mb_release();
}

static void test_tile_borders_and_missing(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	mb_buf *tile = world_tile();
	source s = {tile, 16, 10, 0, {0}, {0}, {0}};
	m.tiles = fake_tile;
	m.tiles_ctx = &s;
	ctui_map_set_online(&m, true);
	look_at_tile(&m, 160, 70); /* 320 x 280 dots: the 256-dot tile and parts
				      of its neighbours */
	ctui_map_draw(&m, 0, 0, 160, 70);
	CHECK(m.missing == 8); /* the 3 x 3 tiles around, one served */
	CHECK(ctui_map_cached(&m) == 1);
	/* the ocean outline follows the tile edges: x = 32 .. 288, y = 12 ..
	 * 268, none drawn */
	int on_border = 0;
	for (int d = 14; d < 266; d++)
		on_border += ctui_canvas_get(&m.canvas, 32, d) +
			     ctui_canvas_get(&m.canvas, 288, d);
	for (int d = 34; d < 286; d++)
		on_border += ctui_canvas_get(&m.canvas, d, 12) +
			     ctui_canvas_get(&m.canvas, d, 268);
	CHECK(on_border == 0);
	/* neighbours are asked again at every draw, until they are available */
	int calls = s.calls;
	ctui_map_draw(&m, 0, 0, 160, 70);
	CHECK(s.calls == calls + 8);
	CHECK(asked(&s, 5, 15, 9) && asked(&s, 5, 17, 11));
	s.serve_x = -1;
	ctui_map_draw(&m, 0, 0, 160, 70);
	CHECK(m.missing == 0 && ctui_map_cached(&m) == 9);
	ctui_map_free(&m);
	mb_release();
}

static void test_tile_zoom_and_wrap(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	mb_buf *tile = world_tile();
	source s = {tile, -1, -1, 0, {0}, {0}, {0}};
	m.tiles = fake_tile;
	m.tiles_ctx = &s;
	ctui_map_set_online(&m, true);
	/* overzoom: the source stops at zoom 6, tiles of 6 are magnified at
	 * zoom 9 */
	m.source_max_zoom = 6;
	look_at_tile(&m, W, H);
	ctui_geo_view_set_zoom(&m.view, 9);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(s.calls > 0);
	for (int i = 0; i < s.calls && i < 128; i++)
		CHECK(s.z[i] == 6);
	CHECK(dots_in(&m) > 0);
	/* across the antimeridian the columns wrap */
	ctui_map_free(&m);
	ctui_map_init(&m);
	m.tiles = fake_tile;
	m.tiles_ctx = &s;
	s.calls = 0;
	ctui_map_set_online(&m, true);
	ctui_map_reset(&m, W, H);
	m.view.lon = 179.99;
	m.view.lat = 0;
	ctui_geo_view_set_zoom(&m.view, 5);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(asked(&s, 5, 31, 15) && asked(&s, 5, 0, 15));
	for (int i = 0; i < s.calls && i < 128; i++)
		CHECK(s.x[i] >= 0 && s.x[i] < 32 && s.y[i] >= 0 && s.y[i] < 32);
	/* and the poles stop the rows */
	m.view.lat = 85;
	s.calls = 0;
	ctui_map_draw(&m, 0, 0, W, H);
	for (int i = 0; i < s.calls && i < 128; i++)
		CHECK(s.y[i] >= 0);
	ctui_map_free(&m);
	mb_release();
}

static void test_tile_cache_and_garbage(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	mb_buf *tile = world_tile();
	source s = {tile, -1, -1, 0, {0}, {0}, {0}};
	m.tiles = fake_tile;
	m.tiles_ctx = &s;
	ctui_map_set_online(&m, true);
	CHECK(ctui_map_set_cache(&m, 2, 0));
	look_at_tile(&m, 160, 70);
	ctui_map_draw(&m, 0, 0, 160,
		      70); /* nine tiles through a cache of two */
	CHECK(ctui_map_cached(&m) == 2 && m.missing == 0);
	CHECK(!ctui_map_set_cache(&m, 0, 0));
	CHECK(ctui_map_cached(&m) == 0);
	ctui_map_free(&m);

	/* bytes that are not a tile: drawn as nothing, and not asked for again
	 */
	ctui_map_init(&m);
	static const uint8_t JUNK[] = {0x1a, 0x7f, 0x01, 0xff};
	mb_buf junk = {(uint8_t *)JUNK, sizeof JUNK, sizeof JUNK};
	source js = {&junk, 16, 10, 0, {0}, {0}, {0}};
	m.tiles = fake_tile;
	m.tiles_ctx = &js;
	ctui_map_set_online(&m, true);
	look_at_tile(&m, W, H);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(dots_in(&m) == 0 && ctui_map_cached(&m) == 1);
	int calls = js.calls;
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(js.calls == calls);
	ctui_map_free(&m);
	mb_release();
}

static const ctui_map_marker CITIES[] = {
    {.lon = 2.35,
     .lat = 48.86,
     .title = "Paris",
     .text = "lat 48.86 N\nlng 2.35 E"},
    {.lon = 139.69,
     .lat = 35.69,
     .title = "Tokyo",
     .text = "lat 35.69 N\nlng 139.69 E"},
    {.lon = 151.21, .lat = -33.87, .title = "Sydney"},
};

/* The cell where marker i is drawn; false when it is not in view. */
static bool find_cell(ctui_map *m, int i, int *cy, int *cx)
{
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			if (ctui_map_marker_at(m, W, H, y, x) == i) {
				*cy = y;
				*cx = x;
				return true;
			}
	return false;
}

static void test_marker_selection(void)
{
	ctui_map m;
	ctui_map_init(&m);
	CHECK(m.marker == -1 && !m.bubble && m.marker_count == 0);
	CHECK(ctui_map_select_step(&m, W, H, 1) == CTUI_MAP_NONE);
	ctui_map_set_markers(&m, CITIES, 3);
	CHECK(m.marker_count == 3 && m.marker == -1);
	CHECK(ctui_map_key(&m, '\t', W, H) == CTUI_MAP_SELECTED &&
	      m.marker == 0);
	CHECK(ctui_map_key(&m, '\t', W, H) == CTUI_MAP_SELECTED &&
	      m.marker == 1);
	CHECK(ctui_map_key(&m, '\t', W, H) == CTUI_MAP_SELECTED &&
	      m.marker == 2);
	CHECK(ctui_map_key(&m, '\t', W, H) == CTUI_MAP_SELECTED &&
	      m.marker == 0);
	CHECK(ctui_map_key(&m, KEY_BTAB, W, H) == CTUI_MAP_SELECTED &&
	      m.marker == 2);
	CHECK(ctui_map_select(&m, W, H, 2) == CTUI_MAP_NONE);
	CHECK(ctui_map_select(&m, W, H, 7) == CTUI_MAP_SELECTED &&
	      m.marker == -1);
	CHECK(ctui_map_select_step(&m, W, H, -1) == CTUI_MAP_SELECTED &&
	      m.marker == 2);
	/* a new set of markers clears the selection and the bubble */
	ctui_map_set_bubble(&m, true);
	CHECK(m.bubble);
	ctui_map_set_markers(&m, CITIES, 2);
	CHECK(m.marker == -1 && !m.bubble && m.marker_count == 2);
	ctui_map_set_markers(&m, NULL, 5);
	CHECK(m.marker_count == 0);
	ctui_map_free(&m);
}

static void test_marker_bubble_keys(void)
{
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_set_markers(&m, CITIES, 3);
	/* nothing to show without a selection */
	CHECK(ctui_map_key(&m, '\n', W, H) == CTUI_MAP_NONE && !m.bubble);
	ctui_map_key(&m, '\t', W, H);
	CHECK(ctui_map_key(&m, '\n', W, H) == CTUI_MAP_BUBBLE && m.bubble);
	CHECK(ctui_map_key(&m, KEY_ENTER, W, H) == CTUI_MAP_BUBBLE &&
	      !m.bubble);
	CHECK(ctui_map_key(&m, '\r', W, H) == CTUI_MAP_BUBBLE && m.bubble);
	CHECK(ctui_map_key(&m, 27, W, H) == CTUI_MAP_BUBBLE && !m.bubble);
	CHECK(ctui_map_key(&m, 27, W, H) == CTUI_MAP_NONE);
	/* moving the selection hides the bubble */
	ctui_map_key(&m, '\n', W, H);
	ctui_map_key(&m, '\t', W, H);
	CHECK(m.marker == 1 && !m.bubble);
	ctui_map_free(&m);
}

static void test_marker_cells(void)
{
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_set_markers(&m, CITIES, 3);
	int py = 0, px = 0, ty = 0, tx = 0, sy = 0, sx = 0;
	CHECK(find_cell(&m, 0, &py, &px) && find_cell(&m, 1, &ty, &tx) &&
	      find_cell(&m, 2, &sy, &sx));
	/* Paris is north-west of Tokyo and Sydney is south of both */
	CHECK(px < tx && sy > ty && sy > py);
	CHECK(ctui_map_marker_at(&m, W, H, -1, 0) == -1);
	CHECK(ctui_map_marker_at(&m, W, H, 0, W) == -1);
	CHECK(ctui_map_marker_at(&m, 0, 0, 0, 0) == -1);
	/* the cell under the marker reads back as the place */
	double lon, lat;
	CHECK(ctui_map_lonlat_at(&m, W, H, py, px, &lon, &lat));
	CHECK(fabs(lon - 2.35) < 2 * 360.0 / W && fabs(lat - 48.86) < 20);
	/* zoomed in on Paris, the others are out of view */
	m.view.lon = 2.35;
	m.view.lat = 48.86;
	ctui_map_zoom(&m, W, H, 8, -1, -1);
	CHECK(find_cell(&m, 0, &py, &px) && !find_cell(&m, 1, &ty, &tx));
	/* selecting a marker out of view brings it in */
	CHECK(ctui_map_select(&m, W, H, 1) == CTUI_MAP_SELECTED);
	CHECK(find_cell(&m, 1, &ty, &tx));
	CHECK(fabs(m.view.lon - 139.69) < 1 && fabs(m.view.lat - 35.69) < 1);
	ctui_map_free(&m);
}

static void test_marker_draw(void)
{
	screen_open(H, W);
	ctui_map m;
	ctui_map_init(&m);
	m.visible[CTUI_MAP_COASTLINE] = false; /* only the markers are drawn */
	ctui_map_set_markers(&m, CITIES, 3);
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(dots_in(&m) > 0);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_YELLOW, false)));
	CHECK(!screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	CHECK(!screen_has_text("Paris") && !screen_has_text("lat 48.86"));
	/* each marker is a disc of five dots, like a scatter point */
	CHECK(dots_in(&m) == 15);
	int ty = 0, tx = 0;
	CHECK(find_cell(&m, 1, &ty, &tx));
	CHECK(ctui_canvas_get(&m.canvas, tx * 2, ty * 4) ||
	      ctui_canvas_get(&m.canvas, tx * 2 + 1, ty * 4 + 1) ||
	      ctui_canvas_get(&m.canvas, tx * 2, ty * 4 + 2) ||
	      ctui_canvas_get(&m.canvas, tx * 2 + 1, ty * 4 + 3));
	/* the selected one is larger and in its own colour */
	ctui_map_select(&m, W, H, 1);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	CHECK(dots_in(&m) > 15 && !screen_has_text("Tokyo"));
	/* the bubble: the title in the top edge, the lines inside */
	ctui_map_set_bubble(&m, true);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_text("Tokyo") && screen_has_text("lat 35.69 N") &&
	      screen_has_text("lng 139.69 E"));
	/* a marker without text still gets a bubble with its title */
	ctui_map_select(&m, W, H, 2);
	ctui_map_set_bubble(&m, true);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_text("Sydney") && !screen_has_text("lat 35.69"));
	/* the bubble stays inside a small area, and below a marker at the top
	 */
	ctui_map_select(&m, W, H, 0);
	ctui_map_set_bubble(&m, true);
	ctui_map_draw(&m, 2, 3, 30, 8);
	ctui_map_draw(&m, 0, 0, 12, 4);
	ctui_map_draw(&m, 0, 0, 5, 3); /* too small for a bubble */
	m.marker_color = CTUI_ICON_PURPLE;
	ctui_map_select(&m, W, H, 1);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_PURPLE, false)));
	ctui_map_free(&m);
	endwin();
}

static void test_marker_icons(void)
{
	screen_open(H, W);
	static const ctui_map_marker ICONS[] = {
	    {.lon = 2.35,
	     .lat = 48.86,
	     .icon = "\xe2\x98\x85",
	     .ascii_icon = "#",
	     .title = "Paris"},
	    {.lon = 139.69, .lat = 35.69, .icon = "<>", .title = "Tokyo"},
	    {.lon = 151.21, .lat = -33.87, .title = "Sydney"},
	};
	ctui_map m;
	ctui_map_init(&m);
	m.visible[CTUI_MAP_COASTLINE] = false;
	ctui_map_set_markers(&m, ICONS, 3);
	ctui_map_draw(&m, 0, 0, W, H);
	/* the icons are text, only Sydney is a disc of dots */
	CHECK(screen_has_text("\xe2\x98\x85") && screen_has_text("<>"));
	CHECK(dots_in(&m) == 5);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_YELLOW, false)));
	/* a click on any column of an icon hits it */
	int py = 0, px = 0, ty = 0, tx = 0;
	CHECK(find_cell(&m, 0, &py, &px) && find_cell(&m, 1, &ty, &tx));
	CHECK(ctui_map_marker_at(&m, W, H, ty, tx + 1) == 1);
	CHECK(ctui_map_marker_at(&m, W, H, ty, tx + 2) == -1);
	CHECK(ctui_map_marker_at(&m, W, H, py, px + 1) == -1);
	/* the selected icon takes the selected colour */
	ctui_map_select(&m, W, H, 1);
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	/* without UTF-8 the ASCII icon is drawn, or "*" when there is none */
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	screen_clear();
	ctui_map_draw(&m, 0, 0, W, H);
	CHECK(screen_has_text("#") && !screen_has_text("\xe2\x98\x85"));
	CHECK(!screen_has_text("<>"));
	ctui_map_free(&m);
	endwin();
	unsetenv("CTUI_ASCII");
}

static void test_marker_mouse(void)
{
	ctui_map m;
	ctui_map_init(&m);
	ctui_map_set_markers(&m, CITIES, 3);
	int ty = 0, tx = 0, py = 0, px = 0;
	CHECK(find_cell(&m, 1, &ty, &tx) && find_cell(&m, 0, &py, &px));
	/* a click on a star selects it and shows its bubble */
	CHECK(ctui_map_press(&m, W, H, ty, tx));
	CHECK(m.marker == -1); /* nothing happens before the release */
	CHECK(ctui_map_release(&m));
	CHECK(m.marker == 1 && m.bubble);
	/* a second click on the same star hides the bubble */
	ctui_map_press(&m, W, H, ty, tx);
	ctui_map_release(&m);
	CHECK(m.marker == 1 && !m.bubble);
	ctui_map_press(&m, W, H, ty, tx);
	ctui_map_release(&m);
	CHECK(m.bubble);
	/* a click on another star moves the selection */
	ctui_map_press(&m, W, H, py, px);
	ctui_map_release(&m);
	CHECK(m.marker == 0 && m.bubble);
	/* a click elsewhere hides the bubble and keeps the selection */
	ctui_map_press(&m, W, H, H - 1, 0);
	ctui_map_release(&m);
	CHECK(m.marker == 0 && !m.bubble);
	/* a drag starting on a star pans the map and is not a click */
	double lon = m.view.lon;
	ctui_map_press(&m, W, H, py, px);
	ctui_map_drag(&m, W, H, py, px + 3);
	ctui_map_release(&m);
	CHECK(!m.bubble && m.marker == 0 && m.view.lon != lon);
	/* a press outside the area is no click */
	CHECK(!ctui_map_press(&m, W, H, -1, 0));
	CHECK(!ctui_map_release(&m));
	ctui_map_free(&m);
}

int main(void)
{
	RUN(test_coastline);
	RUN(test_defaults);
	RUN(test_world_view);
	RUN(test_pan);
	RUN(test_keys);
	RUN(test_colors);
	RUN(test_hit_and_position);
	RUN(test_mouse);
	RUN(test_draw_world);
	RUN(test_draw_zoom_and_wrap);
	RUN(test_draw_misc);
	RUN(test_tile_categories);
	RUN(test_offline_ignores_tiles);
	RUN(test_online_tiles);
	RUN(test_tile_borders_and_missing);
	RUN(test_tile_zoom_and_wrap);
	RUN(test_tile_cache_and_garbage);
	RUN(test_marker_selection);
	RUN(test_marker_bubble_keys);
	RUN(test_marker_cells);
	RUN(test_marker_draw);
	RUN(test_marker_icons);
	RUN(test_marker_mouse);
	return CHECK_DONE();
}
