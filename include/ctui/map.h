/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * map.h - a world map drawn with Braille dots.
 *
 * The map shows the whole world offline, from a coastline embedded in the
 * library (Natural Earth 1:110m, public domain,
 * https://www.naturalearthdata.com/, simplified to 0.01 degree), at the
 * smallest zoom that fits the area. The zoom is capped at
 * CTUI_MAP_OFFLINE_MAX_ZOOM, where that coarse coastline still looks right.
 *
 * Online mode lifts the cap: with `online` set and a tile callback given,
 * zooms above the cap draw vector tiles (MVT, see mvt.h) instead of the
 * coastline, up to CTUI_MAP_ONLINE_MAX_ZOOM. The library has no network, no
 * SQLite and no zlib: the callback hands over raw, already decompressed MVT
 * bytes for a tile (z, x, y), or says the tile is not available (yet).
 * Decoded tiles are kept in a bounded LRU cache. Past source_max_zoom the
 * tiles of that zoom are magnified. The caller must credit the data it feeds
 * in (the OpenStreetMap contributors, for instance): `attribution` is drawn
 * in the bottom right corner.
 *
 * The time zones can be drawn over either source: dotted meridians every 15
 * degrees, half way between the zone centres (the nominal zones, which
 * political borders only follow roughly), each zone labelled with its offset
 * from UTC at the top of the area when it is wide enough.
 *
 * Features are drawn as outlines in categories (water, roads, ...) that
 * can be shown or hidden and recoloured with an ctui_icon_color each. The
 * keys pan (arrows, h j k l) and zoom (+ and -), letters toggle the
 * categories, the mouse drags to pan and the wheel zooms where it points.
 * The viewport wraps around the antimeridian, stops at the poles, and
 * follows the size of the area, so a resize needs nothing.
 *
 * Markers are places given by the caller, drawn as discs of dots on the
 * canvas like the points of a scatter plot (see linechart.h), or as an icon
 * of the caller's choice, any text. Tab and Shift-Tab
 * move the selection from one marker to the next, Enter shows the info bubble
 * of the selected marker (its title and its text) and Escape hides it; a click
 * on a marker selects it and shows its bubble, a click elsewhere hides the
 * bubble.
 *
 * Coordinates of the event functions are relative to the top-left cell of
 * the area the map is drawn in, which is width x height cells; the map is
 * 2 * width by 4 * height dots.
 */
#ifndef CTUI_MAP_H
#define CTUI_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ctui/canvas.h>
#include <ctui/geo.h>
#include <ctui/mvt.h>
#include <ctui/theme.h>
#include <ctui/tilecache.h>

#define CTUI_MAP_OFFLINE_MAX_ZOOM 4.0
#define CTUI_MAP_ONLINE_MAX_ZOOM 18.0
#define CTUI_MAP_ZOOM_STEP 0.5

typedef enum ctui_map_category {
	CTUI_MAP_NOTHING = -1, /* never drawn */
	CTUI_MAP_WATER = 0,
	CTUI_MAP_ROADS,
	CTUI_MAP_RAILS,
	CTUI_MAP_FERRIES,
	CTUI_MAP_BOUNDARIES,
	CTUI_MAP_MARITIME,
	CTUI_MAP_BUILDINGS,
	CTUI_MAP_AEROWAY,
	CTUI_MAP_COASTLINE, /* the embedded coastline */
	CTUI_MAP_OTHER,
	CTUI_MAP_TIMEZONES, /* the lines between the 24 time zones */
	CTUI_MAP_CATEGORY_COUNT
} ctui_map_category;

/* The raw bytes of tile (z, x, y): true with *data and *len set (valid until
 * the next call) when the tile is available, false when it is not, or not
 * yet. The map asks again at the next draw for a tile that was missing, so
 * the callback should answer quickly and may fetch in the background. A
 * tile that cannot be decoded is cached as empty and not asked for again. */
typedef bool (*ctui_map_tile_fn)(void *ctx, int z, int x, int y,
				 const uint8_t **data, size_t *len);

/* A place on the map, drawn as a disc of dots or as an icon of your choice
 * (a star, a pin, a letter...), in marker_color, or marker_selected_color
 * when selected. The strings belong to the caller and must outlive the
 * map or the next ctui_map_set_markers(). */
typedef struct ctui_map_marker {
	double lon, lat;
	const char *title; /* top edge of the bubble, may be NULL */
	const char *text;  /* lines of the bubble, separated by '\n', may be
			      NULL */
	const char *icon;  /* drawn instead of the disc of dots (any text, one
			      or a few columns), NULL for the disc */
	const char *ascii_icon; /* the same without UTF-8; "*" when NULL */
} ctui_map_marker;

typedef struct ctui_map {
	ctui_geo_view view; /* kept inside the zoom limits at every call */
	bool visible[CTUI_MAP_CATEGORY_COUNT];
	ctui_icon_color colors[CTUI_MAP_CATEGORY_COUNT];
	bool online;		/* use the tile callback, lift the zoom cap */
	ctui_map_tile_fn tiles; /* may be NULL */
	void *tiles_ctx;
	int source_max_zoom;	 /* deepest zoom the source has (14); deeper is
				    magnified */
	double offline_max_zoom; /* CTUI_MAP_OFFLINE_MAX_ZOOM */
	double online_max_zoom;	 /* CTUI_MAP_ONLINE_MAX_ZOOM */
	const char *attribution; /* drawn bottom right, may be NULL */
	const ctui_style *
	    style; /* colours of the attribution, may be NULL; set after init */
	size_t missing; /* tiles wanted but not available at the last draw */
	const ctui_map_marker *markers; /* see ctui_map_set_markers() */
	size_t marker_count;
	ctui_icon_color marker_color;	       /* yellow */
	ctui_icon_color marker_selected_color; /* orange, drawn larger */
	int marker;  /* the selected marker, -1 for none */
	bool bubble; /* the bubble of the selected marker is shown */
	/* private */
	bool placed;
	bool dragging;
	bool drag_moved;
	int press_marker; /* marker under the press, -1 for none */
	int drag_y, drag_x;
	ctui_canvas canvas;
	ctui_tilecache cache;
} ctui_map;

typedef enum ctui_map_event {
	CTUI_MAP_NONE = 0,
	CTUI_MAP_MOVED,	   /* the centre changed */
	CTUI_MAP_ZOOMED,   /* the zoom changed */
	CTUI_MAP_TOGGLED,  /* a category was shown or hidden */
	CTUI_MAP_RESET,	   /* back to the whole world */
	CTUI_MAP_SELECTED, /* another marker is selected, or none */
	CTUI_MAP_BUBBLE	   /* the bubble was shown or hidden */
} ctui_map_event;

/* The whole world, default categories and colours. The area size is only
 * known when drawing or handling an event, which places the first view. */
void ctui_map_init(ctui_map *map);
void ctui_map_free(ctui_map *map);

/* Allow or forbid the tile callback; turning it off brings the zoom back
 * under the offline cap. */
void ctui_map_set_online(ctui_map *map, bool online);

/* Size the tile cache: at most `tiles` tiles and about `bytes` bytes of
 * decoded data (0 = no byte limit). Drops what is cached. Defaults: 64
 * tiles, 64 MiB. */
bool ctui_map_set_cache(ctui_map *map, size_t tiles, size_t bytes);

/* Tiles held in the cache. */
size_t ctui_map_cached(const ctui_map *map);

/* The highest zoom now allowed: the online maximum when online with a
 * callback, else the offline one. */
double ctui_map_max_zoom(const ctui_map *map);

/* Show the whole world in a width x height area, centred. */
void ctui_map_reset(ctui_map *map, int width, int height);

/* Move by `dx` columns and `dy` rows (east and south for positive values). */
ctui_map_event ctui_map_pan(ctui_map *map, int width, int height, double dx,
			    double dy);

/* Zoom by `steps` steps of CTUI_MAP_ZOOM_STEP (negative = out) around the cell
 * (y, x), or around the centre when it is outside the area. */
ctui_map_event ctui_map_zoom(ctui_map *map, int width, int height, int steps,
			     int y, int x);

/* Arrows / h j k l pan, + = - _ zoom, Home and 0 show the world again, Tab
 * and Shift-Tab select the next and the previous marker, Enter shows or
 * hides its bubble, Escape hides it, and the key of a category toggles it
 * (see ctui_map_category_for_key()). */
ctui_map_event ctui_map_key(ctui_map *map, int key, int width, int height);

/* The category toggled by a key (w water, d roads, r rails, f ferries,
 * b boundaries, m maritime, u buildings, a aeroway, c coastline, x other,
 * z time zones),
 * or CTUI_MAP_NOTHING. */
ctui_map_category ctui_map_category_for_key(int key);

/* Show or hide a category; CTUI_MAP_TOGGLED when it changed. */
ctui_map_event ctui_map_set_visible(ctui_map *map, ctui_map_category category,
				    bool visible);

/* English name of a category. */
const char *ctui_map_category_name(ctui_map_category category);

/* The colour of a category, with the road classes (motorway, trunk,
 * primary, ...) in warmer colours while roads still have their default
 * colour. `klass` is the "class" property of a feature, may be NULL. */
ctui_icon_color ctui_map_color(const ctui_map *map, ctui_map_category category,
			       const char *klass);

/* Show `count` markers (the array is not copied; NULL or 0 for none). The
 * selection is cleared and the bubble hidden. */
void ctui_map_set_markers(ctui_map *map, const ctui_map_marker *markers,
			  size_t count);

/* Select marker `index` (-1 for none), bring it into view when it is outside
 * and hide the bubble. CTUI_MAP_SELECTED when the selection changed. */
ctui_map_event ctui_map_select(ctui_map *map, int width, int height, int index);

/* Select the next marker (`step` > 0) or the previous one, wrapping round;
 * the first (or the last) when none is selected. */
ctui_map_event ctui_map_select_step(ctui_map *map, int width, int height,
				    int step);

/* Show or hide the bubble of the selected marker; it does nothing without a
 * selection. CTUI_MAP_BUBBLE when it changed. */
ctui_map_event ctui_map_set_bubble(ctui_map *map, bool shown);

/* The marker whose disc touches cell (y, x), or -1 (the selected one wins
 * when discs overlap). */
int ctui_map_marker_at(ctui_map *map, int width, int height, int y, int x);

/* True when the cell (y, x) is inside a width x height map. */
bool ctui_map_hit(int width, int height, int y, int x);

/* The position under the centre of cell (y, x); false outside the area. */
bool ctui_map_lonlat_at(ctui_map *map, int width, int height, int y, int x,
			double *lon, double *lat);

/* Left press inside the area starts a drag. A press that is released
 * without moving is a click: on a marker it selects it and shows its bubble,
 * elsewhere it hides the bubble. */
bool ctui_map_press(ctui_map *map, int width, int height, int y, int x);

/* Pointer moved (the point may be outside the area) / released after a
 * press: the map follows the pointer; true when a press ended (the markers
 * and the bubble may have changed, see map->marker and map->bubble). */
ctui_map_event ctui_map_drag(ctui_map *map, int width, int height, int y,
			     int x);
bool ctui_map_release(ctui_map *map);

/* Wheel: positive steps zoom in around the pointer. */
ctui_map_event ctui_map_wheel(ctui_map *map, int width, int height, int y,
			      int x, int steps);

/* The category of a tile feature (by the name of its layer and its
 * properties), or CTUI_MAP_NOTHING for layers that are not drawn: area fills
 * that would flood the dots and label anchors. */
ctui_map_category ctui_map_categorize(const ctui_mvt_layer *layer,
				      const ctui_mvt_feature *feature);

/* Draw the map in a width x height area. */
void ctui_map_draw(ctui_map *map, int y, int x, int width, int height);

/* The embedded coastline: its polylines, and the points of polyline i as
 * pairs of longitude, latitude in hundredths of a degree (the number of
 * points is returned; 0 when i is out of range). */
size_t ctui_map_coast_count(void);
size_t ctui_map_coast_line(size_t i, const int16_t **points);

#endif /* CTUI_MAP_H */
