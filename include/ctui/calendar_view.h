/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_view.h - a week view (seven day columns) and a one-day view of
 * the calendar: an hour axis, caller-supplied events drawn as blocks, and a
 * cursor on a day and a time.
 *
 * Like ctui_calendar it never reads the clock: the caller gives today and the
 * current time of day. Events are owned by the caller, read through an
 * array that must outlive the view; events that overlap in time share the
 * width of their column side by side. Times are minutes since midnight
 * (0..1440) of the day an event belongs to; an event does not cross
 * midnight (an end past 1440 is cut, an end not after the start lasts one
 * minute), unless it has a span (ctui_cal_span): then it ends `extra_days`
 * days later, at its end_minute.
 *
 * Layout in a width x height box: one header row (weekday and day number,
 * or the full date in the day view), then rows of `slot_minutes` minutes
 * (30 by default) starting at `top`, scrolled by the wheel or by the
 * cursor. The first 6 columns are the hour axis ("09:00"); the rest is
 * split between 1 or 7 columns. Coordinates are relative to the top-left
 * cell. Years 1 to 9999 are supported.
 *
 * Several days can be selected (`sel`, see calendar_select.h) in the week
 * view as in the month grid: Shift+Left / Shift+Right (or H / L) extend a
 * range from the anchor by a day, Space adds or removes the cursor day,
 * Esc clears the selection (Esc still cancels a grabbed event first). The
 * selection keys never use Shift+Up / Shift+Down, which keep resizing the
 * event under the cursor. With the mouse, a press on a day header then a
 * drag over other headers selects the range, a right click (see
 * ctui_cal_view_toggle_at()) toggles a day, and a press on a header or a slot
 * clears the selection. Selected days have their header on the info tone.
 */
#ifndef CTUI_CALENDAR_VIEW_H
#define CTUI_CALENDAR_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include <ctui/calendar.h>
#include <ctui/calendar_color.h>
#include <ctui/theme.h>

/* How an event repeats. The event's own `day` is the anchor: the series
 * starts there and no day before it occurs. */
typedef enum ctui_cal_repeat {
	CTUI_CAL_REPEAT_NONE = 0,
	CTUI_CAL_REPEAT_DAILY,
	CTUI_CAL_REPEAT_WEEKLY,	 /* on the days of `weekdays` */
	CTUI_CAL_REPEAT_MONTHLY, /* on the day of the month of the anchor */
	CTUI_CAL_REPEAT_YEARLY	 /* on the month and day of the anchor */
} ctui_cal_repeat;

typedef enum ctui_cal_end {
	CTUI_CAL_END_NEVER = 0,
	CTUI_CAL_END_UNTIL, /* `until` is the last day that may occur */
	CTUI_CAL_END_COUNT /* `count` occurrences in all, exceptions included */
} ctui_cal_end;

/* Days a series can leave out (skipped or detached occurrences). It was 8;
 * code that uses the constant needs no change. When the list is full the
 * editing functions report CTUI_CAL_VIEW_FULL and drop nothing. */
#define CTUI_CAL_EXCEPTIONS 32

/* A recurrence rule; all zero means no repeat. */
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
typedef struct ctui_cal_recur {
	ctui_cal_repeat repeat;
	int interval; /* every N days / weeks / months / years; 1..10000, else 1
		       */
	unsigned weekdays; /* weekly: bit 0 Monday ... bit 6 Sunday; 0 = the
			      anchor's weekday */
	ctui_cal_end end;
	long until; /* day number, for CTUI_CAL_END_UNTIL */
	int count;  /* for CTUI_CAL_END_COUNT */
	long exceptions[CTUI_CAL_EXCEPTIONS]; /* days left out of the series */
	int exception_count;
} ctui_cal_recur;

/* How many days an event lasts beyond its own, kept apart from the event in
 * an array parallel to the events (like the recurrence rules), so that the
 * five fields of ctui_cal_event stay as they are. All zero (or no array) is
 * an event that ends on its own day. With extra_days n >= 1 the event
 * starts on `day` at start_minute and ends on day + n at end_minute
 * (exclusive, 1..1440, so it always ends after it starts): a holiday from
 * the 3rd to the 7th is day = the 3rd, 0..1440, extra_days 4. A repeating
 * event keeps its length in every occurrence. n is limited to
 * CTUI_CAL_SPAN_MAX. */
#define CTUI_CAL_SPAN_MAX 366

typedef struct ctui_cal_span {
	int extra_days;
} ctui_cal_span;

