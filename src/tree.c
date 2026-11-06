/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tree.c - a lazily loaded file tree with icons (tree.h).
 */
#include <ctui/labels.h>
#include <ctui/tree.h>

#include <curses.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <ctui/text.h>

static void entry_free(ctui_tree_entry *e)
{
	free(e->path);
	free(e->name);
}

void ctui_tree_free(ctui_tree *t)
{
	for (size_t i = 0; i < t->count; i++)
		entry_free(&t->entries[i]);
	free(t->entries);
	*t = (ctui_tree){0};
}

static int compare(const void *aa, const void *bb)
{
	const ctui_tree_entry *a = aa, *b = bb;
	if (a->directory != b->directory)
		return a->directory ? -1 : 1;
	int order = strcasecmp(a->name, b->name);
	return order ? order : strcmp(a->name, b->name);
}

bool ctui_tree_toggle(ctui_tree *t, size_t index, char *error, size_t cap)
{
	if (index >= t->count || !t->entries[index].directory ||
	    t->entries[index].symlink)
		return true;
	ctui_tree_entry *parent = &t->entries[index];
	ctui_labels words = ctui_labels_resolve(NULL);
	if (parent->expanded) {
		size_t end = index + 1;
		while (end < t->count && t->entries[end].depth > parent->depth)
			entry_free(&t->entries[end++]);
		memmove(t->entries + index + 1, t->entries + end,
			(t->count - end) * sizeof *parent);
		t->count -= end - index - 1;
		parent->expanded = false;
		if (t->selected > index)
			t->selected = index;
		return true;
	}
	if (parent->depth >= CTUI_TREE_DEPTH_LIMIT) {
		snprintf(error, cap, "%s", words.depth_error);
		return false;
	}
	DIR *dir = opendir(parent->path);
	if (!dir) {
		snprintf(error, cap, "%s: %s", words.access_error,
			 strerror(errno));
		return false;
	}
	ctui_tree children = {0};
	const char *reason = NULL;
	for (;;) {
		errno = 0;
		struct dirent *de = readdir(dir);
		if (!de) {
			if (errno)
				reason = words.read_error;
			break;
		}
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		if (t->count + children.count >= CTUI_TREE_LIMIT) {
			reason = words.size_error;
			break;
		}
		ctui_tree_entry e = {.depth = parent->depth + 1};
		size_t size = strlen(parent->path) + strlen(de->d_name) + 2;
		e.path = malloc(size);
		e.name = strdup(de->d_name);
		if (!e.path || !e.name) {
			entry_free(&e);
			reason = words.no_memory;
			break;
		}
		snprintf(e.path, size, "%s/%s", parent->path, de->d_name);
		struct stat st;
		if (lstat(e.path, &st) < 0) {
			entry_free(&e);
			continue;
		}
		e.directory = S_ISDIR(st.st_mode);
		e.symlink = S_ISLNK(st.st_mode);
		ctui_tree_entry *p =
		    realloc(children.entries, (children.count + 1) * sizeof e);
		if (!p) {
			entry_free(&e);
			reason = words.no_memory;
			break;
		}
		children.entries = p;
		children.entries[children.count++] = e;
	}
	closedir(dir);
	if (reason) {
		snprintf(error, cap, "%s", reason);
		ctui_tree_free(&children);
		return false;
	}
	if (children.count) {
		qsort(children.entries, children.count,
		      sizeof *children.entries, compare);
		ctui_tree_entry *p = realloc(
		    t->entries, (t->count + children.count) * sizeof *p);
		if (!p) {
			snprintf(error, cap, "%s", words.no_memory);
			ctui_tree_free(&children);
			return false;
		}
		t->entries = p;
		memmove(p + index + 1 + children.count, p + index + 1,
			(t->count - index - 1) * sizeof *p);
		memcpy(p + index + 1, children.entries,
		       children.count * sizeof *p);
		t->count += children.count;
	}
	free(children.entries);
	t->entries[index].expanded = true;
	return true;
}

