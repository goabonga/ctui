/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * mvt.c - a decoder for Mapbox Vector Tiles (mvt.h).
 */
#include <ctui/mvt.h>

#include <stdlib.h>
#include <string.h>

#define MOVE_TO 1
#define LINE_TO 2
#define CLOSE_PATH 7

#define MAX_COORD 0x40000000 /* 2^30: a point further than this is garbage */

typedef struct reader {
	const uint8_t *p, *end;
} reader;

/* Make room for `need` items of `elem` bytes in the array whose pointer is
 * stored at `arrp` (kept through memcpy, so any item type may be used). */
static bool grow(void *arrp, size_t *cap, size_t need, size_t elem)
{
	if (need <= *cap)
		return true;
	size_t cap2 = *cap ? *cap : 8;
	while (cap2 < need) {
		if (cap2 > SIZE_MAX / 2 / elem)
			return false;
		cap2 *= 2;
	}
	void *cur;
	memcpy((void *)&cur, arrp, sizeof cur);
	void *p = realloc(cur, cap2 * elem);
	if (!p)
		return false;
	memcpy(arrp, (const void *)&p, sizeof p);
	*cap = cap2;
	return true;
}

static bool read_varint(reader *r, uint64_t *out)
{
	uint64_t v = 0;
	for (int shift = 0; shift < 70; shift += 7) {
		if (r->p >= r->end)
			return false;
		uint8_t b = *r->p++;
		if (shift < 64)
			v |= (uint64_t)((unsigned)b & 0x7FU) << (unsigned)shift;
		if (!((unsigned)b & 0x80U)) {
			*out = v;
			return true;
		}
	}
	return false; /* more than 10 bytes */
}

static int64_t zigzag(uint64_t v)
{
	return (int64_t)((v >> 1U) ^ (0U - (v & 1U)));
}

/* One protobuf field: its number and wire type, a varint value or a
 * length-delimited / fixed payload. */
typedef struct field {
	uint32_t number;
	int wire;
	uint64_t varint;
	const uint8_t *data;
	size_t len;
} field;

static bool next_field(reader *r, field *f)
{
	uint64_t key;
	if (!read_varint(r, &key))
		return false;
	f->number = (uint32_t)(key >> 3U);
	f->wire = (int)(key & 7U);
	f->data = NULL;
	f->len = 0;
	size_t left = (size_t)(r->end - r->p);
	switch (f->wire) {
	case 0:
		return read_varint(r, &f->varint);
	case 1:
	case 5:
		f->len = f->wire == 1 ? 8 : 4;
		break;
	case 2: {
		uint64_t n;
		if (!read_varint(r, &n))
			return false;
		left = (size_t)(r->end - r->p);
		if (n > left)
			return false;
		f->len = (size_t)n;
		break;
	}
	default:
		return false; /* groups and unknown wire types */
	}
	if (f->len > left)
		return false;
	f->data = r->p;
	r->p += f->len;
	return true;
}

static char *copy_string(const uint8_t *data, size_t len)
{
	char *s = malloc(len + 1);
	if (!s)
		return NULL;
	memcpy(s, data, len);
	s[len] = '\0';
	return s;
}

static void free_layer(ctui_mvt_layer *l)
{
	free(l->name);
	for (size_t i = 0; i < l->key_count; i++)
		free(l->keys[i]);
	free((void *)l->keys);
	for (size_t i = 0; i < l->value_count; i++)
		free(l->values[i].string);
	free(l->values);
	free(l->features);
	free(l->tags);
	free(l->parts);
	free(l->points);
	*l = (ctui_mvt_layer){0};
}

void ctui_mvt_free(ctui_mvt_tile *tile)
{
	for (size_t i = 0; i < tile->layer_count; i++)
		free_layer(&tile->layers[i]);
	free(tile->layers);
	*tile = (ctui_mvt_tile){0};
}

size_t ctui_mvt_bytes(const ctui_mvt_tile *tile)
{
	size_t n = sizeof *tile + tile->layer_count * sizeof(ctui_mvt_layer);
	for (size_t i = 0; i < tile->layer_count; i++) {
		const ctui_mvt_layer *l = &tile->layers[i];
		n += l->key_count * (sizeof(char *) + 16) +
		     l->value_count * (sizeof(ctui_mvt_value) + 16) +
		     l->feature_count * sizeof(ctui_mvt_feature) +
		     l->tag_count * 2 * sizeof(uint32_t) +
		     l->part_count * sizeof(ctui_mvt_part) +
		     l->point_count * sizeof(ctui_mvt_point);
	}
	return n;
}