typedef struct ctui_cal_event {
	long day;	   /* days since 1970-01-01, see ctui_date_to_days() */
	int start_minute;  /* 0..1439 */
	int end_minute;	   /* exclusive, up to 1440 */
	const char *title; /* may be NULL */
	int pair; /* an ctui_pair for the block; 0 = CTUI_PAIR_DIALOG */
} ctui_cal_event;

typedef enum ctui_cal_view_mode {
	CTUI_CAL_VIEW_WEEK = 0,
	CTUI_CAL_VIEW_DAY
} ctui_cal_view_mode;

/* What an edit of an occurrence of a series applies to, see
 * ctui_cal_view_resolve_scope(). */
typedef enum ctui_cal_scope {
	CTUI_CAL_SCOPE_SERIES =
	    0, /* every event of the series moves or changes */
	CTUI_CAL_SCOPE_OCCURRENCE /* this occurrence only: it is detached from
				     the series */
} ctui_cal_scope;

/* What an event is grabbed for. */
typedef enum ctui_cal_grab {
	CTUI_CAL_GRAB_NONE = 0,
	CTUI_CAL_GRAB_MOVE,
	CTUI_CAL_GRAB_RESIZE,
	CTUI_CAL_GRAB_CREATE /* a mouse drag over empty slots */
} ctui_cal_grab;

typedef enum ctui_cal_edge {
	CTUI_CAL_EDGE_NONE = 0, /* not on an event */
	CTUI_CAL_EDGE_BODY,
	CTUI_CAL_EDGE_TOP,   /* first row: drag to change the start */
	CTUI_CAL_EDGE_BOTTOM /* last row: drag to change the end */
} ctui_cal_edge;

/* State of an edit in progress; read it, change it through the functions. */
typedef struct ctui_cal_edit {
	ctui_cal_grab grab;
	int index; /* the event grabbed */
	bool top;  /* resize: the top edge is the active one */
	ctui_cal_event
	    saved; /* the event and its rule before the edit, to cancel */
	ctui_cal_recur saved_recur;
	ctui_cal_span saved_span;
	long grab_day;	       /* the occurrence grabbed */
	ctui_date cursor_date; /* the cursor before the edit */
	int cursor_minute;
	bool mouse; /* started by a press: keys other than Esc are ignored */
	bool moved; /* the drag changed something */
	int grab_offset; /* move: minutes from the event start to the grabbed
			    row */
	long from_day;	 /* create: where the drag started and where it is */
	int from_minute, to_minute;
} ctui_cal_edit;

typedef struct ctui_cal_view {
	ctui_cal_view_mode mode;
	ctui_date date;	  /* cursor day */
	int minute;	  /* cursor time, the start of its slot */
	int top;	  /* first minute shown, a multiple of the slot */
	int slot_minutes; /* minutes per row, 30 when not 1..720 */
	int week_start;	  /* 0 = Monday ... 6 = Sunday */
	ctui_date today;  /* given by the caller, year 0 for none */
	int now_minute;	  /* current time of day, -1 for none */
	const ctui_cal_event *events;
	int event_count;
	int selected; /* index of the clicked event, -1 for none */
	const char *const *weekday_names; /* 7 names, Monday first, or NULL */
	const ctui_style *style;	  /* may be NULL; set after init */
	const ctui_cal_recur
	    *recurs; /* rule of each event, parallel to events, or NULL */
	/* editing, see ctui_cal_view_set_editable() */
	ctui_cal_event
	    *edit_events; /* the writable events, or NULL when not editable */
	ctui_cal_recur *edit_recurs; /* the writable rules, or NULL */
	int event_capacity;	     /* room for events in the arrays */
	ctui_cal_edit edit;
	int changed;	  /* event of the last editing result (the old index for
			     DELETED) */
	long changed_day; /* its occurrence day */
	int hover; /* event under the pointer, with hover_edge TOP or BOTTOM */
	ctui_cal_edge hover_edge; /* NONE when the pointer is not on an edge */
	ctui_cal_select sel;	  /* the selected days */
	const ctui_cal_span
	    *spans; /* span of each event, parallel to events, or NULL */
	ctui_cal_span *edit_spans; /* the writable spans, or NULL */
	/* occurrence versus series, see ctui_cal_view_resolve_scope() */
	bool ask_scope;	    /* drops of an occurrence of a series wait for the
			       caller's choice */
	bool scope_pending; /* a drop is waiting for
			       ctui_cal_view_resolve_scope() */
	int changed_series; /* DETACHED: the series the occurrence left */
	const ctui_cal_color
	    *colors; /* colour of each event, parallel to events, or NULL */
	ctui_cal_color *edit_colors; /* the writable colours, or NULL */
} ctui_cal_view;

