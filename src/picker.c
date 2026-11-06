/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * picker.c - file and folder chooser dialogs (picker.h).
 */
#include <ctui/picker.h>

#include <curses.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>
#include <ctui/tree.h>

#define BOX_WIDTH 72
#define BOX_HEIGHT 20
#define CHROME_ROWS 5 /* borders, path, rule, hint */
#define LIST_TOP 3    /* rows between the top of the box and the list */
#define SIZE_COLUMNS 9

static const char PARENT[] = "..";
static const char HERE[] = ".";

static void entry_free(ctui_picker_entry *e)
{
	free(e->name);
}

static void entries_free(ctui_picker_entry *entries, size_t count)
{
	for (size_t i = 0; i < count; i++)
		entry_free(&entries[i]);
	free(entries);
}

void ctui_picker_free(ctui_picker *p)
{
	entries_free(p->entries, p->count);
	*p = (ctui_picker){0};
}

static char *copy(const char *s)
{
	size_t n = strlen(s) + 1;
	char *c = malloc(n);
	if (c)
		memcpy(c, s, n);
	return c;
}

static int compare(const void *aa, const void *bb)
{
	const ctui_picker_entry *a = aa, *b = bb;
	if (a->directory != b->directory)
		return a->directory ? -1 : 1;
	int order = strcasecmp(a->name, b->name);
	return order ? order : strcmp(a->name, b->name);
}

/* path + "/" + name without doubling the slash at the root. */
static bool join(char *out, size_t cap, const char *dir, const char *name)
{
	int n = snprintf(out, cap, "%s%s%s", dir, strcmp(dir, "/") ? "/" : "",
			 name);
	return n > 0 && (size_t)n < cap;
}

static bool push(ctui_picker_entry **entries, size_t *count, size_t *capacity,
		 const char *name, bool directory, bool symlink,
		 unsigned long long size)
{
	if (*count == *capacity) {
		size_t bigger = *capacity ? *capacity * 2 : 32;
		ctui_picker_entry *grown =
		    realloc(*entries, bigger * sizeof **entries);
		if (!grown)
			return false;
		*entries = grown;
		*capacity = bigger;
	}
	char *dup = copy(name);
	if (!dup)
		return false;
	(*entries)[(*count)++] =
	    (ctui_picker_entry){dup, directory, symlink, size};
	return true;
}

bool ctui_picker_enter(ctui_picker *p, const char *dir)
{
	char full[CTUI_PICKER_PATH], resolved[PATH_MAX];
	ctui_labels words = ctui_labels_resolve(p->labels);
	if (dir[0] == '/' || !p->path[0])
		snprintf(full, sizeof full, "%s", dir);
	else if (!join(full, sizeof full, p->path, dir))
		goto toolong;
	if (!realpath(full, resolved)) {
		snprintf(p->error, sizeof p->error, "%s %s: %s",
			 words.open_error, dir, strerror(errno));
		return false;
	}
	DIR *d = opendir(resolved);
	if (!d) {
		snprintf(p->error, sizeof p->error, "%s %s: %s",
			 words.open_error, dir, strerror(errno));
		return false;
	}

	ctui_picker_entry *entries = NULL;
	size_t count = 0, capacity = 0;
	bool ok = true;
	if (strcmp(resolved, "/") != 0)
		ok = push(&entries, &count, &capacity, PARENT, true, false, 0);
	if (ok && p->mode != CTUI_PICKER_FILE)
		ok = push(&entries, &count, &capacity, HERE, true, false, 0);
	size_t fixed = count;
	for (;;) {
		struct dirent *de = ok ? readdir(d) : NULL;
		if (de == NULL)
			break;
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		if (de->d_name[0] == '.' && !p->hidden)
			continue;
		char item[CTUI_PICKER_PATH];
		struct stat st, lst;
		if (!join(item, sizeof item, resolved, de->d_name) ||
		    lstat(item, &lst) != 0)
			continue;
		bool symlink = S_ISLNK(lst.st_mode);
		bool directory = false;
		if (stat(item, &st) == 0)
			directory = S_ISDIR(st.st_mode);
		else
			st = lst;
		if (p->mode == CTUI_PICKER_FOLDER && !directory)
			continue;
		ok = push(&entries, &count, &capacity, de->d_name, directory,
			  symlink,
			  directory ? 0 : (unsigned long long)st.st_size);
	}
	closedir(d);
	if (!ok) {
		entries_free(entries, count);
		snprintf(p->error, sizeof p->error, "%s", words.no_memory);
		return false;
	}
	if (count > fixed)
		qsort(entries + fixed, count - fixed, sizeof *entries, compare);

	/* Going up selects the folder we came from. */
	char came_from[NAME_MAX + 1] = "";
	size_t old_len = strlen(p->path), new_len = strlen(resolved);
	if (old_len > new_len && !strncmp(p->path, resolved, new_len))
		snprintf(came_from, sizeof came_from, "%s",
			 p->path + new_len + (strcmp(resolved, "/") ? 1 : 0));
	char *slash = strchr(came_from, '/');
	if (slash)
		*slash = '\0';

	entries_free(p->entries, p->count);
	p->entries = entries;
	p->count = count;
	snprintf(p->path, sizeof p->path, "%s", resolved);
	p->selected = p->mode == CTUI_PICKER_FOLDER
			  ? (count > 1 ? 1 : 0)
			  : (count > fixed ? fixed : 0);
	p->top = 0;
	p->error[0] = '\0';
	if (*came_from)
		for (size_t i = 0; i < count; i++)
			if (!strcmp(entries[i].name, came_from))
				p->selected = i;
	return true;

toolong:
	snprintf(p->error, sizeof p->error, "%s", words.path_too_long);
	return false;
}

