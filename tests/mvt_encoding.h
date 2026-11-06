/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * mvt_encoding.h - a minimal MVT / protobuf encoder to build test tiles
 * (the C counterpart of the encoder used to test the Python reference).
 *
 *   mb_buf *line = mb_line_feature(1, pts, 3, tags, 2);
 *   mb_buf *layer = mb_layer("transportation", 4096, keys, 1, values, 1, &line,
 * 1); mb_buf *tile = mb_tile(&layer, 1);
 *   ... tile->p, tile->len ...
 *   mb_release();                      frees every buffer made so far
 *
 * Buffers are owned by a global list, so tests never free one by hand.
 */
#ifndef CTUI_TESTS_MVT_ENCODING_H
#define CTUI_TESTS_MVT_ENCODING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct mb_buf {
	uint8_t *p;
	size_t len, cap;
} mb_buf;

static mb_buf **mb_all_;
static size_t mb_count_, mb_cap_;

static mb_buf *mb_new(void)
{
	mb_buf *b = calloc(1, sizeof *b);
	if (mb_count_ == mb_cap_) {
		mb_cap_ = mb_cap_ ? mb_cap_ * 2 : 64;
		mb_buf **grown = (mb_buf **)realloc((void *)mb_all_,
						    mb_cap_ * sizeof *mb_all_);
		if (!grown)
			abort();
		mb_all_ = grown;
	}
	mb_all_[mb_count_++] = b;
	return b;
}

static __attribute__((unused)) void mb_release(void)
{
	for (size_t i = 0; i < mb_count_; i++) {
		free(mb_all_[i]->p);
		free(mb_all_[i]);
	}
	free((void *)mb_all_);
	mb_all_ = NULL;
	mb_count_ = mb_cap_ = 0;
}

static void mb_put(mb_buf *b, const void *data, size_t n)
{
	if (!n)
		return;
	if (b->len + n > b->cap) {
		b->cap = (b->len + n) * 2;
		uint8_t *grown = realloc(b->p, b->cap);
		if (!grown)
			abort();
		b->p = grown;
	}
	memcpy(b->p + b->len, data, n);
	b->len += n;
}

static void mb_varint(mb_buf *b, uint64_t v)
{
	do {
		uint8_t byte = (uint8_t)(v & 0x7FU);
		v >>= 7U;
		if (v)
			byte = (uint8_t)(byte | 0x80U);
		mb_put(b, &byte, 1);
	} while (v);
}

static uint64_t mb_zigzag(int64_t v)
{
	return ((uint64_t)v << 1U) ^ (uint64_t)(-(v < 0));
}

static void mb_tag(mb_buf *b, unsigned number, unsigned wire)
{
	mb_varint(b, ((uint64_t)number << 3U) | wire);
}

/* A length-delimited field holding a copy of `payload`. */
static void mb_ld(mb_buf *b, unsigned number, const mb_buf *payload)
{
	mb_tag(b, number, 2);
	mb_varint(b, payload->len);
	mb_put(b, payload->p, payload->len);
}

static void mb_packed(mb_buf *b, unsigned number, const uint64_t *values,
		      size_t n)
{
	mb_buf tmp = {0};
	for (size_t i = 0; i < n; i++)
		mb_varint(&tmp, values[i]);
	mb_ld(b, number, &tmp);
	free(tmp.p);
}

/* Values of a layer's table. */
static __attribute__((unused)) mb_buf *mb_value_string(const char *text)
{
	mb_buf *b = mb_new(), s = {.p = (uint8_t *)text, .len = strlen(text)};
	mb_ld(b, 1, &s);
	return b;
}

static __attribute__((unused)) mb_buf *mb_value_float(float v)
{
	mb_buf *b = mb_new();
	mb_tag(b, 2, 5);
	mb_put(b, &v, 4); /* little endian hosts */
	return b;
}

static __attribute__((unused)) mb_buf *mb_value_double(double v)
{
	mb_buf *b = mb_new();
	mb_tag(b, 3, 1);
	mb_put(b, &v, 8);
	return b;
}

/* kind: 4 = int, 5 = uint, 6 = sint (zigzag), 7 = bool */
static __attribute__((unused)) mb_buf *mb_value_int(unsigned kind, int64_t v)
{
	mb_buf *b = mb_new();
	mb_tag(b, kind, 0);
	mb_varint(b, kind == 6 ? mb_zigzag(v) : (uint64_t)v);
	return b;
}

