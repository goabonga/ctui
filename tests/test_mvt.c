/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_mvt.c - the vector tile decoder, on tiles built by mvt_encoding.h.
 */
#include "check.h"
#include "mvt_encoding.h"

#include <ctui/mvt.h>

static ctui_mvt_status decode(const mb_buf *b, ctui_mvt_tile *t)
{
	return ctui_mvt_decode(b->p, b->len, t);
}

static void test_empty(void)
{
	ctui_mvt_tile t;
	CHECK(ctui_mvt_decode(NULL, 0, &t) == CTUI_MVT_OK);
	CHECK(t.layer_count == 0 && t.layers == NULL);
	ctui_mvt_free(&t);
	mb_buf *tile = mb_tile(NULL, 0);
	CHECK(decode(tile, &t) == CTUI_MVT_OK && t.layer_count == 0);
	ctui_mvt_free(&t);
	CHECK(ctui_mvt_decode(NULL, 5, &t) == CTUI_MVT_ERR_FORMAT);
	mb_release();
}

static void test_line_layer(void)
{
	static const int PTS[][2] = {{10, 20}, {30, 20}, {30, 50}};
	const char *keys[] = {"class", "rank"};
	mb_buf *values[] = {mb_value_string("motorway"), mb_value_int(6, -7)};
	uint64_t tags[] = {0, 0, 1, 1};
	mb_buf *feat = mb_path_feature(42, 2, PTS, 3, tags, 4);
	mb_buf *layer =
	    mb_layer("transportation", 2048, keys, 2, values, 2, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);

	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK);
	CHECK(t.layer_count == 1);
	const ctui_mvt_layer *l = &t.layers[0];
	CHECK_STR(l->name, "transportation");
	CHECK(l->extent == 2048 && l->key_count == 2 && l->value_count == 2);
	CHECK(l->feature_count == 1);
	const ctui_mvt_feature *f = &l->features[0];
	CHECK(f->id == 42 && f->type == CTUI_MVT_G_LINESTRING &&
	      f->tag_count == 2);
	CHECK(f->part_count == 1);
	size_t n;
	const ctui_mvt_point *p = ctui_mvt_feature_part(l, f, 0, &n);
	CHECK(n == 3 && p[0].x == 10 && p[0].y == 20 && p[1].x == 30 &&
	      p[1].y == 20 && p[2].x == 30 && p[2].y == 50);
	CHECK(ctui_mvt_feature_part(l, f, 1, &n) == NULL && n == 0);
	CHECK(ctui_mvt_value_is(ctui_mvt_property(l, f, "class"), "motorway"));
	CHECK(!ctui_mvt_value_is(ctui_mvt_property(l, f, "class"), "rail"));
	const ctui_mvt_value *rank = ctui_mvt_property(l, f, "rank");
	double num;
	CHECK(rank && rank->type == CTUI_MVT_V_INT && rank->integer == -7);
	CHECK(ctui_mvt_value_number(rank, &num) && num == -7);
	CHECK(ctui_mvt_property(l, f, "missing") == NULL);
	CHECK(!ctui_mvt_value_number(ctui_mvt_property(l, f, "class"), &num));
	CHECK(!ctui_mvt_value_is(NULL, "x") &&
	      !ctui_mvt_value_number(NULL, &num));
	CHECK(ctui_mvt_bytes(&t) > 0);
	ctui_mvt_free(&t);
	CHECK(t.layer_count == 0 && t.layers == NULL);
	mb_release();
}

static void test_default_extent(void)
{
	static const int PTS[][2] = {{0, 0}, {5, 5}};
	mb_buf *feat = mb_path_feature(1, 2, PTS, 2, NULL, 0);
	mb_buf *layer = mb_layer("x", 0, NULL, 0, NULL, 0, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK && t.layers[0].extent == 4096);
	ctui_mvt_free(&t);
	mb_release();
}

static void test_polygon_and_parts(void)
{
	static const int RING[][2] = {{0, 0}, {100, 0}, {100, 100}};
	static const int HOLE[][2] = {{20, 20}, {20, 40}, {40, 40}};
	mb_buf *g = mb_new();
	mb_cursor cur = {0, 0};
	mb_path(g, &cur, RING, 3, true);
	mb_path(g, &cur, HOLE, 3, true);
	mb_buf *feat = mb_feature(1, 3, NULL, 0, g);
	mb_buf *layer = mb_layer("water", 4096, NULL, 0, NULL, 0, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK);
	const ctui_mvt_layer *l = &t.layers[0];
	const ctui_mvt_feature *f = &l->features[0];
	CHECK(f->type == CTUI_MVT_G_POLYGON && f->part_count == 2);
	size_t n;
	const ctui_mvt_point *p = ctui_mvt_feature_part(l, f, 0, &n);
	CHECK(n == 4 && p[3].x == 0 && p[3].y == 0); /* the ring is closed */
	p = ctui_mvt_feature_part(l, f, 1, &n);
	CHECK(n == 4 && p[0].x == 20 && p[0].y == 20 && p[3].x == 20 &&
	      p[3].y == 20);
	ctui_mvt_free(&t);
	mb_release();
}

