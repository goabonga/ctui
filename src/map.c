/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * map.c - a world map drawn with Braille dots (map.h).
 */
#include <ctui/map.h>

#include <curses.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "map_coast.h"
#include <ctui/text.h>

#define MIN_VIEW_ZOOM (-8.0)
#define DEFAULT_SOURCE_MAX_ZOOM 14
#define DEFAULT_CACHE_TILES 64
#define DEFAULT_CACHE_BYTES ((size_t)64 << 20U)
#define MAX_TILES 48 /* tiles drawn at once, fewer than the cache holds */

static const char *const NAMES[CTUI_MAP_CATEGORY_COUNT] = {
    "water",	"roads",     "rails",	"ferries",   "boundaries",
    "maritime", "buildings", "aeroway", "coastline", "other",
};

static const bool DEFAULT_VISIBLE[CTUI_MAP_CATEGORY_COUNT] = {
    true, true, false, false, true, false, true, true, true, true,
};

static const ctui_icon_color DEFAULT_COLORS[CTUI_MAP_CATEGORY_COUNT] = {
    CTUI_ICON_BLUE,   /* water */
    CTUI_ICON_GRAY,   /* roads */
    CTUI_ICON_PURPLE, /* rails */
    CTUI_ICON_CYAN,   /* ferries */
    CTUI_ICON_GRAY,   /* boundaries */
    CTUI_ICON_GRAY,   /* maritime */
    CTUI_ICON_GRAY,   /* buildings */
    CTUI_ICON_GRAY,   /* aeroway */
    CTUI_ICON_GREEN,  /* coastline */
    CTUI_ICON_GRAY,   /* other */
};

void ctui_map_init(ctui_map *map)
{
	*map = (ctui_map){0};
	ctui_geo_view_init(&map->view, MIN_VIEW_ZOOM,
			   CTUI_MAP_OFFLINE_MAX_ZOOM);
	memcpy(map->visible, DEFAULT_VISIBLE, sizeof map->visible);
	memcpy(map->colors, DEFAULT_COLORS, sizeof map->colors);
	map->offline_max_zoom = CTUI_MAP_OFFLINE_MAX_ZOOM;
	map->online_max_zoom = CTUI_MAP_ONLINE_MAX_ZOOM;
	map->source_max_zoom = DEFAULT_SOURCE_MAX_ZOOM;
}

void ctui_map_free(ctui_map *map)
{
	ctui_canvas_free(&map->canvas);
	ctui_tilecache_free(&map->cache);
	map->dragging = false;
}

double ctui_map_max_zoom(const ctui_map *map)
{
	return map->online && map->tiles ? map->online_max_zoom
					 : map->offline_max_zoom;
}

void ctui_map_set_online(ctui_map *map, bool online)
{
	map->online = online;
	double max = ctui_map_max_zoom(map);
	map->view.max_zoom =
	    max > map->view.min_zoom ? max : map->view.min_zoom;
	ctui_geo_view_set_zoom(&map->view, map->view.zoom);
}

bool ctui_map_set_cache(ctui_map *map, size_t tiles, size_t bytes)
{
	ctui_tilecache_free(&map->cache);
	return ctui_tilecache_init(&map->cache, tiles, bytes);
}

size_t ctui_map_cached(const ctui_map *map)
{
	return map->cache.count;
}

/* Fit the zoom limits to a width x height area, placing the first view.
 * False when the area is empty. */
static bool sync(ctui_map *map, int width, int height)
{
	if (width < 1 || height < 1)
		return false;
	double fit = ctui_geo_fit_zoom(width * 2, height * 4);
	map->view.min_zoom = fit;
	map->view.max_zoom =
	    ctui_map_max_zoom(map) > fit ? ctui_map_max_zoom(map) : fit;
	if (!map->placed) {
		map->placed = true;
		map->view.lon = map->view.lat = 0;
		map->view.zoom = fit;
	}
	ctui_geo_view_set_zoom(&map->view, map->view.zoom);
	return true;
}