bool ctui_tree_init(ctui_tree *t, const char *root, char *error, size_t cap)
{
	ctui_labels words = ctui_labels_resolve(NULL);
	char *path = realpath(root, NULL);
	if (!path) {
		snprintf(error, cap, "%s: %s", words.access_error,
			 strerror(errno));
		return false;
	}
	struct stat st;
	if (stat(path, &st) < 0 || !S_ISDIR(st.st_mode)) {
		free(path);
		snprintf(error, cap, "%s", words.not_a_folder);
		return false;
	}
	ctui_tree next = {0};
	next.entries = calloc(1, sizeof *next.entries);
	if (!next.entries) {
		free(path);
		snprintf(error, cap, "%s", words.no_memory);
		return false;
	}
	const char *base = strrchr(path, '/');
	next.entries[0] =
	    (ctui_tree_entry){.path = path,
			      .name = strdup(base && base[1] ? base + 1 : path),
			      .directory = true};
	next.count = 1;
	if (!next.entries[0].name) {
		ctui_tree_free(&next);
		snprintf(error, cap, "%s", words.no_memory);
		return false;
	}
	if (!ctui_tree_toggle(&next, 0, error, cap)) {
		ctui_tree_free(&next);
		return false;
	}
	ctui_tree_free(t);
	*t = next;
	return true;
}

typedef struct {
	const char *extension, *icon, *ascii;
	ctui_icon_color color;
} icon_type;

static const icon_type *entry_icon(const ctui_tree_entry *e)
{
	static const icon_type link = {NULL, "", "[@]", CTUI_ICON_CYAN};
	static const icon_type folder = {NULL, "", "[D]", CTUI_ICON_YELLOW};
	static const icon_type open_folder = {NULL, "", "[D]",
					      CTUI_ICON_YELLOW};
	static const icon_type config = {NULL, "", "[=]", CTUI_ICON_GRAY};
	static const icon_type generic = {NULL, "", "[F]", CTUI_ICON_GRAY};
	static const icon_type types[] = {
	    {".c", "", "[C]", CTUI_ICON_BLUE},
	    {".h", "", "[H]", CTUI_ICON_PURPLE},
	    {".cpp", "", "[+]", CTUI_ICON_BLUE},
	    {".hpp", "", "[+]", CTUI_ICON_PURPLE},
	    {".py", "", "[P]", CTUI_ICON_YELLOW},
	    {".js", "", "[J]", CTUI_ICON_YELLOW},
	    {".ts", "", "[T]", CTUI_ICON_BLUE},
	    {".json", "", "[J]", CTUI_ICON_YELLOW},
	    {".md", "", "[M]", CTUI_ICON_CYAN},
	    {".txt", "", "[T]", CTUI_ICON_GRAY},
	    {".sh", "", "[$]", CTUI_ICON_GREEN},
	    {".html", "", "[W]", CTUI_ICON_ORANGE},
	    {".css", "", "[S]", CTUI_ICON_BLUE},
	    {".yml", "", "[Y]", CTUI_ICON_PURPLE},
	    {".yaml", "", "[Y]", CTUI_ICON_PURPLE},
	    {".toml", "", "[=]", CTUI_ICON_GRAY},
	    {".png", "", "[I]", CTUI_ICON_PURPLE},
	    {".jpg", "", "[I]", CTUI_ICON_PURPLE},
	    {".svg", "", "[I]", CTUI_ICON_PURPLE},
	    {".zip", "", "[Z]", CTUI_ICON_ORANGE},
	    {".gz", "", "[Z]", CTUI_ICON_ORANGE},
	    {".o", "", "[B]", CTUI_ICON_GRAY},
	};
	if (e->symlink)
		return &link;
	if (e->directory)
		return e->expanded ? &open_folder : &folder;
	if (!strcmp(e->name, "Makefile") || !strcmp(e->name, "CMakeLists.txt"))
		return &config;
	const char *ext = strrchr(e->name, '.');
	for (size_t i = 0; ext && i < sizeof types / sizeof *types; i++)
		if (!strcasecmp(ext, types[i].extension))
			return &types[i];
	return &generic;
}

const char *ctui_tree_icon(const ctui_tree_entry *e, bool unicode)
{
	const icon_type *icon = entry_icon(e);
	return unicode ? icon->icon : icon->ascii;
}

ctui_icon_color ctui_tree_icon_color(const ctui_tree_entry *e)
{
	return entry_icon(e)->color;
}

void ctui_tree_scroll(ctui_tree *t, size_t rows)
{
	if (rows == 0)
		rows = 1;
	if (t->selected >= t->count)
		t->selected = t->count ? t->count - 1 : 0;
	if (t->selected < t->top)
		t->top = t->selected;
	if (t->selected >= t->top + rows)
		t->top = t->selected - rows + 1;
	size_t max = t->count > rows ? t->count - rows : 0;
	if (t->top > max)
		t->top = max;
}

