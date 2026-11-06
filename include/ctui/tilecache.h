/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tilecache.h - a bounded cache of decoded vector tiles.
 *
 * Tiles are keyed by (z, x, y). The cache holds at most `capacity` tiles
 * and, when max_bytes is not 0, about that many bytes of decoded data
 * (ctui_mvt_bytes()); the least recently used tile is dropped first. A tile
 * just stored is never the one dropped, so a single tile larger than the
 * byte limit still stays.
 *
 * The cache owns the tiles it holds. A pointer it returns stays valid until
 * the next ctui_tilecache_put() or ctui_tilecache_clear().
 */
#ifndef CTUI_TILECACHE_H
#define CTUI_TILECACHE_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/mvt.h>

typedef struct ctui_tilecache_entry {
	int z, x, y;
	ctui_mvt_tile tile;
	size_t bytes;
	unsigned long long stamp; /* last use; 0 = free slot */
} ctui_tilecache_entry;

typedef struct ctui_tilecache {
	ctui_tilecache_entry *entries;
	size_t capacity, count;
	size_t max_bytes, bytes;
	unsigned long long clock;
} ctui_tilecache;

/* False when capacity is 0 or memory runs out. */
bool ctui_tilecache_init(ctui_tilecache *cache, size_t capacity,
			 size_t max_bytes);

/* Free every tile and the cache. */
void ctui_tilecache_free(ctui_tilecache *cache);

/* Drop every tile, keeping the cache usable. */
void ctui_tilecache_clear(ctui_tilecache *cache);

/* The tile, marking it as just used, or NULL. */
const ctui_mvt_tile *ctui_tilecache_get(ctui_tilecache *cache, int z, int x,
					int y);

/* Store a tile, replacing one with the same key and dropping the least
 * recently used ones beyond the limits. The cache takes the tile (*tile is
 * emptied); returns the stored copy. */
const ctui_mvt_tile *ctui_tilecache_put(ctui_tilecache *cache, int z, int x,
					int y, ctui_mvt_tile *tile);

#endif /* CTUI_TILECACHE_H */
