/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * geo.c - Web Mercator maths, a pan / zoom viewport and tile selection (geo.h).
 */
#include <ctui/geo.h>

#include <math.h>

#define PI 3.14159265358979323846

double ctui_geo_clamp_lat(double lat)
{
	if (lat != lat)
		return 0;
	return lat < -CTUI_GEO_MAX_LAT	? -CTUI_GEO_MAX_LAT
	       : lat > CTUI_GEO_MAX_LAT ? CTUI_GEO_MAX_LAT
					: lat;
}

double ctui_geo_wrap_lon(double lon)
{
	if (!isfinite(lon))
		return 0;
	lon = fmod(lon + 180.0, 360.0);
	if (lon < 0)
		lon += 360.0;
	return lon - 180.0;
}

void ctui_geo_project(double lon, double lat, double *x, double *y)
{
	double rad = ctui_geo_clamp_lat(lat) * PI / 180.0;
	*x = (lon + 180.0) / 360.0;
	*y = (1.0 - asinh(tan(rad)) / PI) / 2.0;
}

void ctui_geo_unproject(double x, double y, double *lon, double *lat)
{
	*lon = x * 360.0 - 180.0;
	*lat = atan(sinh(PI * (1.0 - 2.0 * y))) * 180.0 / PI;
}

void ctui_geo_to_tile(double lon, double lat, int zoom, double *x, double *y)
{
	double n = ldexp(1.0, zoom < 0 ? 0 : zoom);
	ctui_geo_project(lon, lat, x, y);
	*x *= n;
	*y *= n;
}

void ctui_geo_from_tile(double x, double y, int zoom, double *lon, double *lat)
{
	double n = ldexp(1.0, zoom < 0 ? 0 : zoom);
	ctui_geo_unproject(x / n, y / n, lon, lat);
}

void ctui_geo_view_init(ctui_geo_view *v, double min_zoom, double max_zoom)
{
	if (max_zoom < min_zoom)
		max_zoom = min_zoom;
	*v = (ctui_geo_view){
	    .zoom = min_zoom, .min_zoom = min_zoom, .max_zoom = max_zoom};
}

double ctui_geo_fit_zoom(int pw, int ph)
{
	int side = pw < ph ? pw : ph;
	if (side < 1)
		side = 1;
	return log2((double)side / CTUI_GEO_TILE_SIZE);
}

bool ctui_geo_view_set_zoom(ctui_geo_view *v, double zoom)
{
	if (zoom != zoom)
		return false;
	if (zoom < v->min_zoom)
		zoom = v->min_zoom;
	if (zoom > v->max_zoom)
		zoom = v->max_zoom;
	bool changed = zoom != v->zoom;
	v->zoom = zoom;
	return changed;
}

double ctui_geo_view_scale(const ctui_geo_view *v)
{
	return CTUI_GEO_TILE_SIZE * pow(2.0, v->zoom);
}

void ctui_geo_view_to_pixel(const ctui_geo_view *v, double wx, double wy,
			    int pw, int ph, double *px, double *py)
{
	double cx, cy, scale = ctui_geo_view_scale(v);
	ctui_geo_project(v->lon, v->lat, &cx, &cy);
	*px = (wx - cx) * scale + pw / 2.0;
	*py = (wy - cy) * scale + ph / 2.0;
}

void ctui_geo_view_from_pixel(const ctui_geo_view *v, double px, double py,
			      int pw, int ph, double *lon, double *lat)
{
	double cx, cy, scale = ctui_geo_view_scale(v);
	ctui_geo_project(v->lon, v->lat, &cx, &cy);
	double wy = cy + (py - ph / 2.0) / scale;
	wy = wy < 0 ? 0 : wy > 1 ? 1 : wy;
	ctui_geo_unproject(cx + (px - pw / 2.0) / scale, wy, lon, lat);
	*lon = ctui_geo_wrap_lon(*lon);
}

