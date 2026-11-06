/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * logview.h - a log viewer: a bounded ring of records, filters, follow mode.
 *
 * Records (a time in seconds given by the caller, a level, a source tag and
 * a message) are pushed into a ring of fixed capacity; once it is full the
 * oldest record is dropped. Text is copied into the ring, so the producer's
 * buffers can go away at once. Control characters and invalid bytes become
 * '?' (line breaks and tabs become spaces), a source keeps
 * CTUI_LOG_SOURCE_MAX - 1 bytes and a message CTUI_LOG_MESSAGE_MAX bytes.
 *
 * Levels are coloured with the theme. A record is visible when its level is
 * on and the text filter, a case-insensitive substring, occurs in its
 * message or source; the matches in messages are highlighted.
 *
 * Follow mode keeps the newest visible record at the bottom. Scrolling up
 * (keys, wheel, scrollbar) turns it off; End or 'f' turns it on again. The
 * view is anchored on a record, not on a row, so it stays put when new
 * records come in or old ones are dropped. A message is cut with the
 * ellipsis, or wrapped on several rows with `wrap`.
 *
 * Geometry: width x height is the whole widget, the last column holding the
 * scrollbar. The gutter shows nothing, the record number or the time of day
 * (UTC, from the seconds). The same width and height must be given to the
 * key, wheel and hit functions as to ctui_logview_draw(); the layout and hit
 * functions are pure.
 *
 * Keys: Up/Down (k/j) one record, PgUp/PgDn a page, Home the oldest, End
 * follow; 't' 'd' 'i' 'w' 'e' toggle the levels trace, debug, info, warn
 * and error; 'f' toggles follow, 'r' wrapping, 'g' cycles the gutter. The
 * '/' filter is up to the caller, who passes the text to
 * ctui_logview_set_filter().
 *
 * Not thread-safe: use one logview from the UI thread only.
 */
#ifndef CTUI_LOGVIEW_H
#define CTUI_LOGVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/scrollbar.h>
#include <ctui/theme.h>

#define CTUI_LOG_SOURCE_MAX 32	  /* bytes of a source tag, incl. NUL */
#define CTUI_LOG_MESSAGE_MAX 4096 /* bytes kept of a message */
#define CTUI_LOG_FILTER_MAX 64	  /* bytes of the text filter, incl. NUL */
#define CTUI_LOG_CAPACITY_MAX (1U << 20U)

typedef enum ctui_log_level {
	CTUI_LOG_TRACE = 0,
	CTUI_LOG_DEBUG,
	CTUI_LOG_INFO,
	CTUI_LOG_WARN,
	CTUI_LOG_ERROR,
	CTUI_LOG_LEVELS
} ctui_log_level;

typedef enum ctui_log_gutter {
	CTUI_LOG_GUTTER_NONE = 0,
	CTUI_LOG_GUTTER_NUMBER, /* sequence number of the record, from 1 */
	CTUI_LOG_GUTTER_TIME	/* HH:MM:SS */
} ctui_log_gutter;

typedef struct ctui_log_record {
	double time; /* seconds, as given */
	ctui_log_level level;
	char source[CTUI_LOG_SOURCE_MAX];
	char *message; /* owned by the ring */
	size_t seq;    /* 0 for the first record ever pushed */
} ctui_log_record;