bool ctui_picker_open(ctui_picker *p, ctui_picker_mode mode, const char *start)
{
	*p = (ctui_picker){.mode = mode};
	if (ctui_picker_enter(p, start && *start ? start : "."))
		return true;
	return ctui_picker_enter(p, "/");
}

/* Keep the selection inside a window of rows rows. */
static void keep_visible(ctui_picker *p, size_t rows)
{
	if (rows == 0)
		rows = 1;
	if (p->selected < p->top)
		p->top = p->selected;
	if (p->selected >= p->top + rows)
		p->top = p->selected - rows + 1;
}

static ctui_dialog_result accept(ctui_picker *p, const char *name)
{
	if (name ? !join(p->chosen, sizeof p->chosen, p->path, name)
		 : snprintf(p->chosen, sizeof p->chosen, "%s", p->path) >=
		       (int)sizeof p->chosen) {
		snprintf(p->error, sizeof p->error, "%s",
			 ctui_labels_resolve(p->labels).path_too_long);
		return CTUI_DIALOG_OPEN;
	}
	return CTUI_DIALOG_ACCEPTED;
}

static ctui_dialog_result activate(ctui_picker *p)
{
	if (p->selected >= p->count)
		return CTUI_DIALOG_OPEN;
	const ctui_picker_entry *e = &p->entries[p->selected];
	if (!strcmp(e->name, HERE) && p->mode != CTUI_PICKER_FILE)
		return accept(p, NULL);
	if (e->directory) {
		char name[NAME_MAX + 1];
		snprintf(name, sizeof name, "%s",
			 e->name); /* the entry goes away on success */
		ctui_picker_enter(p, name);
		return CTUI_DIALOG_OPEN;
	}
	return accept(p, e->name);
}

ctui_dialog_result ctui_picker_key(ctui_picker *p, int key, size_t rows)
{
	size_t last = p->count ? p->count - 1 : 0;
	size_t jump = rows > 1 ? rows - 1 : 1;
	switch (key) {
	case 27:
	case 'q':
		return CTUI_DIALOG_CANCELLED;
	case KEY_DOWN:
	case 'j':
		if (p->selected < last)
			p->selected++;
		break;
	case KEY_UP:
	case 'k':
		if (p->selected > 0)
			p->selected--;
		break;
	case KEY_HOME:
		p->selected = 0;
		break;
	case KEY_END:
		p->selected = last;
		break;
	case KEY_NPAGE:
		p->selected =
		    p->selected + jump < last ? p->selected + jump : last;
		break;
	case KEY_PPAGE:
		p->selected = p->selected > jump ? p->selected - jump : 0;
		break;
	case '\n':
	case KEY_ENTER:
	case KEY_RIGHT: {
		ctui_dialog_result r = activate(p);
		keep_visible(p, rows);
		return r;
	}
	case ' ':
		if (p->mode != CTUI_PICKER_FILE && p->selected < p->count &&
		    strcmp(p->entries[p->selected].name, PARENT) != 0)
			return !strcmp(p->entries[p->selected].name, HERE)
				   ? accept(p, NULL)
				   : accept(p, p->entries[p->selected].name);
		break;
	case KEY_LEFT:
	case KEY_BACKSPACE:
	case 127:
	case 8:
		ctui_picker_enter(p, PARENT);
		break;
	case '.': {
		char keep[NAME_MAX + 1] = "";
		if (p->selected < p->count)
			snprintf(keep, sizeof keep, "%s",
				 p->entries[p->selected].name);
		p->hidden = !p->hidden;
		char here[CTUI_PICKER_PATH];
		snprintf(here, sizeof here, "%s", p->path);
		ctui_picker_enter(p, here);
		for (size_t i = 0; i < p->count; i++)
			if (!strcmp(p->entries[i].name, keep))
				p->selected = i;
		break;
	}
	default:
		break;
	}
	keep_visible(p, rows);
	return CTUI_DIALOG_OPEN;
}