void ctui_map_reset(ctui_map *map, int width, int height)
{
	if (!sync(map, width, height))
		return;
	map->view.lon = map->view.lat = 0;
	map->view.zoom = map->view.min_zoom;
}

ctui_map_event ctui_map_pan(ctui_map *map, int width, int height, double dx,
			    double dy)
{
	if (!sync(map, width, height))
		return CTUI_MAP_NONE;
	double lon = map->view.lon, lat = map->view.lat;
	ctui_geo_view_pan(&map->view, dx * 2, dy * 4);
	return fabs(map->view.lon - lon) > 1e-12 ||
		       fabs(map->view.lat - lat) > 1e-12
		   ? CTUI_MAP_MOVED
		   : CTUI_MAP_NONE;
}

bool ctui_map_hit(int width, int height, int y, int x)
{
	return x >= 0 && y >= 0 && x < width && y < height;
}

ctui_map_event ctui_map_zoom(ctui_map *map, int width, int height, int steps,
			     int y, int x)
{
	if (!sync(map, width, height) || steps == 0)
		return CTUI_MAP_NONE;
	double step = steps * CTUI_MAP_ZOOM_STEP;
	bool changed;
	if (ctui_map_hit(width, height, y, x))
		changed =
		    ctui_geo_view_zoom_at(&map->view, step, x * 2 + 1,
					  y * 4 + 2, width * 2, height * 4);
	else
		changed = ctui_geo_view_zoom_by(&map->view, step);
	return changed ? CTUI_MAP_ZOOMED : CTUI_MAP_NONE;
}

ctui_map_category ctui_map_category_for_key(int key)
{
	switch (key) {
	case 'w':
		return CTUI_MAP_WATER;
	case 'd':
		return CTUI_MAP_ROADS;
	case 'r':
		return CTUI_MAP_RAILS;
	case 'f':
		return CTUI_MAP_FERRIES;
	case 'b':
		return CTUI_MAP_BOUNDARIES;
	case 'm':
		return CTUI_MAP_MARITIME;
	case 'u':
		return CTUI_MAP_BUILDINGS;
	case 'a':
		return CTUI_MAP_AEROWAY;
	case 'c':
		return CTUI_MAP_COASTLINE;
	case 'x':
		return CTUI_MAP_OTHER;
	default:
		return CTUI_MAP_NOTHING;
	}
}

ctui_map_event ctui_map_set_visible(ctui_map *map, ctui_map_category category,
				    bool visible)
{
	if (category < 0 || category >= CTUI_MAP_CATEGORY_COUNT ||
	    map->visible[category] == visible)
		return CTUI_MAP_NONE;
	map->visible[category] = visible;
	return CTUI_MAP_TOGGLED;
}

const char *ctui_map_category_name(ctui_map_category category)
{
	return category >= 0 && category < CTUI_MAP_CATEGORY_COUNT
		   ? NAMES[category]
		   : "";
}

ctui_map_event ctui_map_key(ctui_map *map, int key, int width, int height)
{
	if (!sync(map, width, height))
		return CTUI_MAP_NONE;
	double cols = width >= 16 ? width / 8 : 2,
	       rows = height >= 16 ? height / 8 : 2;
	switch (key) {
	case KEY_LEFT:
	case 'h':
		return ctui_map_pan(map, width, height, -cols, 0);
	case KEY_RIGHT:
	case 'l':
		return ctui_map_pan(map, width, height, cols, 0);
	case KEY_UP:
	case 'k':
		return ctui_map_pan(map, width, height, 0, -rows);
	case KEY_DOWN:
	case 'j':
		return ctui_map_pan(map, width, height, 0, rows);
	case '+':
	case '=':
		return ctui_map_zoom(map, width, height, 1, -1, -1);
	case '-':
	case '_':
		return ctui_map_zoom(map, width, height, -1, -1, -1);
	case KEY_HOME:
	case '0':
		ctui_map_reset(map, width, height);
		return CTUI_MAP_RESET;
	default:
		break;
	}
	ctui_map_category category = ctui_map_category_for_key(key);
	if (category == CTUI_MAP_NOTHING)
		return CTUI_MAP_NONE;
	return ctui_map_set_visible(map, category, !map->visible[category]);
}

