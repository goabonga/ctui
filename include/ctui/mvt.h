/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * mvt.h - a decoder for Mapbox Vector Tiles (MVT 2.1).
 *
 * Reads the protobuf encoding of the vector_tile schema without any
 * library: layers with their extent, a key / value table, and features
 * with their tags and geometry. The input is raw, already decompressed
 * bytes; a tile cut short or corrupt is refused as a whole, never read past
 * its end. Geometry is decoded to integer points of the tile grid (0 ..
 * extent, y growing south): a polygon ring or a line is a part, a closed
 * ring repeats its first point.
 *
 * The decoded tile owns its memory (ctui_mvt_free()). A layer keeps the
 * points, parts and tags of all its features in three flat arrays that the
 * features index into.
 */
#ifndef CTUI_MVT_H
#define CTUI_MVT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum ctui_mvt_status {
	CTUI_MVT_OK = 0,
	CTUI_MVT_ERR_FORMAT, /* not a valid vector tile */
	CTUI_MVT_ERR_MEMORY
} ctui_mvt_status;

typedef enum ctui_mvt_geom {
	CTUI_MVT_G_UNKNOWN = 0,
	CTUI_MVT_G_POINT = 1,
	CTUI_MVT_G_LINESTRING = 2,
	CTUI_MVT_G_POLYGON = 3
} ctui_mvt_geom;

typedef enum ctui_mvt_value_type {
	CTUI_MVT_V_NONE = 0,
	CTUI_MVT_V_STRING,
	CTUI_MVT_V_DOUBLE, /* float and double */
	CTUI_MVT_V_INT, /* int, uint and sint (a uint past INT64_MAX saturates)
			 */
	CTUI_MVT_V_BOOL
} ctui_mvt_value_type;

typedef struct ctui_mvt_value {
	ctui_mvt_value_type type;
	char *string;
	double number;	 /* CTUI_MVT_V_DOUBLE */
	int64_t integer; /* CTUI_MVT_V_INT, and 0 / 1 for a bool */
} ctui_mvt_value;

typedef struct ctui_mvt_point {
	int32_t x, y;
} ctui_mvt_point;

typedef struct ctui_mvt_part {
	size_t first, count; /* into the points of the layer */
} ctui_mvt_part;

typedef struct ctui_mvt_feature {
	uint64_t id;
	ctui_mvt_geom type;
	size_t tag_first, tag_count;   /* into tags: (key, value) index pairs,
					  tag_count of them */
	size_t part_first, part_count; /* into the parts of the layer */
} ctui_mvt_feature;

typedef struct ctui_mvt_layer {
	char *name;
	uint32_t extent; /* 4096 when the tile does not say */
	char **keys;
	size_t key_count;
	ctui_mvt_value *values;
	size_t value_count;
	ctui_mvt_feature *features;
	size_t feature_count;
	uint32_t *tags; /* 2 * tag_count entries */
	size_t tag_count;
	ctui_mvt_part *parts;
	size_t part_count;
	ctui_mvt_point *points;
	size_t point_count;
} ctui_mvt_layer;

typedef struct ctui_mvt_tile {
	ctui_mvt_layer *layers;
	size_t layer_count;
} ctui_mvt_tile;

/* Decode a tile. On failure *out is left empty (nothing to free). */
ctui_mvt_status ctui_mvt_decode(const uint8_t *data, size_t len,
				ctui_mvt_tile *out);

void ctui_mvt_free(ctui_mvt_tile *tile);

/* Memory held by the tile, for a cache that bounds its size. */
size_t ctui_mvt_bytes(const ctui_mvt_tile *tile);

/* The value of property `key` of a feature, or NULL. */
const ctui_mvt_value *ctui_mvt_property(const ctui_mvt_layer *layer,
					const ctui_mvt_feature *feature,
					const char *key);

/* True for a string value equal to `text`. */
bool ctui_mvt_value_is(const ctui_mvt_value *value, const char *text);

/* True, and *out set, for an integer, double or bool value. */
bool ctui_mvt_value_number(const ctui_mvt_value *value, double *out);

/* The points of part `i` of a feature (NULL, and *count 0, when out of
 * range). */
const ctui_mvt_point *ctui_mvt_feature_part(const ctui_mvt_layer *layer,
					    const ctui_mvt_feature *feature,
					    size_t i, size_t *count);

/* Whether a polygon segment runs along, or outside, an edge of the tile
 * (0 or extent on both ends). Tiles clip their areas, so oceans grow
 * outlines that follow the tile borders; drawn, they would show as straight
 * lines across the map. */
bool ctui_mvt_edge_segment(ctui_mvt_point a, ctui_mvt_point b, uint32_t extent);

#endif /* CTUI_MVT_H */