/* ---- geometry and mouse ------------------------------------------ */

bool ctui_picker_geometry_for(const ctui_picker *p, int lines, int cols,
			      ctui_rect *box)
{
	if (lines < 10 || cols < 40)
		return false;
	int width = p->width ? (p->width < 40 ? 40 : p->width) : BOX_WIDTH;
	int height = p->height ? (p->height < 10 ? 10 : p->height) : BOX_HEIGHT;
	return ctui_dialog_box(lines, cols, width, height, box);
}

bool ctui_picker_geometry(int lines, int cols, ctui_rect *box)
{
	ctui_picker defaults = {0};
	return ctui_picker_geometry_for(&defaults, lines, cols, box);
}

size_t ctui_picker_rows(int box_height)
{
	return box_height > CHROME_ROWS ? (size_t)(box_height - CHROME_ROWS)
					: 1;
}

int ctui_picker_hit(const ctui_picker *p, int lines, int cols, int y, int x)
{
	ctui_rect box;
	if (!ctui_picker_geometry_for(p, lines, cols, &box))
		return -1;
	int row = y - (box.y + LIST_TOP);
	if (row < 0 || row >= (int)ctui_picker_rows(box.height) || x <= box.x ||
	    x >= box.x + box.width - 1)
		return -1;
	size_t i = p->top + (size_t)row;
	return i < p->count ? (int)i : -1;
}

/* Describe the list to the scrollbar and place it in the right margin. */
static void sync_bar(ctui_scrollbar *bar, const ctui_picker *p,
		     const ctui_rect *box)
{
	size_t rows = ctui_picker_rows(box->height);
	ctui_scrollbar_set(bar, p->count, rows, p->top);
	ctui_scrollbar_place(bar, box->y + LIST_TOP, box->x + box->width - 2,
			     (int)rows);
}

/* The bar moved the window: bring the selection into it. */
static void follow_bar(ctui_picker *p, const ctui_rect *box)
{
	size_t rows = ctui_picker_rows(box->height);
	p->top = p->bar.offset;
	if (p->selected < p->top)
		p->selected = p->top;
	else if (p->selected >= p->top + rows)
		p->selected = p->top + rows - 1;
}

bool ctui_picker_drag(ctui_picker *p, int lines, int cols, int y)
{
	ctui_rect box;
	if (!ctui_picker_geometry_for(p, lines, cols, &box))
		return false;
	sync_bar(&p->bar, p, &box);
	if (ctui_scrollbar_drag(&p->bar, y))
		follow_bar(p, &box);
	return p->bar.dragging;
}

bool ctui_picker_release(ctui_picker *p)
{
	return ctui_scrollbar_release(&p->bar);
}

bool ctui_picker_wheel(ctui_picker *p, int lines, int cols, int rows)
{
	ctui_rect box;
	if (!ctui_picker_geometry_for(p, lines, cols, &box))
		return false;
	sync_bar(&p->bar, p, &box);
	if (ctui_scrollbar_wheel(&p->bar, rows))
		follow_bar(p, &box);
	return true;
}

ctui_dialog_result ctui_picker_click(ctui_picker *p, int lines, int cols, int y,
				     int x)
{
	ctui_rect box;
	if (!ctui_picker_geometry_for(p, lines, cols, &box))
		return CTUI_DIALOG_OPEN;
	if (ctui_frame_close_hit(box.y, box.x, box.width, y, x))
		return CTUI_DIALOG_CANCELLED;
	sync_bar(&p->bar, p, &box);
	if (ctui_scrollbar_press(&p->bar, y, x)) {
		follow_bar(p, &box);
		return CTUI_DIALOG_OPEN;
	}
	int i = ctui_picker_hit(p, lines, cols, y, x);
	if (i < 0)
		return CTUI_DIALOG_OPEN;
	size_t rows = ctui_picker_rows(box.height);
	ctui_dialog_result r = CTUI_DIALOG_OPEN;
	if ((size_t)i == p->selected)
		r = activate(p);
	else
		p->selected = (size_t)i;
	keep_visible(p, rows);
	return r;
}

