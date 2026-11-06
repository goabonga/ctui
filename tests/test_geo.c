/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_geo.c - Mercator maths, the viewport and tile selection.
 */
#include "check.h"

#include <math.h>

#include <ctui/geo.h>

static void test_project(void)
{
	double x, y, lon, lat;
	ctui_geo_project(0, 0, &x, &y);
	CHECK_NEAR(x, 0.5, 1e-12);
	CHECK_NEAR(y, 0.5, 1e-12);
	ctui_geo_project(-180, CTUI_GEO_MAX_LAT, &x, &y);
	CHECK_NEAR(x, 0, 1e-12);
	CHECK_NEAR(y, 0, 1e-9);
	ctui_geo_project(180, -CTUI_GEO_MAX_LAT, &x, &y);
	CHECK_NEAR(x, 1, 1e-12);
	CHECK_NEAR(y, 1, 1e-9);
	/* beyond the poles the latitude is clamped */
	ctui_geo_project(0, 90, &x, &y);
	CHECK_NEAR(y, 0, 1e-9);
	/* Paris */
	ctui_geo_project(2.3522, 48.8566, &x, &y);
	ctui_geo_unproject(x, y, &lon, &lat);
	CHECK_NEAR(lon, 2.3522, 1e-9);
	CHECK_NEAR(lat, 48.8566, 1e-9);
	CHECK_NEAR(ctui_geo_clamp_lat(100), CTUI_GEO_MAX_LAT, 1e-12);
	CHECK_NEAR(ctui_geo_clamp_lat(-100), -CTUI_GEO_MAX_LAT, 1e-12);
	CHECK_NEAR(ctui_geo_clamp_lat(NAN), 0, 0);
}

static void test_wrap(void)
{
	CHECK_NEAR(ctui_geo_wrap_lon(0), 0, 1e-12);
	CHECK_NEAR(ctui_geo_wrap_lon(180), -180, 1e-12);
	CHECK_NEAR(ctui_geo_wrap_lon(190), -170, 1e-12);
	CHECK_NEAR(ctui_geo_wrap_lon(-190), 170, 1e-12);
	CHECK_NEAR(ctui_geo_wrap_lon(720 + 10), 10, 1e-9);
	CHECK_NEAR(ctui_geo_wrap_lon(INFINITY), 0, 0);
}

static void test_tile_coords(void)
{
	double x, y, lon, lat;
	ctui_geo_to_tile(0, 0, 1, &x, &y);
	CHECK_NEAR(x, 1, 1e-12);
	CHECK_NEAR(y, 1, 1e-12);
	ctui_geo_to_tile(-180, 85, 3, &x, &y);
	CHECK_NEAR(x, 0, 1e-12);
	CHECK(y > 0 && y < 0.1);
	ctui_geo_to_tile(2.3522, 48.8566, 10, &x,
			 &y); /* the tile of Paris at zoom 10 */
	CHECK((int)x == 518 && (int)y == 352);
	ctui_geo_from_tile(x, y, 10, &lon, &lat);
	CHECK_NEAR(lon, 2.3522, 1e-9);
	CHECK_NEAR(lat, 48.8566, 1e-9);
	ctui_geo_to_tile(0, 0, -4, &x, &y); /* negative zoom reads as 0 */
	CHECK_NEAR(x, 0.5, 1e-12);
}

static void test_view_zoom(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, -1, 4);
	CHECK_NEAR(v.zoom, -1, 0);
	CHECK_NEAR(ctui_geo_view_scale(&v), 128, 1e-9);
	CHECK(ctui_geo_view_zoom_by(&v, 0.5) && v.zoom == -0.5);
	CHECK(ctui_geo_view_zoom_by(&v, 10) && v.zoom == 4);
	CHECK(!ctui_geo_view_zoom_by(&v, 1)); /* already at the top */
	CHECK(ctui_geo_view_zoom_by(&v, -100) && v.zoom == -1);
	CHECK(!ctui_geo_view_set_zoom(&v, NAN));
	ctui_geo_view_init(&v, 3, 1); /* max raised to min */
	CHECK(v.max_zoom == 3);
	CHECK_NEAR(ctui_geo_fit_zoom(512, 1024), 1, 1e-12);
	CHECK_NEAR(ctui_geo_fit_zoom(1024, 128), -1, 1e-12);
	CHECK(ctui_geo_fit_zoom(0, 0) < 0); /* never NaN */
}