static bool class_is(const char *klass, const char *const *names)
{
	for (; *names; names++)
		if (klass && strcmp(klass, *names) == 0)
			return true;
	return false;
}

ctui_icon_color ctui_map_color(const ctui_map *map, ctui_map_category category,
			       const char *klass)
{
	if (category < 0 || category >= CTUI_MAP_CATEGORY_COUNT)
		return CTUI_ICON_GRAY;
	if (category == CTUI_MAP_ROADS &&
	    map->colors[CTUI_MAP_ROADS] == DEFAULT_COLORS[CTUI_MAP_ROADS]) {
		static const char *const MAJOR[] = {
		    "motorway", "motorway_construction", "trunk", NULL};
		static const char *const MEDIUM[] = {"primary", "secondary",
						     "tertiary", NULL};
		if (class_is(klass, MAJOR))
			return CTUI_ICON_ORANGE;
		if (class_is(klass, MEDIUM))
			return CTUI_ICON_YELLOW;
	}
	return map->colors[category];
}

bool ctui_map_lonlat_at(ctui_map *map, int width, int height, int y, int x,
			double *lon, double *lat)
{
	if (!ctui_map_hit(width, height, y, x) || !sync(map, width, height))
		return false;
	ctui_geo_view_from_pixel(&map->view, x * 2 + 1, y * 4 + 2, width * 2,
				 height * 4, lon, lat);
	return true;
}

bool ctui_map_press(ctui_map *map, int width, int height, int y, int x)
{
	if (!ctui_map_hit(width, height, y, x))
		return false;
	map->dragging = true;
	map->drag_y = y;
	map->drag_x = x;
	return true;
}

ctui_map_event ctui_map_drag(ctui_map *map, int width, int height, int y, int x)
{
	if (!map->dragging)
		return CTUI_MAP_NONE;
	int dy = y - map->drag_y, dx = x - map->drag_x;
	map->drag_y = y;
	map->drag_x = x;
	/* the map follows the pointer: the centre moves the other way */
	return ctui_map_pan(map, width, height, -dx, -dy);
}

bool ctui_map_release(ctui_map *map)
{
	bool was = map->dragging;
	map->dragging = false;
	return was;
}

ctui_map_event ctui_map_wheel(ctui_map *map, int width, int height, int y,
			      int x, int steps)
{
	return ctui_map_zoom(map, width, height, steps, y, x);
}

/* The embedded coastline, once for each copy of the world in view. */
static void draw_coast(ctui_map *map, int pw, int ph)
{
	double scale = ctui_geo_view_scale(&map->view), cx, cy;
	ctui_geo_project(map->view.lon, map->view.lat, &cx, &cy);
	double ox = (0 - cx) * scale + pw / 2.0,
	       oy = (0 - cy) * scale + ph / 2.0;
	long first = (long)floor(-ox / scale),
	     last = (long)ceil((pw - ox) / scale) - 1;
	if (last - first > 8)
		last = first + 8;
	ctui_icon_color color = map->colors[CTUI_MAP_COASTLINE];
	const int16_t *points = ctui_map_coast_points;
	for (size_t i = 0; i < ctui_map_coast_line_count; i++) {
		size_t n = ctui_map_coast_lengths[i];
		for (long copy = first; copy <= last; copy++) {
			double x0 = 0, y0 = 0;
			for (size_t k = 0; k < n; k++) {
				int lon = points[k * 2],
				    lat = points[k * 2 + 1];
				double wx, wy;
				ctui_geo_project(lon / 100.0, lat / 100.0, &wx,
						 &wy);
				double x1 = ox + ((double)copy + wx) * scale,
				       y1 = oy + wy * scale;
				/* at the poles (-90) the line would run along
				 * the edge of the map */
				bool pole =
				    k > 0 && abs(lat) >= 8900 &&
				    abs(points[(k - 1) * 2 + 1]) >= 8900;
				if (k > 0 && !pole)
					ctui_canvas_clipped_line(&map->canvas,
								 x0, y0, x1, y1,
								 color);
				x0 = x1;
				y0 = y1;
			}
		}
		points += n * 2;
	}
}