typedef enum ctui_cal_view_event {
	CTUI_CAL_VIEW_NONE = 0,
	CTUI_CAL_VIEW_MOVED,	 /* the cursor moved inside the day */
	CTUI_CAL_VIEW_DATE,	 /* the cursor day changed */
	CTUI_CAL_VIEW_SCROLLED,	 /* only the visible hours changed */
	CTUI_CAL_VIEW_PICKED,	 /* an event was clicked: see `selected` */
	CTUI_CAL_VIEW_ACTIVATED, /* Enter */
	/* editing; the event concerned is `changed`, on the day `changed_day`
	 */
	CTUI_CAL_VIEW_CREATED, /* a new event was added at the end (title NULL)
				*/
	CTUI_CAL_VIEW_GRABBED, /* an event was grabbed to move or resize it */
	CTUI_CAL_VIEW_EVENT_MOVED,  /* the grabbed event changed day or time */
	CTUI_CAL_VIEW_RESIZED,	    /* the grabbed event changed start or end */
	CTUI_CAL_VIEW_DROPPED,	    /* the edit was kept */
	CTUI_CAL_VIEW_CANCELLED,    /* the edit was undone */
	CTUI_CAL_VIEW_DELETE_ASKED, /* x / Del: call ctui_cal_view_delete(v,
				       false) when sure */
	CTUI_CAL_VIEW_DELETE_SERIES_ASKED, /* X: call ctui_cal_view_delete(v,
					      true) when sure */
	CTUI_CAL_VIEW_DELETED,	/* an event was removed; the later ones moved
				   down one place */
	CTUI_CAL_VIEW_EXCEPTED, /* a day was added to the exceptions of a series
				 */
	CTUI_CAL_VIEW_HOVERED, /* the pointer reached or left an edge: redraw */
	CTUI_CAL_VIEW_FULL, /* no room: the capacity or the exceptions are used
			       up */
	CTUI_CAL_VIEW_SELECTED, /* the selected days changed (the cursor day may
				   have moved too) */
	CTUI_CAL_VIEW_ASK_SCOPE, /* an occurrence of a series was dropped with
				  `ask_scope` set: ask the user, then call
				  ctui_cal_view_resolve_scope() (or cancel) */
	CTUI_CAL_VIEW_DETACHED	 /* this occurrence only: `changed` is the new
				  event, `changed_series` the series, which now
				  skips the day */
} ctui_cal_view_event;

typedef enum ctui_cal_view_part {
	CTUI_CAL_VIEW_OUTSIDE = 0,
	CTUI_CAL_VIEW_HEADER, /* a day header */
	CTUI_CAL_VIEW_AXIS,   /* an hour label */
	CTUI_CAL_VIEW_SLOT    /* a cell of the grid */
} ctui_cal_view_part;

/* Show date with the cursor at 08:00 and no events. An invalid date
 * leaves 1970-01-01. */
void ctui_cal_view_init(ctui_cal_view *v, ctui_cal_view_mode mode, int year,
			int month, int day);

/* Give today and the time of day (minutes, -1 for none); an invalid date
 * means no today. */
void ctui_cal_view_set_today(ctui_cal_view *v, int year, int month, int day,
			     int now_minute);

/* Use an array of events (not copied); selected is cleared. */
void ctui_cal_view_set_events(ctui_cal_view *v, const ctui_cal_event *events,
			      int count);

/* Give the recurrence rules, one per event (index i is the rule of events[i]),
 * or NULL for none. Not copied, and not reset by set_events. Occurrences are
 * drawn in every column they fall on with a marker (a circular arrow, "R" in
 * ASCII) at the right of the title, and a click picks the event of the series.
 */
void ctui_cal_view_set_recurs(ctui_cal_view *v, const ctui_cal_recur *recurs);

/* Give the spans, one per event (index i is the span of events[i]), or NULL
 * for none: events that last several days. Not copied, and not reset by
 * set_events. In the week view such an event is drawn in every column of
 * its days, in the day view only the part on the shown day; the cut edges
 * carry a marker (a small triangle, "<" and ">" in ASCII) on the title row. */
void ctui_cal_view_set_spans(ctui_cal_view *v, const ctui_cal_span *spans);

/* Give the colours, one per event (index i is the colour of events[i], see
 * calendar_color.h), or NULL for none: an event with a colour is drawn on its
 * tone instead of its `pair`. Not copied, and not reset by set_events. */
void ctui_cal_view_set_colors(ctui_cal_view *v, const ctui_cal_color *colors);