static void test_multipoint(void)
{
	mb_buf *g = mb_new();
	mb_varint(g, mb_command(MB_MOVE_TO,
				2)); /* two points: two parts of one point */
	mb_varint(g, mb_zigzag(5));
	mb_varint(g, mb_zigzag(6));
	mb_varint(g, mb_zigzag(10));
	mb_varint(g, mb_zigzag(-2));
	mb_buf *feat = mb_feature(9, 1, NULL, 0, g);
	mb_buf *layer = mb_layer("poi", 4096, NULL, 0, NULL, 0, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK);
	const ctui_mvt_feature *f = &t.layers[0].features[0];
	size_t n;
	CHECK(f->type == CTUI_MVT_G_POINT && f->part_count == 2);
	const ctui_mvt_point *p = ctui_mvt_feature_part(&t.layers[0], f, 1, &n);
	CHECK(n == 1 && p[0].x == 15 &&
	      p[0].y == 4); /* relative to the first */
	ctui_mvt_free(&t);
	mb_release();
}

static void test_values(void)
{
	const char *keys[] = {"s", "f", "d", "i", "u", "z", "b"};
	mb_buf *values[] = {mb_value_string("a\xc3\xa9"),
			    mb_value_float(1.5f),
			    mb_value_double(-2.25),
			    mb_value_int(4, 123456789012LL),
			    mb_value_int(5, 77),
			    mb_value_int(6, -300),
			    mb_value_int(7, 1)};
	uint64_t tags[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6};
	static const int PTS[][2] = {{0, 0}, {1, 1}};
	mb_buf *feat = mb_path_feature(1, 2, PTS, 2, tags, 14);
	mb_buf *layer = mb_layer("v", 4096, keys, 7, values, 7, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK);
	const ctui_mvt_layer *l = &t.layers[0];
	const ctui_mvt_feature *f = &l->features[0];
	CHECK_STR(ctui_mvt_property(l, f, "s")->string, "a\xc3\xa9");
	double v;
	CHECK(ctui_mvt_value_number(ctui_mvt_property(l, f, "f"), &v) &&
	      v == 1.5);
	CHECK(ctui_mvt_value_number(ctui_mvt_property(l, f, "d"), &v) &&
	      v == -2.25);
	CHECK(ctui_mvt_property(l, f, "i")->integer == 123456789012LL);
	CHECK(ctui_mvt_property(l, f, "u")->integer == 77);
	CHECK(ctui_mvt_property(l, f, "z")->integer == -300);
	CHECK(ctui_mvt_property(l, f, "b")->type == CTUI_MVT_V_BOOL &&
	      ctui_mvt_property(l, f, "b")->integer == 1);
	CHECK(ctui_mvt_value_number(ctui_mvt_property(l, f, "b"), &v) &&
	      v == 1);
	ctui_mvt_free(&t);
	mb_release();
}

static void test_layers_and_unknown_fields(void)
{
	static const int PTS[][2] = {{0, 0}, {9, 9}};
	mb_buf *f1 = mb_path_feature(1, 2, PTS, 2, NULL, 0);
	mb_buf *f2 = mb_path_feature(2, 2, PTS, 2, NULL, 0);
	mb_buf *layers[] = {mb_layer("a", 4096, NULL, 0, NULL, 0, &f1, 1),
			    mb_layer("b", 4096, NULL, 0, NULL, 0, &f2, 1)};
	mb_buf *tile = mb_tile(layers, 2);
	/* an unknown varint field (number 9) and a fixed32 one (number 10) are
	 * skipped */
	mb_tag(tile, 9, 0);
	mb_varint(tile, 300);
	mb_tag(tile, 10, 5);
	mb_put(tile, "abcd", 4);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK && t.layer_count == 2);
	CHECK_STR(t.layers[0].name, "a");
	CHECK_STR(t.layers[1].name, "b");
	CHECK(t.layers[1].features[0].id == 2);
	ctui_mvt_free(&t);
	mb_release();
}