static const char *const SKIPPED_LAYERS[] = {
    /* area fills, which flood a canvas when drawn as outlines, and label
       anchors */
    "landcover",     "landuse",		"park", "globallandcover",
    "place",	     "water_name",	"poi",	"housenumber",
    "mountain_peak", "aerodrome_label", NULL,
};

ctui_map_category ctui_map_categorize(const ctui_mvt_layer *layer,
				      const ctui_mvt_feature *feature)
{
	const char *name = layer->name;
	for (const char *const *s = SKIPPED_LAYERS; *s; s++)
		if (strcmp(name, *s) == 0)
			return CTUI_MAP_NOTHING;
	if (strcmp(name, "water") == 0 || strcmp(name, "waterway") == 0)
		return CTUI_MAP_WATER;
	if (strcmp(name, "boundary") == 0) {
		double maritime;
		/* maritime == 1 marks sea borders (EEZ, territorial waters) */
		if (ctui_mvt_value_number(
			ctui_mvt_property(layer, feature, "maritime"),
			&maritime) &&
		    maritime == 1)
			return CTUI_MAP_MARITIME;
		return CTUI_MAP_BOUNDARIES;
	}
	if (strcmp(name, "transportation") == 0) {
		const ctui_mvt_value *klass =
		    ctui_mvt_property(layer, feature, "class");
		if (ctui_mvt_value_is(klass, "ferry"))
			return CTUI_MAP_FERRIES;
		if (ctui_mvt_value_is(klass, "rail") ||
		    ctui_mvt_value_is(klass, "transit"))
			return CTUI_MAP_RAILS;
		return CTUI_MAP_ROADS;
	}
	if (strcmp(name, "building") == 0)
		return CTUI_MAP_BUILDINGS;
	if (strcmp(name, "aeroway") == 0)
		return CTUI_MAP_AEROWAY;
	return CTUI_MAP_OTHER;
}

/* A dot at a fractional position, if it is a sensible one. */
static void set_dot(ctui_map *map, double x, double y, ctui_icon_color color)
{
	if (fabs(x) < 1e9 && fabs(y) < 1e9)
		ctui_canvas_set(&map->canvas, (int)lround(x), (int)lround(y),
				color);
}

/* The features of a tile whose north-west corner is at (x0, y0) dots and
 * whose side is `size` dots. */
static void draw_tile(ctui_map *map, const ctui_mvt_tile *tile, double x0,
		      double y0, double size)
{
	for (size_t li = 0; li < tile->layer_count; li++) {
		const ctui_mvt_layer *layer = &tile->layers[li];
		double scale = size / layer->extent;
		for (size_t fi = 0; fi < layer->feature_count; fi++) {
			const ctui_mvt_feature *f = &layer->features[fi];
			ctui_map_category category =
			    ctui_map_categorize(layer, f);
			if (category == CTUI_MAP_NOTHING ||
			    !map->visible[category])
				continue;
			const ctui_mvt_value *klass =
			    ctui_mvt_property(layer, f, "class");
			ctui_icon_color color = ctui_map_color(
			    map, category,
			    klass && klass->type == CTUI_MVT_V_STRING
				? klass->string
				: NULL);
			for (size_t pi = 0; pi < f->part_count; pi++) {
				size_t n;
				const ctui_mvt_point *p =
				    ctui_mvt_feature_part(layer, f, pi, &n);
				if (f->type == CTUI_MVT_G_POINT || n == 1) {
					for (size_t k = 0; k < n; k++)
						set_dot(
						    map, x0 + p[k].x * scale,
						    y0 + p[k].y * scale, color);
					continue;
				}
				for (size_t k = 1; k < n; k++) {
					if (f->type == CTUI_MVT_G_POLYGON &&
					    ctui_mvt_edge_segment(
						p[k - 1], p[k], layer->extent))
						continue;
					ctui_canvas_clipped_line(
					    &map->canvas,
					    x0 + p[k - 1].x * scale,
					    y0 + p[k - 1].y * scale,
					    x0 + p[k].x * scale,
					    y0 + p[k].y * scale, color);
				}
			}
		}
	}
}