/* Make the events editable: the arrays (not copied) hold `count` events and
 * have room for `capacity`; recurs may be NULL or be parallel to them. The
 * view then creates, deletes, moves and resizes events in place and keeps
 * event_count, so the caller keeps storage and capacity and reads back
 * what changed (see ctui_cal_view_event). Sets events and recurs; selected is
 * cleared. Deleting an event moves the later ones down. Titles are not
 * copied: the caller sets the title of a created event.
 *
 * Keys of an editable view (besides the navigation keys):
 *   a, n        create an event of one slot at the cursor
 *   x, Del      ask to delete the event under the cursor (an occurrence of
 *               a series is added to its exceptions); X asks to delete the
 *               whole series. The view deletes nothing by itself: the
 *               caller confirms, then calls ctui_cal_view_delete().
 *   Enter, m    grab the event under the cursor to move it: arrows or
 *               hjkl move it by day and slot, Enter drops it, Esc cancels
 *               and restores it. Enter with no event is still ACTIVATED.
 *   r           grab it to resize: Up/Down move the active edge (the end
 *               at first), Left/Right or Tab switch between the end and
 *               the start, Enter drops, Esc cancels
 *   Shift+Up/Down (KEY_SR / KEY_SF, \e[1;2A and \e[1;2B in xterm) move the
 *               end of the event under the cursor one slot, with no mode
 * The event under the cursor is the one that occurs on the cursor day and
 * covers the cursor slot. Editing an occurrence of a series edits the whole
 * series: a move shifts the series by the same days, a resize changes every
 * occurrence. Alt is not used: terminals disagree on its sequences. */
void ctui_cal_view_set_editable(ctui_cal_view *v, ctui_cal_event *events,
				ctui_cal_recur *recurs, int count,
				int capacity);

/* Let an editable view change the spans too (call it after
 * ctui_cal_view_set_editable(); `spans` parallel to its events, room for its
 * capacity), so that it creates, deletes, moves and resizes several-day
 * events. Moving shifts the whole event; resizing its bottom edge changes
 * the end (the day too: the edge goes on into the next column or day with
 * the keys, to the column under the pointer with the mouse) and the top
 * edge the start (the time only, for a repeating event); the end always
 * stays after the start. Recurrence keeps the length of every occurrence;
 * an occurrence is skipped by the day it starts. */
void ctui_cal_view_set_editable_spans(ctui_cal_view *v, ctui_cal_span *spans);

/* Let an editable view keep the colours too (call it after
 * ctui_cal_view_set_editable(); `colors` parallel to its events, room for its
 * capacity): a created event gets no colour, deleting moves the colours down
 * with their events and a detached occurrence copies the colour of its series.
 */
void ctui_cal_view_set_editable_colors(ctui_cal_view *v,
				       ctui_cal_color *colors);

/* Set the colour of event `index` (0 for none, see ctui_cal_color_next());
 * false when the view has no editable colours, the event does not exist or
 * the colour is invalid. */
bool ctui_cal_view_set_color(ctui_cal_view *v, int index, int color);

/* The event that occurs on the cursor day (or lasts through it) and covers
 * the cursor slot (the selected one first), or -1. */
int ctui_cal_view_event_under_cursor(const ctui_cal_view *v);

/* Add an event of one slot at the cursor; it becomes `selected`. */
ctui_cal_view_event ctui_cal_view_create(ctui_cal_view *v, int height);

/* Delete the event under the cursor: a whole series (series true) or a
 * single event is removed (DELETED), one occurrence of a series is added to
 * its exceptions (EXCEPTED). */
ctui_cal_view_event ctui_cal_view_delete(ctui_cal_view *v, bool series);

/* Grab the event under the cursor to move it (resize false) or to resize
 * it; ctui_cal_view_drop() keeps the change, ctui_cal_view_cancel() restores
 * the event and the cursor. Both return NONE when nothing is grabbed. */
ctui_cal_view_event ctui_cal_view_grab(ctui_cal_view *v, bool resize);
ctui_cal_view_event ctui_cal_view_drop(ctui_cal_view *v);
ctui_cal_view_event ctui_cal_view_cancel(ctui_cal_view *v, int height);

/* Occurrence versus series. An edit that moves or resizes an occurrence of
 * a series changes the whole series by default. With `ask_scope` set the
 * drop (Enter, or the mouse release) of such an edit changes nothing yet and
 * returns CTUI_CAL_VIEW_ASK_SCOPE: the view keeps the edit pending (keys and
 * the mouse are ignored, Esc or ctui_cal_view_cancel() undo it) until the
 * caller asks the user and calls ctui_cal_view_resolve_scope():
 *  - CTUI_CAL_SCOPE_SERIES keeps the series as edited (DROPPED);
 *  - CTUI_CAL_SCOPE_OCCURRENCE puts the series back as it was, appends a new
 *    ordinary event (a copy of the series event, title and pair included,
 *    with the new day and times and no rule) and adds the day the occurrence
 *    had to the exceptions of the series (DETACHED: `changed` is the new
 *    event, `changed_series` the series, `selected` the new event).
 * When the array has no room or the exceptions are full the result is
 * CTUI_CAL_VIEW_FULL and nothing changes: the choice is still pending, so
 * the caller may resolve it with the series scope or cancel. NONE when
 * nothing is pending. Events that are not series never ask. */