/* The keys and values may follow the features on the wire. */
static void test_tables_after_features(void)
{
	static const int PTS[][2] = {{0, 0}, {9, 9}};
	uint64_t tags[] = {0, 0};
	mb_buf *feat = mb_path_feature(1, 2, PTS, 2, tags, 2);
	mb_buf *layer = mb_new(), *val = mb_value_string("v");
	mb_buf name = {.p = (uint8_t *)"l", .len = 1},
	       key = {.p = (uint8_t *)"k", .len = 1};
	mb_ld(layer, 1, &name);
	mb_ld(layer, 2, feat);
	mb_ld(layer, 3, &key);
	mb_ld(layer, 4, val);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_OK);
	CHECK(ctui_mvt_value_is(
	    ctui_mvt_property(&t.layers[0], &t.layers[0].features[0], "k"),
	    "v"));
	ctui_mvt_free(&t);
	mb_release();
}

static void test_malformed(void)
{
	static const int PTS[][2] = {{0, 0}, {9, 9}};
	ctui_mvt_tile t;
	mb_buf *feat = mb_path_feature(1, 2, PTS, 2, NULL, 0);
	mb_buf *layer = mb_layer("x", 4096, NULL, 0, NULL, 0, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	/* every truncation of a valid tile is refused or decodes to a prefix,
	 * never read past the end */
	int refused = 0;
	for (size_t cut = 0; cut < tile->len; cut++) {
		ctui_mvt_status st = ctui_mvt_decode(tile->p, cut, &t);
		if (st != CTUI_MVT_OK)
			refused++;
		CHECK(st != CTUI_MVT_OK || t.layer_count <= 1);
		ctui_mvt_free(&t);
	}
	CHECK(refused > 0);
	CHECK(decode(tile, &t) == CTUI_MVT_OK);
	ctui_mvt_free(&t);

	/* a varint longer than ten bytes */
	uint8_t longvar[] = {0x18, 0xff, 0xff, 0xff, 0xff, 0xff,
			     0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
	CHECK(ctui_mvt_decode(longvar, sizeof longvar, &t) ==
	      CTUI_MVT_ERR_FORMAT);
	/* a length past the end */
	uint8_t toolong[] = {0x1a, 0x7f, 0x01};
	CHECK(ctui_mvt_decode(toolong, sizeof toolong, &t) ==
	      CTUI_MVT_ERR_FORMAT);
	/* a group wire type */
	uint8_t group[] = {0x1b};
	CHECK(ctui_mvt_decode(group, sizeof group, &t) == CTUI_MVT_ERR_FORMAT);
	mb_release();
}

/* A value that fails after its string was read: refused, and (under ASan) not
 * leaked. */
static void test_bad_value(void)
{
	mb_buf *value = mb_value_string("ab");
	mb_put(value, "\x12\x7f",
	       2); /* a field 2 of 127 bytes with none behind it */
	mb_buf *layer = mb_layer("x", 4096, NULL, 0, &value, 1, NULL, 0);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	CHECK(decode(tile, &t) == CTUI_MVT_ERR_FORMAT);
	CHECK(t.layer_count == 0 && t.layers == NULL);
	mb_release();
}

static mb_buf *tile_of_geometry(mb_buf *geom, const uint64_t *tags,
				size_t ntags)
{
	const char *keys[] = {"k"};
	mb_buf *values[] = {mb_value_string("v")};
	mb_buf *feat = mb_feature(1, 2, tags, ntags, geom);
	mb_buf *layer = mb_layer("x", 4096, keys, 1, values, 1, &feat, 1);
	return mb_tile(&layer, 1);
}

static void test_bad_geometry(void)
{
	ctui_mvt_tile t;
	mb_buf *g = mb_new();
	/* ClosePath with no MoveTo */
	mb_varint(g, mb_command(MB_CLOSE_PATH, 1));
	CHECK(decode(tile_of_geometry(g, NULL, 0), &t) == CTUI_MVT_ERR_FORMAT);
	/* a command that is not MoveTo, LineTo or ClosePath */
	g = mb_new();
	mb_varint(g, mb_command(3, 1));
	CHECK(decode(tile_of_geometry(g, NULL, 0), &t) == CTUI_MVT_ERR_FORMAT);
	/* a count with fewer coordinates behind it than it says (also a huge
	 * count) */
	g = mb_new();
	mb_varint(g, mb_command(MB_MOVE_TO, 1U << 28U));
	mb_varint(g, 2);
	mb_varint(g, 2);
	CHECK(decode(tile_of_geometry(g, NULL, 0), &t) == CTUI_MVT_ERR_FORMAT);
	g = mb_new();
	mb_varint(g, mb_command(MB_MOVE_TO, 1));
	mb_varint(g, 2); /* the y is missing */
	CHECK(decode(tile_of_geometry(g, NULL, 0), &t) == CTUI_MVT_ERR_FORMAT);
	/* a point way out of the grid */
	g = mb_new();
	mb_varint(g, mb_command(MB_MOVE_TO, 1));
	mb_varint(g, mb_zigzag(1099511627776LL));
	mb_varint(g, 0);
	CHECK(decode(tile_of_geometry(g, NULL, 0), &t) == CTUI_MVT_ERR_FORMAT);
	/* tags: a key past the table, a value past the table, an odd count */
	uint64_t bad_key[] = {1, 0}, bad_val[] = {0, 1}, odd[] = {0, 0, 0};
	g = mb_new();
	CHECK(decode(tile_of_geometry(g, bad_key, 2), &t) ==
	      CTUI_MVT_ERR_FORMAT);
	CHECK(decode(tile_of_geometry(g, bad_val, 2), &t) ==
	      CTUI_MVT_ERR_FORMAT);
	CHECK(decode(tile_of_geometry(g, odd, 3), &t) == CTUI_MVT_ERR_FORMAT);
	/* a feature without geometry is fine */
	uint64_t ok[] = {0, 0};
	CHECK(decode(tile_of_geometry(g, ok, 2), &t) == CTUI_MVT_OK &&
	      t.layers[0].features[0].part_count == 0);
	ctui_mvt_free(&t);
	mb_release();
}

static void test_edge_segment(void)
{
	ctui_mvt_point a = {0, 10}, b = {0, 90}, c = {50, 50}, d = {4096, 3},
		       e = {4096, 4096};
	CHECK(ctui_mvt_edge_segment(a, b, 4096));  /* along the west edge */
	CHECK(!ctui_mvt_edge_segment(a, c, 4096)); /* leaves it */
	CHECK(ctui_mvt_edge_segment(d, e, 4096));  /* along the east edge */
	CHECK(ctui_mvt_edge_segment((ctui_mvt_point){-5, 3},
				    (ctui_mvt_point){-9, 80},
				    4096)); /* outside */
	CHECK(ctui_mvt_edge_segment((ctui_mvt_point){3, 4096},
				    (ctui_mvt_point){90, 4200},
				    4096)); /* south */
	CHECK(ctui_mvt_edge_segment((ctui_mvt_point){3, 0},
				    (ctui_mvt_point){90, 0}, 4096)); /* north */
	CHECK(!ctui_mvt_edge_segment(c, c, 4096));
	CHECK(!ctui_mvt_edge_segment((ctui_mvt_point){0, 10},
				     (ctui_mvt_point){4096, 10},
				     4096)); /* across */
}

/* Corrupted tiles never crash the decoder (run under ASan / UBSan too). */
static void test_fuzz(void)
{
	static const int RING[][2] = {{0, 0}, {100, 0}, {100, 100}};
	const char *keys[] = {"class"};
	mb_buf *values[] = {mb_value_string("sea")};
	uint64_t tags[] = {0, 0};
	mb_buf *feat = mb_path_feature(3, 3, RING, 3, tags, 2);
	mb_buf *layer = mb_layer("water", 4096, keys, 1, values, 1, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	uint8_t *copy = malloc(tile->len);
	unsigned seed = 12345;
	int ok = 0;
	for (int round = 0; round < 3000; round++) {
		memcpy(copy, tile->p, tile->len);
		for (int k = 0; k < 1 + round % 3; k++) {
			seed = seed * 1103515245U + 12345U;
			copy[(seed >> 8U) % tile->len] = (uint8_t)(seed >> 20U);
		}
		ctui_mvt_tile t;
		if (ctui_mvt_decode(copy, tile->len, &t) == CTUI_MVT_OK) {
			ok++;
			for (size_t i = 0; i < t.layer_count; i++)
				for (size_t j = 0;
				     j < t.layers[i].feature_count; j++)
					(void)ctui_mvt_property(
					    &t.layers[i],
					    &t.layers[i].features[j], "class");
		}
		ctui_mvt_free(&t);
	}
	CHECK(ok > 0 && ok < 3000);
	free(copy);
	mb_release();
}

int main(void)
{
	RUN(test_empty);
	RUN(test_line_layer);
	RUN(test_default_extent);
	RUN(test_polygon_and_parts);
	RUN(test_multipoint);
	RUN(test_values);
	RUN(test_layers_and_unknown_fields);
	RUN(test_tables_after_features);
	RUN(test_malformed);
	RUN(test_bad_value);
	RUN(test_bad_geometry);
	RUN(test_edge_segment);
	RUN(test_fuzz);
	return CHECK_DONE();
}