/* ---- drawing ------------------------------------------------------ */

static void size_text(char *out, size_t cap, unsigned long long bytes)
{
	static const char UNITS[] = "KMGT";
	if (bytes < 1024) {
		snprintf(out, cap, "%llu B", bytes);
		return;
	}
	double v = (double)bytes / 1024;
	int unit = 0;
	while (v >= 1024 && unit < 3) {
		v /= 1024;
		unit++;
	}
	snprintf(out, cap, "%.1f %c", v, UNITS[unit]);
}

static void picker_draw_impl(const ctui_picker *p)
{
	ctui_rect box;
	if (!ctui_picker_geometry_for(p, LINES, COLS, &box))
		return;
	bool unicode = ctui_theme_unicode();
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false);
	ctui_labels labels = ctui_labels_resolve(p->labels);
	ctui_frame_draw(box.y, box.x, box.height, box.width, true, true,
			p->mode == CTUI_PICKER_FILE	? labels.file_title
			: p->mode == CTUI_PICKER_FOLDER ? labels.folder_title
							: labels.any_title);
	ctui_frame_close_draw(box.y, box.x, box.width, true);
	int x = box.x + 2, width = box.width - 4;

	/* the path, keeping its tail when it is too long */
	const char *shown = p->path;
	const char *lead = "";
	if ((int)ctui_utf8_columns(shown) > width) {
		lead = ctui_theme_glyphs()->ellipsis;
		int room = width - (int)ctui_utf8_columns(lead);
		while (*shown && (int)ctui_utf8_columns(shown) > room) {
			unsigned long cp;
			shown += ctui_utf8_decode(shown, &cp);
		}
	}
	int used = ctui_put(box.y + 1, x, width, lead,
			    ctui_attr(CTUI_PAIR_DIALOG, true));
	ctui_put(box.y + 1, x + used, width - used, shown,
		 ctui_attr(CTUI_PAIR_DIALOG, true));
	ctui_separator(box.y + 2, x, width,
		       ctui_attr(CTUI_PAIR_DIALOG_BORDER, false));

	size_t rows = ctui_picker_rows(box.height);
	for (size_t r = 0; r < rows; r++) {
		int row = box.y + LIST_TOP + (int)r;
		size_t i = p->top + r;
		bool selected = i == p->selected;
		int attr = selected ? ctui_attr(CTUI_PAIR_SELECTION, true)
				    : ctui_attr(CTUI_PAIR_BODY, false);
		ctui_fill(row, x, width, attr);
		if (i >= p->count)
			continue;
		const ctui_picker_entry *e = &p->entries[i];
		ctui_tree_entry shape = {.name = e->name,
					 .directory = e->directory,
					 .symlink = e->symlink};
		int cx = x + 1;
		cx += ctui_put(row, cx, x + width - cx,
			       ctui_tree_icon(&shape, unicode),
			       ctui_attr_icon(ctui_tree_icon_color(&shape),
					      selected)) +
		      1;
		const char *label = e->name;
		char text[CTUI_PICKER_PATH];
		if (p->mode != CTUI_PICKER_FILE && !strcmp(e->name, HERE)) {
			snprintf(text, sizeof text, "%s", labels.this_folder);
			label = text;
		}
		int name_room =
		    x + width - cx - (e->directory ? 0 : SIZE_COLUMNS);
		ctui_put(row, cx, name_room, label, attr);
		if (!e->directory) {
			char size[24];
			size_text(size, sizeof size, e->size);
			int w = (int)strlen(size);
			ctui_put(row, x + width - 1 - w, w, size, attr);
		}
	}

	ctui_scrollbar bar = p->bar;
	sync_bar(&bar, p, &box);
	ctui_scrollbar_draw(&bar, true);

	int hint = box.y + box.height - 2;
	if (p->error[0]) {
		ctui_put(hint, x, width, p->error,
			 ctui_attr(CTUI_PAIR_NOTICE_ERROR, true));
		return;
	}
	ctui_put(hint, x, width,
		 p->mode == CTUI_PICKER_FILE	 ? labels.file_hint
		 : p->mode == CTUI_PICKER_FOLDER ? labels.folder_hint
						 : labels.any_hint,
		 (int)((unsigned)dialog | (unsigned)A_DIM));
}

void ctui_picker_draw(const ctui_picker *p)
{
	ctui_style_begin(p->style);
	picker_draw_impl(p);
	ctui_style_end();
}