ctui_cal_view_event
ctui_cal_view_resolve_scope(ctui_cal_view *v, ctui_cal_scope scope, int height);

/* Number of columns: 7 in the week view, 1 in the day view. */
int ctui_cal_view_columns(const ctui_cal_view *v);

/* Day number (ctui_date_to_days) of the first column, and of column col. */
long ctui_cal_view_first_day(const ctui_cal_view *v);
long ctui_cal_view_day_of(const ctui_cal_view *v, int col);

/* Rows of the grid below the header in a box of that height. */
int ctui_cal_view_rows(int height);

/* Round the cursor and the top to the slot and scroll so that the cursor
 * row is visible in a box of that height. */
void ctui_cal_view_reveal(ctui_cal_view *v, int height);

/* Lay out the events of one day: for every event, its lane (0..) and the
 * number of lanes shared by the group of events it overlaps. Both are -1
 * for events of other days. Arrays hold `count` ints. Returns the number
 * of events of that day. */
int ctui_cal_layout(const ctui_cal_event *events, int count, long day,
		    int *lane, int *lanes);

/* Same as ctui_cal_layout() for events with rules (recurs may be NULL). */
int ctui_cal_layout_recur(const ctui_cal_event *events,
			  const ctui_cal_recur *recurs, int count, long day,
			  int *lane, int *lanes);

/* Same again for events with spans (recurs and spans may be NULL): an event
 * that lasts several days takes its part of the day, from its start time on
 * the first, all day between, up to its end time on the last. */
int ctui_cal_layout_span(const ctui_cal_event *events,
			 const ctui_cal_recur *recurs,
			 const ctui_cal_span *spans, int count, long day,
			 int *lane, int *lanes);

/* Where an event is drawn in a width x height box: row, column, rows and
 * columns, clipped to the visible hours. For an event that repeats, the first
 * occurrence shown. False when it is not visible. */
bool ctui_cal_view_event_rect(const ctui_cal_view *v, int width, int height,
			      int index, int *y, int *x, int *h, int *w);

/* What is at (y, x); *day (a day number) and *minute (start of the slot)
 * are set when the part has them, and may be NULL. */
ctui_cal_view_part ctui_cal_view_hit(const ctui_cal_view *v, int width,
				     int height, int y, int x, long *day,
				     int *minute);

/* Index of the event drawn at (y, x), or -1. */
int ctui_cal_view_event_at(const ctui_cal_view *v, int width, int height, int y,
			   int x);

/* ---- editing helpers (pure) ----
 *
 * Moves and resizes snap to the slot, never leave the day (0..1440) and
 * never produce an event that ends at or before its start. */

/* Which part of an event block a row is: `row` 0..rows-1 inside a block of
 * `rows` rows. A block of one row is all body; of two rows, the last row is
 * the bottom edge; from three rows, the first is the top edge and the last
 * the bottom edge. An edge that is cut by the visible hours (top_cut,
 * bottom_cut) is body. */
ctui_cal_edge ctui_cal_edge_of(int row, int rows, bool top_cut,
			       bool bottom_cut);

/* The part of the event drawn at (y, x) in a width x height box; *index
 * and *day (the occurrence) are set when it is on an event, both may be
 * NULL. */
ctui_cal_edge ctui_cal_view_event_part(const ctui_cal_view *v, int width,
				       int height, int y, int x, int *index,
				       long *day);

/* Where an event of `duration` minutes lands when asked to start at `start`
 * on `day`: the start snapped down to the slot and kept so that the event
 * ends by midnight, the day kept in first..last. */
void ctui_cal_move_clamp(int duration, long day, int start, int slot,
			 long first, long last, long *out_day, int *out_start);

/* Move the top edge (top true) or the bottom edge of e to `minute`, snapped
 * down to the slot, leaving at least one slot (less only at midnight). True
 * when the event changed. */
bool ctui_cal_event_resize(ctui_cal_event *e, bool top, int minute, int slot);

/* The range of slots from the one holding minute a to the one holding b
 * (either order) as start and end; false when it is not inside the day. */
bool ctui_cal_range(int a, int b, int slot, int *start, int *end);

/* Move the cursor to a date and a time (minute -1 keeps the time). Dates
 * outside the supported years are refused. */