/* A Value message. */
static ctui_mvt_status parse_value(const uint8_t *data, size_t len,
				   ctui_mvt_value *out)
{
	reader r = {data, data + len};
	field f;
	*out = (ctui_mvt_value){0};
	while (r.p < r.end) {
		if (!next_field(&r, &f))
			return CTUI_MVT_ERR_FORMAT;
		if (f.number == 1 && f.wire == 2) {
			char *s = copy_string(f.data, f.len);
			if (!s)
				return CTUI_MVT_ERR_MEMORY;
			free(out->string);
			out->type = CTUI_MVT_V_STRING;
			out->string = s;
		} else if (f.number == 2 && f.wire == 5) {
			uint32_t bits = (uint32_t)f.data[0] |
					(uint32_t)f.data[1] << 8U |
					(uint32_t)f.data[2] << 16U |
					(uint32_t)f.data[3] << 24U;
			float x;
			memcpy(&x, &bits, 4);
			out->type = CTUI_MVT_V_DOUBLE;
			out->number = (double)x;
		} else if (f.number == 3 && f.wire == 1) {
			uint64_t bits = 0;
			for (int i = 7; i >= 0; i--)
				bits = bits << 8U | f.data[i];
			double x;
			memcpy(&x, &bits, 8);
			out->type = CTUI_MVT_V_DOUBLE;
			out->number = x;
		} else if ((f.number == 4 || f.number == 5) && f.wire == 0) {
			out->type = CTUI_MVT_V_INT;
			out->integer = f.number == 4 ? (int64_t)f.varint
				       : f.varint > (uint64_t)INT64_MAX
					   ? INT64_MAX
					   : (int64_t)f.varint;
		} else if (f.number == 6 && f.wire == 0) {
			out->type = CTUI_MVT_V_INT;
			out->integer = zigzag(f.varint);
		} else if (f.number == 7 && f.wire == 0) {
			out->type = CTUI_MVT_V_BOOL;
			out->integer = f.varint != 0;
		}
	}
	return CTUI_MVT_OK;
}

typedef struct layer_build {
	size_t tag_cap, part_cap, point_cap, feature_cap;
} layer_build;

/* Decode the geometry commands of a feature into the layer's parts. */
static ctui_mvt_status parse_geometry(ctui_mvt_layer *l, layer_build *b,
				      ctui_mvt_feature *feat,
				      const uint8_t *data, size_t len)
{
	reader r = {data, data + len};
	int64_t x = 0, y = 0;
	bool open = false; /* a part is being built */
	feat->part_first = l->part_count;
	while (r.p < r.end) {
		uint64_t cmd;
		if (!read_varint(&r, &cmd))
			return CTUI_MVT_ERR_FORMAT;
		uint64_t op = cmd & 7U, count = cmd >> 3U;
		if (op == CLOSE_PATH) {
			if (!open)
				return CTUI_MVT_ERR_FORMAT;
			ctui_mvt_part *part = &l->parts[l->part_count - 1];
			if (!grow((void *)&l->points, &b->point_cap,
				  l->point_count + 1, sizeof *l->points))
				return CTUI_MVT_ERR_MEMORY;
			l->points[l->point_count++] = l->points[part->first];
			part->count++;
			continue;
		}
		if (op != MOVE_TO && op != LINE_TO)
			return CTUI_MVT_ERR_FORMAT;
		/* every point takes two bytes at least: this bounds the
		 * allocation */
		if (count > (uint64_t)(r.end - r.p) / 2)
			return CTUI_MVT_ERR_FORMAT;
		for (uint64_t i = 0; i < count; i++) {
			uint64_t dx, dy;
			if (!read_varint(&r, &dx) || !read_varint(&r, &dy))
				return CTUI_MVT_ERR_FORMAT;
			x += zigzag(dx);
			y += zigzag(dy);
			if (x > MAX_COORD || x < -MAX_COORD || y > MAX_COORD ||
			    y < -MAX_COORD)
				return CTUI_MVT_ERR_FORMAT;
			if (op == MOVE_TO || !open) {
				if (!grow((void *)&l->parts, &b->part_cap,
					  l->part_count + 1, sizeof *l->parts))
					return CTUI_MVT_ERR_MEMORY;
				l->parts[l->part_count++] =
				    (ctui_mvt_part){l->point_count, 0};
				open = true;
			}
			if (!grow((void *)&l->points, &b->point_cap,
				  l->point_count + 1, sizeof *l->points))
				return CTUI_MVT_ERR_MEMORY;
			l->points[l->point_count++] =
			    (ctui_mvt_point){(int32_t)x, (int32_t)y};
			l->parts[l->part_count - 1].count++;
		}
	}
	feat->part_count = l->part_count - feat->part_first;
	return CTUI_MVT_OK;
}

