/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_tilecache.c - the bounded LRU cache of decoded tiles.
 */
#include "check.h"
#include "mvt_encoding.h"

#include <ctui/tilecache.h>

/* A decoded tile with one layer named after `tag`, holding `points` points. */
static ctui_mvt_tile make_tile(const char *tag, int points)
{
	int (*pts)[2] = malloc((size_t)points * sizeof *pts);
	for (int i = 0; i < points; i++) {
		pts[i][0] = i;
		pts[i][1] = i;
	}
	mb_buf *feat = mb_path_feature(1, 2, (const int (*)[2])pts,
				       (size_t)points, NULL, 0);
	mb_buf *layer = mb_layer(tag, 4096, NULL, 0, NULL, 0, &feat, 1);
	mb_buf *tile = mb_tile(&layer, 1);
	ctui_mvt_tile t;
	if (ctui_mvt_decode(tile->p, tile->len, &t) != CTUI_MVT_OK)
		t = (ctui_mvt_tile){0};
	free(pts);
	mb_release();
	return t;
}

static void test_basic(void)
{
	ctui_tilecache c;
	CHECK(!ctui_tilecache_init(&c, 0, 0));
	CHECK(ctui_tilecache_init(&c, 3, 0));
	CHECK(ctui_tilecache_get(&c, 1, 0, 0) == NULL);
	ctui_mvt_tile t = make_tile("a", 2);
	const ctui_mvt_tile *stored = ctui_tilecache_put(&c, 1, 0, 0, &t);
	CHECK(stored && t.layers == NULL && t.layer_count == 0); /* taken */
	CHECK(c.count == 1 && c.bytes > 0);
	const ctui_mvt_tile *got = ctui_tilecache_get(&c, 1, 0, 0);
	CHECK(got == stored);
	if (got == NULL)
		return;
	CHECK(got->layer_count == 1);
	CHECK_STR(got->layers[0].name, "a");
	CHECK(ctui_tilecache_get(&c, 1, 0, 1) == NULL &&
	      ctui_tilecache_get(&c, 2, 0, 0) == NULL);
	ctui_tilecache_clear(&c);
	CHECK(c.count == 0 && c.bytes == 0 &&
	      ctui_tilecache_get(&c, 1, 0, 0) == NULL);
	t = make_tile("b", 2);
	CHECK(ctui_tilecache_put(&c, 1, 0, 0, &t) != NULL); /* still usable */
	ctui_tilecache_free(&c);
	CHECK(c.entries == NULL && c.count == 0);
	ctui_tilecache_free(&c); /* twice is fine */
}

static void test_lru(void)
{
	ctui_tilecache c;
	CHECK(ctui_tilecache_init(&c, 3, 0));
	for (int i = 0; i < 3; i++) {
		ctui_mvt_tile t = make_tile("t", 2);
		ctui_tilecache_put(&c, 5, i, 0, &t);
	}
	CHECK(c.count == 3);
	CHECK(ctui_tilecache_get(&c, 5, 0, 0) !=
	      NULL); /* 0 is now the freshest, 1 the oldest */
	ctui_mvt_tile t = make_tile("t", 2);
	ctui_tilecache_put(&c, 5, 3, 0, &t);
	CHECK(c.count == 3);
	CHECK(ctui_tilecache_get(&c, 5, 1, 0) == NULL);
	CHECK(ctui_tilecache_get(&c, 5, 0, 0) &&
	      ctui_tilecache_get(&c, 5, 2, 0) &&
	      ctui_tilecache_get(&c, 5, 3, 0));
	/* many more tiles never grow it */
	for (int i = 10; i < 100; i++) {
		t = make_tile("t", 2);
		ctui_tilecache_put(&c, 6, i, 0, &t);
		CHECK(c.count <= 3);
	}
	CHECK(ctui_tilecache_get(&c, 6, 99, 0) &&
	      ctui_tilecache_get(&c, 6, 98, 0) &&
	      ctui_tilecache_get(&c, 6, 97, 0));
	CHECK(ctui_tilecache_get(&c, 6, 96, 0) == NULL);
	ctui_tilecache_free(&c);
}

static void test_replace(void)
{
	ctui_tilecache c;
	CHECK(ctui_tilecache_init(&c, 2, 0));
	ctui_mvt_tile t = make_tile("old", 2);
	ctui_tilecache_put(&c, 1, 1, 1, &t);
	t = make_tile("new", 2);
	const ctui_mvt_tile *s = ctui_tilecache_put(&c, 1, 1, 1, &t);
	CHECK(c.count == 1);
	CHECK_STR(s->layers[0].name, "new");
	CHECK_STR(ctui_tilecache_get(&c, 1, 1, 1)->layers[0].name, "new");
	/* the same x and y at another zoom is another tile */
	t = make_tile("z2", 2);
	ctui_tilecache_put(&c, 2, 1, 1, &t);
	CHECK(c.count == 2);
	ctui_tilecache_free(&c);
}

static void test_byte_limit(void)
{
	ctui_mvt_tile probe = make_tile("p", 50);
	size_t one = ctui_mvt_bytes(&probe);
	ctui_mvt_free(&probe);
	ctui_tilecache c;
	CHECK(ctui_tilecache_init(
	    &c, 100, one * 2 + one / 2)); /* two and a half tiles */
	for (int i = 0; i < 6; i++) {
		ctui_mvt_tile t = make_tile("p", 50);
		CHECK(ctui_tilecache_put(&c, 3, i, 0, &t) != NULL);
		CHECK(c.count <= 2 && c.bytes <= c.max_bytes);
	}
	CHECK(c.count == 2 && ctui_tilecache_get(&c, 3, 5, 0) &&
	      ctui_tilecache_get(&c, 3, 4, 0));
	/* one tile bigger than the whole limit is still kept, alone */
	ctui_mvt_tile big = make_tile("big", 5000);
	CHECK(ctui_mvt_bytes(&big) > c.max_bytes);
	const ctui_mvt_tile *s = ctui_tilecache_put(&c, 3, 9, 9, &big);
	CHECK(s != NULL && c.count == 1 &&
	      ctui_tilecache_get(&c, 3, 9, 9) == s);
	ctui_tilecache_free(&c);
}

int main(void)
{
	RUN(test_basic);
	RUN(test_lru);
	RUN(test_replace);
	RUN(test_byte_limit);
	return CHECK_DONE();
}