ctui_cal_view_event ctui_cal_view_goto(ctui_cal_view *v, int year, int month,
				       int day, int minute);

/* Left/Right/h/l: a day, Up/Down/k/j: a slot, PgUp/PgDn: a week (a day in
 * the day view), Home: today and the current time (when given), Enter.
 * The view scrolls to keep the cursor visible in a box of that height. */
ctui_cal_view_event ctui_cal_view_key(ctui_cal_view *v, int key, int height);

/* Left press: a header or a cell moves the cursor and clears the selected
 * days, an event is picked, an hour label moves the time. A press on a
 * header arms the selection drag. */
ctui_cal_view_event ctui_cal_view_press(ctui_cal_view *v, int width, int height,
					int y, int x);

/* Mouse editing of an editable view, after a press (see ctui_cal_view_press):
 *  - a press on an event body then drag moves it to another slot or day
 *    (the grab keeps the row where it was taken); on the top or bottom edge
 *    (ctui_cal_view_event_part) it changes the start or the end, in the day of
 *    the event; both give EVENT_MOVED / RESIZED while dragging
 *  - a press on an empty slot then drag over other slots in the same day
 *    creates an event over the range when released (CREATED, or FULL); a
 *    plain click only moves the cursor
 *  - release keeps it (DROPPED, NONE when nothing changed);
 * ctui_cal_view_cancel() (right button, Esc) restores the event. Coordinates
 * may lie outside the box while dragging: they are clamped to the grid. A drag
 * that began on a day header selects the days from it (SELECTED). */
ctui_cal_view_event ctui_cal_view_drag(ctui_cal_view *v, int width, int height,
				       int y, int x);
ctui_cal_view_event ctui_cal_view_release(ctui_cal_view *v, int width,
					  int height, int y, int x);

/* True while a press is being dragged: an edit (the mouse editing above) or
 * a selection drag over the day headers. Then ctui_cal_view_drag() and
 * ctui_cal_view_release() apply, else ctui_cal_view_hover(). */
bool ctui_cal_view_dragging(const ctui_cal_view *v);

/* Toggle the day under (y, x), a header or a cell, in the selected days and
 * move the cursor to it (the demo binds it to the right button). SELECTED,
 * or NONE outside the days. */
ctui_cal_view_event ctui_cal_view_toggle_at(ctui_cal_view *v, int width,
					    int height, int y, int x);

/* Pointer moved with no button down: remember the edge under it (hover,
 * hover_edge) so that it is drawn marked. HOVERED when that changed. */
ctui_cal_view_event ctui_cal_view_hover(ctui_cal_view *v, int width, int height,
					int y, int x);

/* Wheel: scroll by `rows` rows (negative = earlier hours); the cursor stays. */
ctui_cal_view_event ctui_cal_view_wheel(ctui_cal_view *v, int height, int rows);

/* Add an event from the absolute minute `start` to `end` (see
 * ctui_cal_event_set_range(): it may last several days when the view has
 * editable spans), with the repeat rule `rule` when not NULL and the view
 * has editable rules. It becomes `selected` (CREATED), FULL when there is no
 * room, NONE when the range is invalid or the view is not editable. */
ctui_cal_view_event ctui_cal_view_create_range(ctui_cal_view *v, long start,
					       long end,
					       const ctui_cal_recur *rule);

/* ---- recurrence (pure functions on the event) ----
 *
 * Occurrences are the days >= e->day that match the rule, within years 1
 * to 9999, up to `until` or the `count`-th match, minus the exceptions.
 * An anchor outside those years has no occurrence. Monthly on the 29th, 30th or
 * 31st skips the months without that day; yearly on February 29 only occurs in
 * leap years. Weekly counts weeks from the Monday week of the anchor, whatever
 * week_start the view has. The rule is kept apart from the event, in an array
 * parallel to the events (ctui_cal_view_set_recurs()); a NULL rule or
 * CTUI_CAL_REPEAT_NONE means a single event, which occurs on its own day only.
 */

/* Does the event occur on that day? */
bool ctui_cal_event_occurs(const ctui_cal_event *e, const ctui_cal_recur *r,
			   long day);

/* The first occurrence after `after`; false when there is none. */
bool ctui_cal_event_next(const ctui_cal_event *e, const ctui_cal_recur *r,
			 long after, long *next);

/* The occurrences in first..last (inclusive): up to `max` days are written
 * to `days` (may be NULL), the number found in all is returned. */
int ctui_cal_event_expand(const ctui_cal_event *e, const ctui_cal_recur *r,
			  long first, long last, long *days, int max);

/* ---- detaching an occurrence (pure function) ----
 *
 * Why a detach can fail. */