/* A Feature message; tags and geometry go to the flat arrays of the layer. */
static ctui_mvt_status parse_feature(ctui_mvt_layer *l, layer_build *b,
				     const uint8_t *data, size_t len)
{
	reader r = {data, data + len};
	field f;
	ctui_mvt_feature feat = {0};
	const uint8_t *tags = NULL, *geom = NULL;
	size_t tags_len = 0, geom_len = 0;
	while (r.p < r.end) {
		if (!next_field(&r, &f))
			return CTUI_MVT_ERR_FORMAT;
		if (f.number == 1 && f.wire == 0)
			feat.id = f.varint;
		else if (f.number == 2 && f.wire == 2) {
			tags = f.data;
			tags_len = f.len;
		} else if (f.number == 3 && f.wire == 0)
			feat.type = f.varint <= CTUI_MVT_G_POLYGON
					? (ctui_mvt_geom)f.varint
					: CTUI_MVT_G_UNKNOWN;
		else if (f.number == 4 && f.wire == 2) {
			geom = f.data;
			geom_len = f.len;
		}
	}
	reader tr = {tags, tags + tags_len};
	feat.tag_first = l->tag_count;
	while (tr.p < tr.end) {
		uint64_t k, v;
		if (!read_varint(&tr, &k) || !read_varint(&tr, &v))
			return CTUI_MVT_ERR_FORMAT; /* an odd number of tags is
						       cut short */
		if (k >= l->key_count || v >= l->value_count)
			return CTUI_MVT_ERR_FORMAT;
		if (!grow((void *)&l->tags, &b->tag_cap, (l->tag_count + 1) * 2,
			  sizeof *l->tags))
			return CTUI_MVT_ERR_MEMORY;
		l->tags[l->tag_count * 2] = (uint32_t)k;
		l->tags[l->tag_count * 2 + 1] = (uint32_t)v;
		l->tag_count++;
		feat.tag_count++;
	}
	ctui_mvt_status st = parse_geometry(l, b, &feat, geom, geom_len);
	if (st != CTUI_MVT_OK)
		return st;
	if (!grow((void *)&l->features, &b->feature_cap, l->feature_count + 1,
		  sizeof *l->features))
		return CTUI_MVT_ERR_MEMORY;
	l->features[l->feature_count++] = feat;
	return CTUI_MVT_OK;
}

typedef struct slice {
	const uint8_t *data;
	size_t len;
} slice;