static void test_pixels(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, 0, 10);
	v.zoom = 1; /* the world is 512 pixels */
	double px, py, lon, lat;
	ctui_geo_view_to_pixel(&v, 0.5, 0.5, 100, 60, &px, &py);
	CHECK_NEAR(px, 50, 1e-9);
	CHECK_NEAR(py, 30, 1e-9);
	ctui_geo_view_to_pixel(&v, 0.75, 0.5, 100, 60, &px, &py);
	CHECK_NEAR(px, 50 + 128, 1e-9);
	ctui_geo_view_from_pixel(&v, 50 + 128, 30, 100, 60, &lon, &lat);
	CHECK_NEAR(lon, 90, 1e-9);
	CHECK_NEAR(lat, 0, 1e-9);
	/* past the pole the position stays on the map */
	ctui_geo_view_from_pixel(&v, 50, -100000, 100, 60, &lon, &lat);
	CHECK_NEAR(lat, CTUI_GEO_MAX_LAT, 1e-6);
}

static void test_pan(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, 0, 10);
	v.zoom = 1;
	ctui_geo_view_pan(&v, 128, 0); /* a quarter of the world east */
	CHECK_NEAR(v.lon, 90, 1e-9);
	CHECK_NEAR(v.lat, 0, 1e-9);
	ctui_geo_view_pan(&v, 256, 0); /* around the antimeridian */
	CHECK_NEAR(v.lon, -90, 1e-9);
	ctui_geo_view_pan(&v, -128 * 4, 0);
	CHECK_NEAR(v.lon, -90, 1e-9);
	ctui_geo_view_pan(&v, 0, 100000); /* stops at the south pole */
	CHECK_NEAR(v.lat, -CTUI_GEO_MAX_LAT, 1e-6);
	ctui_geo_view_pan(&v, 0, -1e9);
	CHECK_NEAR(v.lat, CTUI_GEO_MAX_LAT, 1e-6);
	CHECK(v.lon >= -180 && v.lon < 180);
}

static void test_zoom_at(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, 0, 10);
	v.zoom = 2;
	v.lon = 10;
	v.lat = 20;
	double lon0, lat0, lon1, lat1;
	ctui_geo_view_from_pixel(&v, 70, 20, 100, 60, &lon0, &lat0);
	CHECK(ctui_geo_view_zoom_at(&v, 1, 70, 20, 100, 60));
	CHECK_NEAR(v.zoom, 3, 0);
	ctui_geo_view_from_pixel(&v, 70, 20, 100, 60, &lon1, &lat1);
	CHECK_NEAR(lon1, lon0, 1e-6);
	CHECK_NEAR(lat1, lat0, 1e-6);
	/* across the antimeridian */
	v.lon = 179;
	ctui_geo_view_from_pixel(&v, 90, 30, 100, 60, &lon0, &lat0);
	CHECK(ctui_geo_view_zoom_at(&v, -1, 90, 30, 100, 60));
	ctui_geo_view_from_pixel(&v, 90, 30, 100, 60, &lon1, &lat1);
	CHECK_NEAR(lon1, lon0, 1e-6);
	/* at the limit nothing moves */
	ctui_geo_view_set_zoom(&v, 10);
	double before = v.lon;
	CHECK(!ctui_geo_view_zoom_at(&v, 1, 90, 30, 100, 60) &&
	      v.lon == before);
}

static void test_tile_zoom(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, -3, 19);
	v.zoom = 4.4;
	CHECK(ctui_geo_tile_zoom(&v, 14) == 4);
	v.zoom = 4.5;
	CHECK(ctui_geo_tile_zoom(&v, 14) == 5);
	v.zoom = 17;
	CHECK(ctui_geo_tile_zoom(&v, 14) == 14); /* overzoom */
	v.zoom = -2;
	CHECK(ctui_geo_tile_zoom(&v, 14) == 0);
	CHECK(ctui_geo_tile_zoom(&v, -5) == 0);
}