typedef enum ctui_cal_detach_result {
	CTUI_CAL_DETACH_OK = 0,
	CTUI_CAL_DETACH_NO_ROOM,       /* the events array is full */
	CTUI_CAL_DETACH_NO_EXCEPTIONS, /* the series already has
					  CTUI_CAL_EXCEPTIONS exceptions */
	CTUI_CAL_DETACH_INVALID /* not a series, no such occurrence, or a range
				   that is invalid */
} ctui_cal_detach_result;

/* Detach the occurrence of events[index] that starts on day `occ`: a new
 * ordinary event (a copy of the series event with its title and pair, no
 * rule) from the absolute minute `start` to `end` (see
 * ctui_cal_event_set_range(): it may last several days when spans is not NULL)
 * is appended at events[*count], and `occ` is added to the exceptions of the
 * series. The arrays are parallel; recurs is required, spans may be NULL (the
 * range must then fit one day), *count is increased and *out_index (may be
 * NULL) is the new event. Nothing changes unless the result is OK. */
ctui_cal_detach_result ctui_cal_detach(ctui_cal_event *events,
				       ctui_cal_recur *recurs,
				       ctui_cal_span *spans, int *count,
				       int capacity, int index, long occ,
				       long start, long end, int *out_index);

/* ---- events lasting several days (pure functions) ----
 *
 * `sp` is the span of the event (NULL or extra_days 0: one day). Times
 * below are absolute minutes since 1970-01-01 00:00 (day * 1440 + minute).
 * Everything stays in years 1 to 9999. */

/* extra_days of sp limited to 0..CTUI_CAL_SPAN_MAX; 0 for NULL. */
int ctui_cal_span_extra(const ctui_cal_span *sp);

/* Start of the event (its first occurrence), and its end (exclusive). */
long ctui_cal_event_start_abs(const ctui_cal_event *e);
long ctui_cal_event_end_abs(const ctui_cal_event *e, const ctui_cal_span *sp);

/* Set day, start_minute, end_minute and the span of e (the title and the
 * pair are kept) from an absolute start and end. False, and e unchanged,
 * when end <= start, when the days leave years 1 to 9999 or when it lasts
 * more than CTUI_CAL_SPAN_MAX + 1 days. An end at a midnight is the end
 * (1440) of the day before. */
bool ctui_cal_event_set_range(ctui_cal_event *e, ctui_cal_span *sp, long start,
			      long end);

/* The minutes of day `day` taken by the occurrence of e that starts on
 * `occ` (clipped to the day, 0..1440): the whole of the event for a single
 * day (same clipping as the layout: an end not after the start lasts a
 * minute), from start_minute on the first day, up to end_minute on the
 * last, all day between. False when the occurrence does not cover the day. */
bool ctui_cal_event_segment(const ctui_cal_event *e, const ctui_cal_span *sp,
			    long occ, long day, int *start, int *end);

/* Does an occurrence cover that day (it starts on it, or up to extra_days
 * before)? *occ is the start day of the latest such occurrence (may be
 * NULL). Same as ctui_cal_event_occurs() for a single-day event. */
bool ctui_cal_event_covers(const ctui_cal_event *e, const ctui_cal_recur *r,
			   const ctui_cal_span *sp, long day, long *occ);

/* The start days of the occurrences that touch first..last (inclusive),
 * that is that start between first - extra_days and last: up to `max` are
 * written to `days` (may be NULL), the number found in all is returned. */
int ctui_cal_event_expand_span(const ctui_cal_event *e, const ctui_cal_recur *r,
			       const ctui_cal_span *sp, long first, long last,
			       long *days, int max);

/* The days of the month covered by events that last several days (extra_days
 * above 0), as a bitmask (bit d-1 is day d); recurs and spans may be NULL. */
uint32_t ctui_cal_span_month_mask(const ctui_cal_event *events,
				  const ctui_cal_recur *recurs,
				  const ctui_cal_span *spans, int count,
				  int year, int month);

/* Move the top edge (top true) or the bottom edge of a several-day event to
 * the absolute minute `at`, snapped down to the slot of its day. The start
 * stays before the end by a slot (less inside the last day's first slot),
 * the end after the start by a slot or up to the end of the start day; the
 * event may shrink to a single day. With keep_day the start stays on its
 * day and only takes the time of `at` (for a repeating event, whose
 * occurrence days are set by the rule).
 * True when the event changed. */
bool ctui_cal_event_resize_range(ctui_cal_event *e, ctui_cal_span *sp, bool top,
				 long at, int slot, bool keep_day);

/* Like ctui_cal_move_clamp() for an event that lasts `duration` minutes
 * across midnight: the start snapped down to the slot within the day, the
 * day kept in first..last and early enough that the end stays in year 9999. */
