/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * map.c - a world map drawn with Braille dots (map.h).
 */
#include <ctui/map.h>

#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map_coast.h"
#include <ctui/frame.h>
#include <ctui/text.h>

#define MIN_VIEW_ZOOM (-8.0)
#define DEFAULT_SOURCE_MAX_ZOOM 14
#define DEFAULT_CACHE_TILES 64
#define DEFAULT_CACHE_BYTES ((size_t)64 << 20U)
#define TIMEZONE_COUNT 24 /* zones of 15 degrees */
#define TIMEZONE_DASH 2	  /* a dot every 2 dots down a meridian */
#define MAX_TILES 48	  /* tiles drawn at once, fewer than the cache holds */

static const char *const NAMES[CTUI_MAP_CATEGORY_COUNT] = {
    "water",	 "roads",   "rails",	 "ferries", "boundaries", "maritime",
    "buildings", "aeroway", "coastline", "other",   "timezones",
};

static const bool DEFAULT_VISIBLE[CTUI_MAP_CATEGORY_COUNT] = {
    true, true, false, false, true, false, true, true, true, true, false,
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
    CTUI_ICON_PURPLE, /* timezones */
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
	map->marker_color = CTUI_ICON_YELLOW;
	map->marker_selected_color = CTUI_ICON_ORANGE;
	map->marker = map->press_marker = -1;
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
	case 'z':
		return CTUI_MAP_TIMEZONES;
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
	case '\t':
		return ctui_map_select_step(map, width, height, 1);
	case KEY_BTAB:
		return ctui_map_select_step(map, width, height, -1);
	case '\n':
	case '\r':
	case KEY_ENTER:
		return ctui_map_set_bubble(map, !map->bubble);
	case 27:
		return ctui_map_set_bubble(map, false);
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

void ctui_map_set_markers(ctui_map *map, const ctui_map_marker *markers,
			  size_t count)
{
	map->markers = markers;
	map->marker_count = markers ? count : 0;
	map->marker = map->press_marker = -1;
	map->bubble = false;
}

/* The dot of marker i, relative to the area; false when it is outside. The
 * copy of the world nearest to the centre of the area is the one shown. */
static bool marker_dot(const ctui_map *map, size_t i, int width, int height,
		       int *dy, int *dx)
{
	int pw = width * 2, ph = height * 4;
	double wx, wy, px, py, scale = ctui_geo_view_scale(&map->view);
	ctui_geo_project(map->markers[i].lon,
			 ctui_geo_clamp_lat(map->markers[i].lat), &wx, &wy);
	ctui_geo_view_to_pixel(&map->view, wx, wy, pw, ph, &px, &py);
	double ddx = px - pw / 2.0;
	ddx -= scale * floor(ddx / scale + 0.5);
	px = pw / 2.0 + ddx;
	if (px < 0 || py < 0 || px >= pw || py >= ph)
		return false;
	*dx = (int)px;
	*dy = (int)py;
	return true;
}

/* The cell of marker i. */
static bool marker_cell(const ctui_map *map, size_t i, int width, int height,
			int *cy, int *cx)
{
	int dy, dx;
	if (!marker_dot(map, i, width, height, &dy, &dx))
		return false;
	*cy = dy / 4;
	*cx = dx / 2;
	return true;
}

/* A marker is a disc of MARKER_RADIUS dots (MARKER_SELECTED_RADIUS when
 * selected), like the points of a scatter plot. */
#define MARKER_RADIUS 1
#define MARKER_SELECTED_RADIUS 2

/* The text drawn for marker i, or NULL for a disc of dots. */
static const char *marker_icon(const ctui_map *map, size_t i)
{
	const ctui_map_marker *m = &map->markers[i];
	if (!m->icon)
		return NULL;
	if (ctui_theme_glyphs()->unicode)
		return m->icon;
	return m->ascii_icon ? m->ascii_icon : "*";
}

int ctui_map_marker_at(ctui_map *map, int width, int height, int y, int x)
{
	if (!ctui_map_hit(width, height, y, x) || !sync(map, width, height))
		return -1;
	int found = -1;
	for (size_t i = 0; i < map->marker_count; i++) {
		int dy, dx;
		if (!marker_dot(map, i, width, height, &dy, &dx))
			continue;
		bool hit;
		const char *icon = marker_icon(map, i);
		if (icon) { /* the cells of the icon */
			int cols = (int)ctui_utf8_columns(icon);
			hit = y == dy / 4 && x >= dx / 2 &&
			      x < dx / 2 + (cols > 0 ? cols : 1);
		} else {
			/* the dot of the cell nearest to the centre of the disc
			 */
			int r = (int)i == map->marker ? MARKER_SELECTED_RADIUS
						      : MARKER_RADIUS;
			int ny = dy < y * 4	  ? y * 4
				 : dy > y * 4 + 3 ? y * 4 + 3
						  : dy,
			    nx = dx < x * 2	  ? x * 2
				 : dx > x * 2 + 1 ? x * 2 + 1
						  : dx;
			hit = (ny - dy) * (ny - dy) + (nx - dx) * (nx - dx) <=
			      r * r;
		}
		if (hit && (found < 0 || (int)i == map->marker))
			found = (int)i;
	}
	return found;
}

ctui_map_event ctui_map_select(ctui_map *map, int width, int height, int index)
{
	if (index < 0 || (size_t)index >= map->marker_count)
		index = -1;
	if (!sync(map, width, height) || index == map->marker)
		return CTUI_MAP_NONE;
	map->marker = index;
	map->bubble = false;
	int cy, cx;
	if (index >= 0 &&
	    !marker_cell(map, (size_t)index, width, height, &cy, &cx)) {
		map->view.lon = ctui_geo_wrap_lon(map->markers[index].lon);
		map->view.lat = ctui_geo_clamp_lat(map->markers[index].lat);
		ctui_geo_view_pan(&map->view, 0, 0);
	}
	return CTUI_MAP_SELECTED;
}

ctui_map_event ctui_map_select_step(ctui_map *map, int width, int height,
				    int step)
{
	if (map->marker_count == 0)
		return CTUI_MAP_NONE;
	int count = (int)map->marker_count, next;
	if (map->marker < 0)
		next = step > 0 ? 0 : count - 1;
	else
		next = (map->marker + (step > 0 ? 1 : count - 1)) % count;
	return ctui_map_select(map, width, height, next);
}

ctui_map_event ctui_map_set_bubble(ctui_map *map, bool shown)
{
	if (map->marker < 0)
		shown = false;
	if (map->bubble == shown)
		return CTUI_MAP_NONE;
	map->bubble = shown;
	return CTUI_MAP_BUBBLE;
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
	map->drag_moved = false;
	map->drag_y = y;
	map->drag_x = x;
	map->press_marker = ctui_map_marker_at(map, width, height, y, x);
	return true;
}

ctui_map_event ctui_map_drag(ctui_map *map, int width, int height, int y, int x)
{
	if (!map->dragging)
		return CTUI_MAP_NONE;
	int dy = y - map->drag_y, dx = x - map->drag_x;
	map->drag_moved = map->drag_moved || dx != 0 || dy != 0;
	map->drag_y = y;
	map->drag_x = x;
	/* the map follows the pointer: the centre moves the other way */
	return ctui_map_pan(map, width, height, -dx, -dy);
}

bool ctui_map_release(ctui_map *map)
{
	bool was = map->dragging;
	map->dragging = false;
	if (was && !map->drag_moved) { /* a click */
		if (map->press_marker >= 0) {
			bool same = map->marker == map->press_marker;
			map->bubble = !(same && map->bubble);
			map->marker = map->press_marker;
		} else {
			map->bubble = false;
		}
	}
	map->press_marker = -1;
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

/* The copies of the world in view: copy c is shifted by c world widths. Sets
 * the pixel of the west edge of copy 0 and the width of the world. */
static void world_copies(const ctui_map *map, int pw, double *ox, double *scale,
			 long *first, long *last)
{
	double cx, cy;
	*scale = ctui_geo_view_scale(&map->view);
	ctui_geo_project(map->view.lon, map->view.lat, &cx, &cy);
	*ox = pw / 2.0 - cx * *scale;
	*first = (long)floor(-*ox / *scale);
	*last = (long)ceil((pw - *ox) / *scale) - 1;
	if (*last - *first > 8)
		*last = *first + 8;
}

/* The time zones: a dotted meridian at the dateline and between each two
 * zones (7.5 + 15 k degrees), once for each copy of the world in view. */
static void plot_timezones(ctui_map *map, int pw, int ph)
{
	double ox, scale;
	long first, last;
	world_copies(map, pw, &ox, &scale, &first, &last);
	for (long copy = first; copy <= last; copy++)
		for (int j = 0; j < TIMEZONE_COUNT; j++) {
			double lon = j == 0 ? -180.0 : -172.5 + 15.0 * (j - 1);
			double px =
			    ox + ((double)copy + (lon + 180) / 360) * scale;
			if (px < 0 || px >= pw)
				continue;
			for (int dy = 0; dy < ph; dy += TIMEZONE_DASH)
				ctui_canvas_set(
				    &map->canvas, (int)px, dy,
				    map->colors[CTUI_MAP_TIMEZONES]);
		}
}

/* The offset from UTC of each zone, at the top of the area, centred on the
 * meridian of the zone (15 k degrees) and only when the zone is wide enough
 * to hold it. */
static void draw_timezone_labels(const ctui_map *map, int y, int x, int width)
{
	double ox, scale;
	long first, last;
	world_copies(map, width * 2, &ox, &scale, &first, &last);
	int attr = ctui_attr_icon(map->colors[CTUI_MAP_TIMEZONES], false);
	double zone_cols = scale / TIMEZONE_COUNT / 2;
	for (long copy = first; copy <= last; copy++)
		for (int k = -12; k <= 12; k++) {
			char label[8];
			snprintf(label, sizeof label, k ? "%+d" : "0", k);
			int len = (int)strlen(label);
			double px =
			    ox +
			    ((double)copy + (15.0 * k + 180) / 360) * scale;
			if (zone_cols < len + 1)
				continue;
			int col = (int)(px / 2) - len / 2;
			if (col < 0 || col + len > width)
				continue;
			ctui_put(y, x + col, len, label, attr);
		}
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

#define BUBBLE_LINES 8
#define BUBBLE_CHARS 96

/* The info bubble of the selected marker: a frame titled with the marker,
 * above its star when there is room, else below. */
static void draw_bubble(const ctui_map *map, int y, int x, int width,
			int height, int cy, int cx)
{
	const ctui_map_marker *m = &map->markers[map->marker];
	char lines[BUBBLE_LINES][BUBBLE_CHARS];
	int count = 0;
	size_t inner = m->title ? ctui_utf8_columns(m->title) + 2 : 0;
	for (const char *p = m->text; p && *p && count < BUBBLE_LINES;) {
		const char *end = strchr(p, '\n');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		snprintf(lines[count], BUBBLE_CHARS, "%.*s", (int)len, p);
		size_t cols = ctui_utf8_columns(lines[count]);
		inner = cols > inner ? cols : inner;
		count++;
		p = end ? end + 1 : p + len;
	}
	int w = (int)inner + 4, h = (count ? count : 1) + 2;
	if (w > width)
		w = width;
	if (w < 4 || h > height)
		return;
	int bx = cx - w / 2, by = cy - h >= 0 ? cy - h : cy + 1;
	bx = bx > width - w ? width - w : bx < 0 ? 0 : bx;
	by = by > height - h ? height - h : by < 0 ? 0 : by;
	ctui_frame_draw(y + by, x + bx, h, w, true, true, m->title);
	for (int i = 0; i < count; i++)
		ctui_put(y + by + 1 + i, x + bx + 2, w - 4, lines[i],
			 ctui_attr(CTUI_PAIR_DIALOG, false));
}

/* The discs of the markers on the canvas, the selected one on top. */
static void plot_markers(ctui_map *map, int width, int height)
{
	for (size_t pass = 0; pass < 2; pass++)
		for (size_t i = 0; i < map->marker_count; i++) {
			bool selected = (int)i == map->marker;
			int dy, dx;
			if (selected != (pass == 1) || marker_icon(map, i) ||
			    !marker_dot(map, i, width, height, &dy, &dx))
				continue;
			ctui_canvas_circle(&map->canvas, dx, dy,
					   selected ? MARKER_SELECTED_RADIUS
						    : MARKER_RADIUS,
					   selected ? map->marker_selected_color
						    : map->marker_color,
					   true);
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
	if (map->visible[CTUI_MAP_TIMEZONES])
		plot_timezones(map, pw, ph);
	plot_markers(map, width, height);
	ctui_style_begin(map->style);
	ctui_canvas_draw(&map->canvas, y, x);
	if (map->visible[CTUI_MAP_TIMEZONES])
		draw_timezone_labels(map, y, x, width);
	if (map->attribution && *map->attribution) {
		int w = (int)ctui_utf8_columns(map->attribution);
		if (w > width)
			w = width;
		ctui_put(y + height - 1, x + width - w, w, map->attribution,
			 ctui_attr(CTUI_PAIR_MUTED, false));
	}
	for (size_t pass = 0; pass < 2; pass++)
		for (size_t i = 0; i < map->marker_count; i++) {
			bool selected = (int)i == map->marker;
			const char *icon = marker_icon(map, i);
			int cy, cx;
			if (!icon || selected != (pass == 1) ||
			    !marker_cell(map, i, width, height, &cy, &cx))
				continue;
			int attr =
			    ctui_attr_icon(selected ? map->marker_selected_color
						    : map->marker_color,
					   false);
			ctui_put(y + cy, x + cx, width - cx, icon,
				 selected
				     ? (int)((unsigned)attr | (unsigned)A_BOLD)
				     : attr);
		}
	int sy, sx;
	if (map->bubble && map->marker >= 0 &&
	    marker_cell(map, (size_t)map->marker, width, height, &sy, &sx))
		draw_bubble(map, y, x, width, height, sy, sx);
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