static ctui_mvt_status parse_layer(const uint8_t *data, size_t len,
				   ctui_mvt_layer *l)
{
	reader r = {data, data + len};
	field f;
	layer_build b = {0};
	size_t key_cap = 0, value_cap = 0, slice_cap = 0, nslices = 0;
	slice *slices = NULL;
	ctui_mvt_status st = CTUI_MVT_OK;
	l->extent = 4096;
	while (st == CTUI_MVT_OK && r.p < r.end) {
		if (!next_field(&r, &f)) {
			st = CTUI_MVT_ERR_FORMAT;
			break;
		}
		if (f.number == 1 && f.wire == 2) {
			char *name = copy_string(f.data, f.len);
			if (!name) {
				st = CTUI_MVT_ERR_MEMORY;
				break;
			}
			free(l->name);
			l->name = name;
		} else if (f.number == 2 &&
			   f.wire == 2) { /* features wait for the key / value
					     tables */
			if (!grow((void *)&slices, &slice_cap, nslices + 1,
				  sizeof *slices))
				st = CTUI_MVT_ERR_MEMORY;
			else
				slices[nslices++] = (slice){f.data, f.len};
		} else if (f.number == 3 && f.wire == 2) {
			char *key = copy_string(f.data, f.len);
			if (!key || !grow((void *)&l->keys, &key_cap,
					  l->key_count + 1, sizeof *l->keys)) {
				free(key);
				st = CTUI_MVT_ERR_MEMORY;
			} else
				l->keys[l->key_count++] = key;
		} else if (f.number == 4 && f.wire == 2) {
			if (!grow((void *)&l->values, &value_cap,
				  l->value_count + 1, sizeof *l->values))
				st = CTUI_MVT_ERR_MEMORY;
			else {
				st = parse_value(f.data, f.len,
						 &l->values[l->value_count]);
				if (st == CTUI_MVT_OK) {
					l->value_count++;
				} else { /* not counted, so not freed with the
					    layer */
					free(l->values[l->value_count].string);
					l->values[l->value_count].string = NULL;
				}
			}
		} else if (f.number == 5 && f.wire == 0) {
			if (f.varint == 0 || f.varint > 1U << 30U)
				st = CTUI_MVT_ERR_FORMAT;
			else
				l->extent = (uint32_t)f.varint;
		}
	}
	for (size_t i = 0; st == CTUI_MVT_OK && i < nslices; i++)
		st = parse_feature(l, &b, slices[i].data, slices[i].len);
	free(slices);
	if (!l->name && st == CTUI_MVT_OK) {
		l->name = copy_string((const uint8_t *)"", 0);
		if (!l->name)
			st = CTUI_MVT_ERR_MEMORY;
	}
	return st;
}

ctui_mvt_status ctui_mvt_decode(const uint8_t *data, size_t len,
				ctui_mvt_tile *out)
{
	*out = (ctui_mvt_tile){0};
	if (!data && len)
		return CTUI_MVT_ERR_FORMAT;
	reader r = {data, data + len};
	field f;
	size_t cap = 0;
	ctui_mvt_status st = CTUI_MVT_OK;
	while (st == CTUI_MVT_OK && r.p < r.end) {
		if (!next_field(&r, &f)) {
			st = CTUI_MVT_ERR_FORMAT;
			break;
		}
		if (f.number != 3 || f.wire != 2)
			continue;
		if (!grow((void *)&out->layers, &cap, out->layer_count + 1,
			  sizeof *out->layers)) {
			st = CTUI_MVT_ERR_MEMORY;
			break;
		}
		ctui_mvt_layer *l = &out->layers[out->layer_count];
		*l = (ctui_mvt_layer){0};
		out->layer_count++;
		st = parse_layer(f.data, f.len, l);
	}
	if (st != CTUI_MVT_OK)
		ctui_mvt_free(out);
	return st;
}

const ctui_mvt_value *ctui_mvt_property(const ctui_mvt_layer *layer,
					const ctui_mvt_feature *feature,
					const char *key)
{
	for (size_t i = 0; i < feature->tag_count; i++) {
		const uint32_t *t = &layer->tags[(feature->tag_first + i) * 2];
		if (strcmp(layer->keys[t[0]], key) == 0)
			return &layer->values[t[1]];
	}
	return NULL;
}

bool ctui_mvt_value_is(const ctui_mvt_value *value, const char *text)
{
	return value && value->type == CTUI_MVT_V_STRING &&
	       strcmp(value->string, text) == 0;
}

bool ctui_mvt_value_number(const ctui_mvt_value *value, double *out)
{
	if (!value)
		return false;
	switch (value->type) {
	case CTUI_MVT_V_DOUBLE:
		*out = value->number;
		return true;
	case CTUI_MVT_V_INT:
	case CTUI_MVT_V_BOOL:
		*out = (double)value->integer;
		return true;
	default:
		return false;
	}
}

const ctui_mvt_point *ctui_mvt_feature_part(const ctui_mvt_layer *layer,
					    const ctui_mvt_feature *feature,
					    size_t i, size_t *count)
{
	if (i >= feature->part_count) {
		*count = 0;
		return NULL;
	}
	const ctui_mvt_part *p = &layer->parts[feature->part_first + i];
	*count = p->count;
	return &layer->points[p->first];
}

bool ctui_mvt_edge_segment(ctui_mvt_point a, ctui_mvt_point b, uint32_t extent)
{
	int64_t e = (int64_t)extent;
	return (a.x <= 0 && b.x <= 0) || (a.x >= e && b.x >= e) ||
	       (a.y <= 0 && b.y <= 0) || (a.y >= e && b.y >= e);
}