void ctui_tree_scroll_to(ctui_tree *t, size_t rows, size_t top)
{
	if (rows == 0)
		rows = 1;
	size_t max = t->count > rows ? t->count - rows : 0;
	t->top = top > max ? max : top;
	if (t->selected < t->top)
		t->selected = t->top;
	else if (t->selected >= t->top + rows && t->count)
		t->selected = t->top + rows - 1;
}

ctui_tree_event ctui_tree_key(ctui_tree *t, int key, size_t page, char *error,
			      size_t cap)
{
	if (!t->count)
		return CTUI_TREE_NONE;
	size_t before = t->selected, last = t->count - 1;
	ctui_tree_entry *e = &t->entries[t->selected];
	switch (key) {
	case KEY_DOWN:
	case 'j':
		if (t->selected < last)
			t->selected++;
		break;
	case KEY_UP:
	case 'k':
		if (t->selected)
			t->selected--;
		break;
	case KEY_HOME:
		t->selected = 0;
		break;
	case KEY_END:
		t->selected = last;
		break;
	case KEY_NPAGE:
		t->selected =
		    page > last - t->selected ? last : t->selected + page;
		break;
	case KEY_PPAGE:
		t->selected = page > t->selected ? 0 : t->selected - page;
		break;
	case KEY_LEFT:
	case 'h':
		if (e->directory && e->expanded && t->selected > 0)
			return ctui_tree_toggle(t, t->selected, error, cap)
				   ? CTUI_TREE_TOGGLED
				   : CTUI_TREE_ERROR;
		while (t->selected > 0 &&
		       t->entries[t->selected].depth >= e->depth)
			t->selected--;
		break;
	case KEY_RIGHT:
	case 'l':
	case '\n':
	case KEY_ENTER:
		if (e->directory && !e->symlink)
			return ctui_tree_toggle(t, t->selected, error, cap)
				   ? CTUI_TREE_TOGGLED
				   : CTUI_TREE_ERROR;
		return key == KEY_RIGHT || key == 'l' ? CTUI_TREE_NONE
						      : CTUI_TREE_OPEN_FILE;
	default:
		break;
	}
	return t->selected != before ? CTUI_TREE_MOVED : CTUI_TREE_NONE;
}

bool ctui_tree_hover(ctui_tree *t, int row, size_t rows)
{
	bool was = t->hovering;
	size_t before = t->hover;
	t->hovering =
	    row >= 0 && (size_t)row < rows && t->top + (size_t)row < t->count;
	t->hover = t->hovering ? t->top + (size_t)row : 0;
	return t->hovering != was || (t->hovering && t->hover != before);
}

static void tree_draw_impl(const ctui_tree *t, int y, int x, int width,
			   int height, bool focused)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	const char *open = g->tree_open, *shut = g->tree_shut;
	for (int r = 0; r < height && t->top + (size_t)r < t->count; r++) {
		size_t i = t->top + (size_t)r;
		const ctui_tree_entry *e = &t->entries[i];
		bool selected = (i == t->selected && focused) ||
				(t->hovering && i == t->hover);
		int row = y + r;
		int attr = selected ? ctui_attr(CTUI_PAIR_SELECTION, false)
				    : ctui_attr(CTUI_PAIR_BODY, false);
		ctui_fill(row, x, width, attr);
		int cx = x + 1 + (int)e->depth * 2;
		if (e->directory && !e->symlink)
			ctui_put(row, cx, 1, e->expanded ? open : shut, attr);
		cx += 2;
		const char *icon = ctui_tree_icon(e, g->unicode);
		cx +=
		    ctui_put(row, cx, x + width - cx, icon,
			     ctui_attr_icon(ctui_tree_icon_color(e), selected));
		cx += 1;
		if (cx < x + width)
			ctui_put(row, cx, x + width - cx, e->name, attr);
		if (e->status && e->status != ' ' && width > 4)
			ctui_put(row, x + width - 2, 1, (char[]){e->status, 0},
				 (int)((unsigned)attr | (unsigned)A_BOLD));
	}
}

void ctui_tree_draw(const ctui_tree *t, int y, int x, int width, int height,
		    bool focused)
{
	ctui_style_begin(t->style);
	tree_draw_impl(t, y, x, width, height, focused);
	ctui_style_end();
}
