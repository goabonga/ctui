/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * geo.h - Web Mercator maths, a pan / zoom viewport and tile selection.
 *
 * Positions are WGS84 degrees (longitude east, latitude north). The
 * projection is the square slippy-map one (EPSG:3857): "world"
 * coordinates run over [0, 1) from west to east and from north (y = 0) to
 * south, the whole world is one 256-pixel tile at zoom 0, and every zoom
 * level doubles the scale. Latitudes are limited to +-85.0511 degrees.
 *
 * A viewport is a centre and a fractional zoom. It maps the world on a
 * grid of pixels (dots, for a canvas) whose size the caller passes to each
 * function, so a resize needs no state. Panning wraps over the antimeridian
 * and stops at the poles; the zoom stays in [min_zoom, max_zoom]. Tiles of
 * the XYZ scheme are selected for the pixels a viewport shows, nearest the
 * centre first, with the column wrapped into the world.
 *
 * Everything here is pure: no screen, no allocation.
 */
#ifndef CTUI_GEO_H
#define CTUI_GEO_H

#include <stdbool.h>
#include <stddef.h>

#define CTUI_GEO_TILE_SIZE 256
#define CTUI_GEO_MAX_LAT 85.0511287798066

/* Latitude limited to the projectable range; longitude brought to
 * [-180, 180). */
double ctui_geo_clamp_lat(double lat);
double ctui_geo_wrap_lon(double lon);

/* World coordinates of a position (longitude is not wrapped) and back. */
void ctui_geo_project(double lon, double lat, double *x, double *y);
void ctui_geo_unproject(double x, double y, double *lon, double *lat);

/* Fractional tile coordinates at an integer zoom >= 0 (the integer parts
 * name the tile, the fractions the place inside it) and back. A negative
 * zoom is taken as 0. */
void ctui_geo_to_tile(double lon, double lat, int zoom, double *x, double *y);
void ctui_geo_from_tile(double x, double y, int zoom, double *lon, double *lat);

typedef struct ctui_geo_view {
	double lon, lat; /* centre */
	double zoom;	 /* pixels per world unit: 256 * 2^zoom */
	double min_zoom, max_zoom;
} ctui_geo_view;

/* Centred on (0, 0) at min_zoom. max_zoom is raised to min_zoom if lower. */
void ctui_geo_view_init(ctui_geo_view *v, double min_zoom, double max_zoom);

/* The zoom at which the whole world fits a pw x ph pixel box (the smaller
 * side shows the 256-pixel world). */
double ctui_geo_fit_zoom(int pw, int ph);

/* Set the zoom, limited to [min_zoom, max_zoom]; true when it changed. */
bool ctui_geo_view_set_zoom(ctui_geo_view *v, double zoom);

/* Pixels per world unit. */
double ctui_geo_view_scale(const ctui_geo_view *v);

/* World coordinates to pixels of a pw x ph box, and pixels back to a
 * position. */
void ctui_geo_view_to_pixel(const ctui_geo_view *v, double wx, double wy,
			    int pw, int ph, double *px, double *py);
void ctui_geo_view_from_pixel(const ctui_geo_view *v, double px, double py,
			      int pw, int ph, double *lon, double *lat);

/* Move the centre by (dx, dy) pixels (east and south for positive values):
 * longitude wraps, latitude stops at the poles. */
void ctui_geo_view_pan(ctui_geo_view *v, double dx, double dy);

/* Change the zoom by `step`; true when it changed. */
bool ctui_geo_view_zoom_by(ctui_geo_view *v, double step);

/* Same, keeping the place under pixel (px, py) in place. */
bool ctui_geo_view_zoom_at(ctui_geo_view *v, double step, double px, double py,
			   int pw, int ph);

/* The tile zoom drawn for the view: the zoom rounded to an integer in
 * [0, source_max]. Past source_max the tiles of source_max are magnified
 * (overzoom). */
int ctui_geo_tile_zoom(const ctui_geo_view *v, int source_max);

typedef struct ctui_geo_tile {
	int z;
	int x;	/* column in [0, 2^z): the tile to fetch */
	int y;	/* row in [0, 2^z) */
	int ux; /* column before wrapping: where this copy of the world is drawn
		 */
} ctui_geo_tile;

/* The tiles of zoom z that touch a pw x ph box, nearest the centre first.
 * Fills out (at most cap entries) and returns the number found, which may
 * exceed cap; the first cap are then still the nearest. */
size_t ctui_geo_tiles(const ctui_geo_view *v, int pw, int ph, int z,
		      ctui_geo_tile *out, size_t cap);

/* Pixel position of the north-west corner of a tile and its size in
 * pixels (a point at u / extent across the tile is at x + size * u /
 * extent). */
void ctui_geo_tile_rect(const ctui_geo_view *v, const ctui_geo_tile *t, int pw,
			int ph, double *x, double *y, double *size);

#endif /* CTUI_GEO_H */
