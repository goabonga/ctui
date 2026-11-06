/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tilecache.c - a bounded cache of decoded vector tiles (tilecache.h).
 */
#include <ctui/tilecache.h>

#include <stdlib.h>

bool ctui_tilecache_init(ctui_tilecache *cache, size_t capacity,
			 size_t max_bytes)
{
	*cache = (ctui_tilecache){0};
	if (capacity == 0)
		return false;
	cache->entries = calloc(capacity, sizeof *cache->entries);
	if (!cache->entries)
		return false;
	cache->capacity = capacity;
	cache->max_bytes = max_bytes;
	return true;
}

static void drop(ctui_tilecache *cache, ctui_tilecache_entry *e)
{
	ctui_mvt_free(&e->tile);
	cache->bytes -= e->bytes;
	cache->count--;
	*e = (ctui_tilecache_entry){0};
}

void ctui_tilecache_clear(ctui_tilecache *cache)
{
	for (size_t i = 0; i < cache->capacity; i++)
		if (cache->entries[i].stamp)
			drop(cache, &cache->entries[i]);
}

void ctui_tilecache_free(ctui_tilecache *cache)
{
	if (cache->entries)
		ctui_tilecache_clear(cache);
	free(cache->entries);
	*cache = (ctui_tilecache){0};
}

static ctui_tilecache_entry *find(ctui_tilecache *cache, int z, int x, int y)
{
	for (size_t i = 0; i < cache->capacity; i++) {
		ctui_tilecache_entry *e = &cache->entries[i];
		if (e->stamp && e->z == z && e->x == x && e->y == y)
			return e;
	}
	return NULL;
}

/* The least recently used tile other than `keep`. */
static ctui_tilecache_entry *oldest(ctui_tilecache *cache,
				    const ctui_tilecache_entry *keep)
{
	ctui_tilecache_entry *best = NULL;
	for (size_t i = 0; i < cache->capacity; i++) {
		ctui_tilecache_entry *e = &cache->entries[i];
		if (e->stamp && e != keep && (!best || e->stamp < best->stamp))
			best = e;
	}
	return best;
}

const ctui_mvt_tile *ctui_tilecache_get(ctui_tilecache *cache, int z, int x,
					int y)
{
	ctui_tilecache_entry *e = cache->entries ? find(cache, z, x, y) : NULL;
	if (!e)
		return NULL;
	e->stamp = ++cache->clock;
	return &e->tile;
}

const ctui_mvt_tile *ctui_tilecache_put(ctui_tilecache *cache, int z, int x,
					int y, ctui_mvt_tile *tile)
{
	if (!cache->entries)
		return NULL;
	ctui_tilecache_entry *e = find(cache, z, x, y);
	if (e)
		drop(cache, e);
	if (cache->count == cache->capacity)
		drop(cache, oldest(cache, NULL));
	for (size_t i = 0; i < cache->capacity; i++)
		if (!cache->entries[i].stamp) {
			e = &cache->entries[i];
			break;
		}
	e->z = z;
	e->x = x;
	e->y = y;
	e->tile = *tile;
	*tile = (ctui_mvt_tile){0};
	e->bytes = ctui_mvt_bytes(&e->tile);
	e->stamp = ++cache->clock;
	cache->bytes += e->bytes;
	cache->count++;
	while (cache->max_bytes && cache->bytes > cache->max_bytes) {
		ctui_tilecache_entry *old = oldest(cache, e);
		if (!old)
			break;
		drop(cache, old);
	}
	return &e->tile;
}