/* Ask for, decode and draw the tiles in view. */
static void draw_tiles(ctui_map *map, int pw, int ph)
{
	if (!map->cache.entries &&
	    !ctui_map_set_cache(map, DEFAULT_CACHE_TILES, DEFAULT_CACHE_BYTES))
		return;
	int z = ctui_geo_tile_zoom(&map->view, map->source_max_zoom);
	ctui_geo_tile list[MAX_TILES];
	size_t n = ctui_geo_tiles(&map->view, pw, ph, z, list, MAX_TILES);
	if (n > MAX_TILES)
		n = MAX_TILES;
	for (size_t i = 0; i < n; i++) {
		const ctui_geo_tile *t = &list[i];
		const ctui_mvt_tile *tile =
		    ctui_tilecache_get(&map->cache, t->z, t->x, t->y);
		if (!tile) {
			const uint8_t *data;
			size_t len;
			if (!map->tiles(map->tiles_ctx, t->z, t->x, t->y, &data,
					&len)) {
				map->missing++;
				continue;
			}
			ctui_mvt_tile decoded;
			if (ctui_mvt_decode(data, len, &decoded) != CTUI_MVT_OK)
				decoded = (ctui_mvt_tile){0};
			tile = ctui_tilecache_put(&map->cache, t->z, t->x, t->y,
						  &decoded);
			if (!tile) {
				ctui_mvt_free(&decoded);
				continue;
			}
		}
		double x0, y0, size;
		ctui_geo_tile_rect(&map->view, t, pw, ph, &x0, &y0, &size);
		draw_tile(map, tile, x0, y0, size);
	}
}

void ctui_map_draw(ctui_map *map, int y, int x, int width, int height)
{
	if (!sync(map, width, height))
		return;
	if (map->canvas.cols != width || map->canvas.rows != height) {
		ctui_canvas_free(&map->canvas);
		if (!ctui_canvas_init(&map->canvas, width, height))
			return;
	}
	ctui_canvas_clear(&map->canvas);
	int pw = width * 2, ph = height * 4;
	map->missing = 0;
	if (map->online && map->tiles &&
	    map->view.zoom > map->offline_max_zoom + 1e-9)
		draw_tiles(map, pw, ph);
	else if (map->visible[CTUI_MAP_COASTLINE])
		draw_coast(map, pw, ph);
	ctui_style_begin(map->style);
	ctui_canvas_draw(&map->canvas, y, x);
	if (map->attribution && *map->attribution) {
		int w = (int)ctui_utf8_columns(map->attribution);
		if (w > width)
			w = width;
		ctui_put(y + height - 1, x + width - w, w, map->attribution,
			 ctui_attr(CTUI_PAIR_MUTED, false));
	}
	ctui_style_end();
}

size_t ctui_map_coast_count(void)
{
	return ctui_map_coast_line_count;
}

size_t ctui_map_coast_line(size_t i, const int16_t **points)
{
	if (i >= ctui_map_coast_line_count) {
		*points = NULL;
		return 0;
	}
	size_t first = 0;
	for (size_t k = 0; k < i; k++)
		first += ctui_map_coast_lengths[k];
	*points = &ctui_map_coast_points[first * 2];
	return ctui_map_coast_lengths[i];
}