static void test_tiles(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, 0, 19);
	ctui_geo_tile t[64];
	/* the whole world at zoom 0: one tile in view, 256 pixels */
	v.zoom = 0;
	CHECK(ctui_geo_tiles(&v, 256, 256, 0, t, 64) == 1);
	CHECK(t[0].z == 0 && t[0].x == 0 && t[0].y == 0 && t[0].ux == 0);
	/* wider than the world: copies of the tile, wrapped onto column 0 */
	size_t n = ctui_geo_tiles(&v, 600, 100, 0, t, 64);
	CHECK(n == 3);
	CHECK(t[0].ux == 0 && t[0].x == 0);
	bool left = false, right = false;
	for (size_t i = 0; i < n; i++) {
		CHECK(t[i].x == 0);
		left = left || t[i].ux == -1;
		right = right || t[i].ux == 1;
	}
	CHECK(left && right);
	/* zoom 2 centred on the origin: 4 x 4 tiles, a 256 box shows 2 x 2 +
	 * borders */
	v.zoom = 2;
	n = ctui_geo_tiles(&v, 256, 256, 2, t, 64);
	CHECK(n == 4); /* the centre is the corner of four tiles */
	for (size_t i = 0; i < n; i++)
		CHECK(t[i].x >= 1 && t[i].x <= 2 && t[i].y >= 1 && t[i].y <= 2);
	/* near the west edge the columns wrap */
	v.lon = -179;
	v.lat = 0;
	n = ctui_geo_tiles(&v, 512, 256, 2, t, 64);
	bool wrapped = false;
	for (size_t i = 0; i < n; i++) {
		CHECK(t[i].x >= 0 && t[i].x < 4);
		wrapped = wrapped || (t[i].ux < 0 && t[i].x == 3);
	}
	CHECK(wrapped);
	/* near the north pole the rows stop at the map */
	v.lon = 0;
	v.lat = 85;
	n = ctui_geo_tiles(&v, 256, 1024, 3, t, 64);
	for (size_t i = 0; i < n; i++)
		CHECK(t[i].y >= 0 && t[i].y < 8);
	/* bad arguments */
	CHECK(ctui_geo_tiles(&v, 0, 10, 3, t, 64) == 0);
	CHECK(ctui_geo_tiles(&v, 10, 10, -1, t, 64) == 0);
}

static void test_tiles_order(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, 0, 19);
	v.zoom = 4;
	v.lon = 12.34;
	v.lat = 41.9;
	ctui_geo_tile all[64], few[3];
	size_t n = ctui_geo_tiles(&v, 700, 400, 4, all, 64);
	CHECK(n > 3 && n <= 64);
	double cx, cy;
	ctui_geo_project(v.lon, v.lat, &cx, &cy);
	double last = -1;
	for (size_t i = 0; i < n; i++) {
		double dx = (all[i].ux + 0.5) / 16 - cx,
		       dy = (all[i].y + 0.5) / 16 - cy;
		double d = dx * dx + dy * dy;
		CHECK(d >= last);
		last = d;
	}
	/* a short buffer keeps the nearest ones and reports the full count */
	CHECK(ctui_geo_tiles(&v, 700, 400, 4, few, 3) == n);
	for (int i = 0; i < 3; i++)
		CHECK(few[i].ux == all[i].ux && few[i].y == all[i].y);
	CHECK(ctui_geo_tiles(&v, 700, 400, 4, NULL, 0) == n);
}

static void test_tile_rect(void)
{
	ctui_geo_view v;
	ctui_geo_view_init(&v, 0, 19);
	v.zoom = 1; /* 512 pixels for the world */
	ctui_geo_tile t = {1, 1, 0, 1};
	double x, y, size;
	ctui_geo_tile_rect(&v, &t, 100, 60, &x, &y, &size);
	CHECK_NEAR(size, 256, 1e-9);
	CHECK_NEAR(x, 50, 1e-9); /* the east half starts at the centre */
	CHECK_NEAR(y, 30 - 256, 1e-9);
	t.z = 3; /* overzoom factor: tile size follows the zoom */
	v.zoom = 4;
	ctui_geo_tile_rect(&v, &t, 100, 60, &x, &y, &size);
	CHECK_NEAR(size, 512, 1e-9);
}

int main(void)
{
	RUN(test_project);
	RUN(test_wrap);
	RUN(test_tile_coords);
	RUN(test_view_zoom);
	RUN(test_pixels);
	RUN(test_pan);
	RUN(test_zoom_at);
	RUN(test_tile_zoom);
	RUN(test_tiles);
	RUN(test_tiles_order);
	RUN(test_tile_rect);
	return CHECK_DONE();
}