void ctui_cal_move_clamp_span(long duration, long day, int start, int slot,
			      long first, long last, long *out_day,
			      int *out_start);

/* ---- placing an event on the selected days (pure functions) ----
 *
 * Two ways to put one event on a selection of days (calendar_select.h):
 *
 *  A. repeated: the same event on every selected day. When the days are the
 *     first occurrences of one weekly rule the result is SERIES: a single
 *     weekly rule anchored on the first selected day, whose weekday mask
 *     holds the weekdays of the selection and whose interval is the gap in
 *     weeks between the weeks that have selected days. Mon 2 and Thu 5
 *     November give "weekly on Mon and Thu", Mon 2 and Mon 16 give "every
 *     2 weeks on Mon", Thu 5, Mon 9, Thu 12 give Mon and Thu from Thu 5.
 *     The rule has no end: it goes on after the last selected day (set
 *     rule.end to stop it). Days that are not the first occurrences of
 *     such a rule (three dates in different weeks with uneven gaps, a
 *     selection in which a weekday is missing from a later week) give EACH:
 *     one separate event on every selected day, no rule.
 *  B. one event over the selection: when the days are consecutive, a single
 *     event that lasts all of them (SPAN, see ctui_cal_span).
 *  C. each day: one separate event on every selected day, whatever the
 *     days, with no rule, so it does not go on after the last of them and
 *     does not last from the first to the last (EACH, ctui_cal_plan_each()).
 *     This is the one to choose for "10:00 to 11:00 on the 1st to the 5th".
 */
typedef enum ctui_cal_sel_kind {
	CTUI_CAL_SEL_NONE = 0, /* no day selected */
	CTUI_CAL_SEL_SINGLE,   /* one day */
	CTUI_CAL_SEL_RANGE,    /* two or more consecutive days */
	CTUI_CAL_SEL_WEEKLY,   /* not consecutive, the first occurrences of a
				  weekly rule */
	CTUI_CAL_SEL_ARBITRARY /* anything else */
} ctui_cal_sel_kind;

ctui_cal_sel_kind ctui_cal_select_classify(const ctui_cal_select *s);

typedef enum ctui_cal_place_kind {
	CTUI_CAL_PLACE_NONE = 0, /* nothing to place (no day, or no such way for
				    this selection) */
	CTUI_CAL_PLACE_SINGLE,	 /* one day: one plain event */
	CTUI_CAL_PLACE_SERIES, /* one event with the weekly `rule`, anchored on
				  `first` */
	CTUI_CAL_PLACE_EACH,   /* fallback: one event per selected day, `events`
				  of them */
	CTUI_CAL_PLACE_SPAN /* one event from `first` lasting `days` consecutive
			       days (ctui_cal_span) */
} ctui_cal_place_kind;

typedef struct ctui_cal_place {
	ctui_cal_place_kind kind;
	long first; /* first selected day */
	int days;   /* selected days */
	int events; /* events the choice creates: 1, or `days` for EACH */
	ctui_cal_recur rule; /* SERIES only */
} ctui_cal_place;

/* Way A for the selection. */
ctui_cal_place ctui_cal_plan_repeat(const ctui_cal_select *s);

/* Way C for the selection: one separate event on every selected day and
 * nothing else, no rule and no span: EACH for two days or more, SINGLE for
 * one, NONE for an empty selection. */
ctui_cal_place ctui_cal_plan_each(const ctui_cal_select *s);

/* Way B for the selection: SPAN for consecutive days (two or more), SINGLE
 * for one day, NONE for an empty or a non-consecutive selection. */
ctui_cal_place ctui_cal_plan_span(const ctui_cal_select *s);

/* Put a new event on the selected days of the view as the plan says (see
 * ctui_cal_plan_repeat() and ctui_cal_plan_span()), from start_minute to
 * end_minute of the day (of the last day for SPAN: 0 and 1440 make it last
 * whole days). SINGLE: one event on the day; SERIES: one event with the
 * plan's rule on its first day; EACH: one event per selected day, all or
 * none; SPAN: one event over the consecutive days. `selected` and `changed`
 * are the first event created (the others follow it), CREATED on success,
 * FULL when there is not room for all of them, NONE for a plan of kind NONE,
 * an end not after the start or a view that is not editable. Titles are not
 * set. SERIES and SPAN need editable rules or spans (else NONE). */
ctui_cal_view_event ctui_cal_view_place(ctui_cal_view *v,
					const ctui_cal_place *plan,
					int start_minute, int end_minute);

void ctui_cal_view_draw(const ctui_cal_view *v, int y, int x, int width,
			int height);

#endif /* CTUI_CALENDAR_VIEW_H */