/* Geometry commands. */
enum { MB_MOVE_TO = 1, MB_LINE_TO = 2, MB_CLOSE_PATH = 7 };

static uint64_t mb_command(unsigned op, unsigned count)
{
	return ((uint64_t)count << 3U) | op;
}

typedef struct mb_cursor {
	int x, y;
} mb_cursor;

/* Append one part to a geometry: MoveTo the first point, LineTo the others,
 * ClosePath when `close`. The cursor carries the position between parts. */
static __attribute__((unused)) void
mb_path(mb_buf *geom, mb_cursor *cur, const int (*pts)[2], size_t n, bool close)
{
	mb_varint(geom, mb_command(MB_MOVE_TO, 1));
	mb_varint(geom, mb_zigzag(pts[0][0] - cur->x));
	mb_varint(geom, mb_zigzag(pts[0][1] - cur->y));
	cur->x = pts[0][0];
	cur->y = pts[0][1];
	if (n > 1) {
		mb_varint(geom, mb_command(MB_LINE_TO, (unsigned)n - 1));
		for (size_t i = 1; i < n; i++) {
			mb_varint(geom, mb_zigzag(pts[i][0] - cur->x));
			mb_varint(geom, mb_zigzag(pts[i][1] - cur->y));
			cur->x = pts[i][0];
			cur->y = pts[i][1];
		}
	}
	if (close)
		mb_varint(geom, mb_command(MB_CLOSE_PATH, 1));
}

/* A feature from raw tags (key, value index pairs) and geometry. */
static __attribute__((unused)) mb_buf *mb_feature(uint64_t id, unsigned type,
						  const uint64_t *tags,
						  size_t ntags,
						  const mb_buf *geometry)
{
	mb_buf *b = mb_new();
	mb_tag(b, 1, 0);
	mb_varint(b, id);
	if (ntags)
		mb_packed(b, 2, tags, ntags);
	mb_tag(b, 3, 0);
	mb_varint(b, type);
	mb_ld(b, 4, geometry);
	return b;
}

/* A one-part line (type 2) or polygon ring (type 3, closed) feature. */
static __attribute__((unused)) mb_buf *
mb_path_feature(uint64_t id, unsigned type, const int (*pts)[2], size_t n,
		const uint64_t *tags, size_t ntags)
{
	mb_buf *g = mb_new();
	mb_cursor cur = {0, 0};
	mb_path(g, &cur, pts, n, type == 3);
	return mb_feature(id, type, tags, ntags, g);
}

/* A one-point feature (type 1). */
static __attribute__((unused)) mb_buf *mb_point_feature(uint64_t id, int x,
							int y)
{
	mb_buf *g = mb_new();
	mb_varint(g, mb_command(MB_MOVE_TO, 1));
	mb_varint(g, mb_zigzag(x));
	mb_varint(g, mb_zigzag(y));
	return mb_feature(id, 1, NULL, 0, g);
}

/* A layer; extent 0 leaves the field out (the decoder assumes 4096). */
static __attribute__((unused)) mb_buf *
mb_layer(const char *name, unsigned extent, const char *const *keys,
	 size_t nkeys, mb_buf *const *values, size_t nvalues,
	 mb_buf *const *features, size_t nfeatures)
{
	mb_buf *b = mb_new(), n = {.p = (uint8_t *)name, .len = strlen(name)};
	mb_tag(b, 15, 0);
	mb_varint(b, 2);
	mb_ld(b, 1, &n);
	for (size_t i = 0; i < nfeatures; i++)
		mb_ld(b, 2, features[i]);
	for (size_t i = 0; i < nkeys; i++) {
		mb_buf k = {.p = (uint8_t *)keys[i], .len = strlen(keys[i])};
		mb_ld(b, 3, &k);
	}
	for (size_t i = 0; i < nvalues; i++)
		mb_ld(b, 4, values[i]);
	if (extent) {
		mb_tag(b, 5, 0);
		mb_varint(b, extent);
	}
	return b;
}

static __attribute__((unused)) mb_buf *mb_tile(mb_buf *const *layers, size_t n)
{
	mb_buf *b = mb_new();
	for (size_t i = 0; i < n; i++)
		mb_ld(b, 3, layers[i]);
	return b;
}

#endif /* CTUI_TESTS_MVT_ENCODING_H */