void ctui_geo_view_pan(ctui_geo_view *v, double dx, double dy)
{
	double cx, cy, scale = ctui_geo_view_scale(v);
	ctui_geo_project(v->lon, v->lat, &cx, &cy);
	cx += dx / scale;
	cx -= floor(cx); /* around the antimeridian */
	cy += dy / scale;
	cy = cy < 0 ? 0 : cy > 1 ? 1 : cy;
	ctui_geo_unproject(cx, cy, &v->lon, &v->lat);
}

bool ctui_geo_view_zoom_by(ctui_geo_view *v, double step)
{
	return ctui_geo_view_set_zoom(v, v->zoom + step);
}

bool ctui_geo_view_zoom_at(ctui_geo_view *v, double step, double px, double py,
			   int pw, int ph)
{
	double lon, lat;
	ctui_geo_view_from_pixel(v, px, py, pw, ph, &lon, &lat);
	if (!ctui_geo_view_zoom_by(v, step))
		return false;
	/* Where that place is now, and the shift that brings it back under the
	 * pointer. */
	double wx, wy, qx, qy, scale = ctui_geo_view_scale(v);
	ctui_geo_project(lon, lat, &wx, &wy);
	ctui_geo_view_to_pixel(v, wx, wy, pw, ph, &qx, &qy);
	double dx = qx - px;
	dx -=
	    scale * floor(dx / scale + 0.5); /* the nearest copy of the world */
	ctui_geo_view_pan(v, dx, qy - py);
	return true;
}

int ctui_geo_tile_zoom(const ctui_geo_view *v, int source_max)
{
	int z = (int)floor(v->zoom + 0.5);
	if (source_max < 0)
		source_max = 0;
	return z < 0 ? 0 : z > source_max ? source_max : z;
}

/* Squared distance from the view centre (world units) to the centre of a tile.
 */
static double distance2(const ctui_geo_tile *t, double n, double cx, double cy)
{
	double dx = ((double)t->ux + 0.5) / n - cx,
	       dy = ((double)t->y + 0.5) / n - cy;
	return dx * dx + dy * dy;
}

size_t ctui_geo_tiles(const ctui_geo_view *v, int pw, int ph, int z,
		      ctui_geo_tile *out, size_t cap)
{
	if (z < 0 || z > 30 || pw < 1 || ph < 1)
		return 0;
	double cx, cy, scale = ctui_geo_view_scale(v);
	ctui_geo_project(v->lon, v->lat, &cx, &cy);
	long n = (long)(1UL << (unsigned)z);
	double half_w = pw / 2.0 / scale, half_h = ph / 2.0 / scale;
	long x0 = (long)floor((cx - half_w) * (double)n),
	     x1 = (long)ceil((cx + half_w) * (double)n) - 1;
	long y0 = (long)floor((cy - half_h) * (double)n),
	     y1 = (long)ceil((cy + half_h) * (double)n) - 1;
	if (y0 < 0)
		y0 = 0;
	if (y1 > n - 1)
		y1 = n - 1;
	if (x1 - x0 > 4096) /* absurdly wide: nothing sensible to draw */
		return 0;
	size_t total = 0;
	for (long tx = x0; tx <= x1; tx++)
		for (long ty = y0; ty <= y1; ty++) {
			ctui_geo_tile t = {z, (int)(((tx % n) + n) % n),
					   (int)ty, (int)tx};
			double d = distance2(&t, (double)n, cx, cy);
			size_t kept = total < cap ? total : cap;
			size_t at = kept;
			while (at > 0 &&
			       distance2(&out[at - 1], (double)n, cx, cy) > d)
				at--;
			if (at < cap) {
				for (size_t i = kept < cap ? kept : cap - 1;
				     i > at; i--)
					out[i] = out[i - 1];
				out[at] = t;
			}
			total++;
		}
	return total;
}

void ctui_geo_tile_rect(const ctui_geo_view *v, const ctui_geo_tile *t, int pw,
			int ph, double *x, double *y, double *size)
{
	double n = ldexp(1.0, t->z);
	ctui_geo_view_to_pixel(v, (double)t->ux / n, (double)t->y / n, pw, ph,
			       x, y);
	*size = ctui_geo_view_scale(v) / n;
}