typedef struct ctui_logview {
	ctui_log_record *ring;
	size_t capacity, head, count; /* head: slot of the oldest record */
	size_t next_seq;	      /* records pushed so far */
	size_t dropped;		      /* records lost to overflow or refused */
	unsigned levels; /* bit i set: level i visible; all by default */
	char filter[CTUI_LOG_FILTER_MAX];
	bool follow; /* true by default */
	bool wrap;   /* false: long messages are cut */
	ctui_log_gutter gutter;
	size_t top_seq; /* anchor of the window while not following */
	bool has_selection;
	size_t selected_seq;
	ctui_scrollbar bar;	   /* as last drawn */
	int oy, ox, width, height; /* placement as last drawn */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_logview;

/* A ring of `capacity` records (0 keeps nothing and refuses every push).
 * False when the capacity exceeds CTUI_LOG_CAPACITY_MAX or memory is short. */
bool ctui_logview_init(ctui_logview *v, size_t capacity);
void ctui_logview_free(ctui_logview *v);

/* Append a record, dropping the oldest when full. False when the ring has
 * no capacity or memory ran out (counted in `dropped`). Out-of-range
 * levels are clamped; NULL strings are empty. */
bool ctui_logview_push(ctui_logview *v, double time, ctui_log_level level,
		       const char *source, const char *message);

/* Remove every record (the settings stay). */
void ctui_logview_clear(ctui_logview *v);

/* Record i of the ring, 0 being the oldest; NULL past the end. */
const ctui_log_record *ctui_logview_at(const ctui_logview *v, size_t i);

/* Levels. */
void ctui_logview_set_level(ctui_logview *v, ctui_log_level level, bool on);
bool ctui_logview_level_on(const ctui_logview *v, ctui_log_level level);
const char *ctui_log_level_name(ctui_log_level level); /* "TRACE" .. "ERROR" */
ctui_pair ctui_log_level_pair(ctui_log_level level);   /* its theme colour */

/* Text filter; NULL or "" shows every record of the visible levels. */
void ctui_logview_set_filter(ctui_logview *v, const char *text);

/* Byte offset of the first case-insensitive (ASCII) occurrence of needle in
 * hay at or after from, on character boundaries; SIZE_MAX when none. An
 * empty needle never matches. */
size_t ctui_log_find(const char *hay, const char *needle, size_t from);

/* "HH:MM:SS" of a time in seconds (UTC), into out of capacity cap. */
void ctui_log_format_time(double seconds, char *out, size_t cap);

/* Whether a record passes the level and text filters. */
bool ctui_logview_matches(const ctui_logview *v, const ctui_log_record *r);

/* Visible records: how many, and record i (0 = oldest); NULL past the end. */
size_t ctui_logview_visible(const ctui_logview *v);
const ctui_log_record *ctui_logview_visible_at(const ctui_logview *v, size_t i);

/* Where things go in a width x height widget (see ctui_logview_layout_for). */
typedef struct ctui_logview_layout {
	int gutter; /* columns of the gutter */
	int prefix; /* columns of level and source, after the gutter */
	int text;   /* columns of the message (at least 1) */
	int body;   /* columns left of the scrollbar */
} ctui_logview_layout;

ctui_logview_layout ctui_logview_layout_for(const ctui_logview *v, int width);

/* Rows taken by a record: 1 when cut, else its wrapped lines. */
size_t ctui_logview_row_count(const ctui_logview *v, const ctui_log_record *r,
			      int width);

/* Index (among the visible records) of the first one shown: the newest page
 * when following, else the anchor clamped so the window stays full. */
size_t ctui_logview_top(const ctui_logview *v, int width, int height);

/* Index of the first record of the last page: the smallest top that shows
 * the newest visible record in full. */
size_t ctui_logview_last_top(const ctui_logview *v, int width, int height);

/* Visible record under (y, x) of a widget placed at (oy, ox); false when
 * none (gutter and text count, the scrollbar column does not). */
bool ctui_logview_hit(const ctui_logview *v, int oy, int ox, int width,
		      int height, int y, int x, size_t *index);

/* Move the window by delta visible records (negative = up); true when it
 * moved. Moving up turns follow off. */
bool ctui_logview_scroll(ctui_logview *v, int width, int height, int delta);

/* Follow on (and the window to the newest) or off. */
void ctui_logview_set_follow(ctui_logview *v, bool follow);

/* The selected record, or NULL (none, or already dropped). */
const ctui_log_record *ctui_logview_selected(const ctui_logview *v);
void ctui_logview_select(ctui_logview *v,
			 const ctui_log_record *r); /* NULL clears */

/* Apply a key; true when it was used. */
bool ctui_logview_key(ctui_logview *v, int key, int width, int height);

/* Pointer events in screen coordinates, valid after a draw. Each returns
 * true when the view took the event. A press selects the record under it or
 * grabs the scrollbar; the wheel moves by `rows` records. */
bool ctui_logview_press(ctui_logview *v, int y, int x);
bool ctui_logview_drag(ctui_logview *v, int y);
bool ctui_logview_release(ctui_logview *v);
bool ctui_logview_wheel(ctui_logview *v, int rows);

void ctui_logview_draw(ctui_logview *v, int y, int x, int width, int height);

#endif /* CTUI_LOGVIEW_H */
