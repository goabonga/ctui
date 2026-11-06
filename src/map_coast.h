/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * map_coast.h - the embedded coastline (map_coast.c, generated).
 */
#ifndef CTUI_MAP_COAST_H
#define CTUI_MAP_COAST_H

#include <stddef.h>
#include <stdint.h>

/* ctui_map_coast_line_count polylines; line i has ctui_map_coast_lengths[i]
 * points, stored one after the other in ctui_map_coast_points as pairs of
 * longitude, latitude in hundredths of a degree. */
extern const size_t ctui_map_coast_line_count;
extern const uint16_t ctui_map_coast_lengths[];
extern const int16_t ctui_map_coast_points[];

#endif /* CTUI_MAP_COAST_H */
