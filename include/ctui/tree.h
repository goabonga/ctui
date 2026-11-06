/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tree.h - a lazily loaded file tree with icons.
 *
 * The tree is a flat array in display order: expanding a directory
 * inserts its children (folders first, then names, case-insensitively)
 * right after it; collapsing removes them. Symbolic links are never
 * followed. The tree is limited to CTUI_TREE_LIMIT entries and
 * CTUI_TREE_DEPTH_LIMIT levels; going over reports an error and leaves the
 * tree unchanged.
 *
 * Icons are Nerd Font glyphs in UTF-8 locales and bracketed letters
 * ("[C]") otherwise, coloured by file type.
 */
#ifndef CTUI_TREE_H
#define CTUI_TREE_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_TREE_LIMIT 10000u
#define CTUI_TREE_DEPTH_LIMIT 64u

typedef struct ctui_tree_entry {
	char *path, *name;
	size_t depth;
	bool directory, symlink, expanded;
	char status; /* one-letter marker set by the caller (e.g. 'M'), ' ' or 0
			for none */
} ctui_tree_entry;

typedef struct ctui_tree {
	ctui_tree_entry *entries;
	size_t count;
	size_t selected, top;
	bool hovering; /* a row under the pointer is drawn like the selection */
	size_t hover;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_tree;

typedef enum ctui_tree_event {
	CTUI_TREE_NONE = 0,
	CTUI_TREE_MOVED,
	CTUI_TREE_TOGGLED,
	CTUI_TREE_OPEN_FILE, /* Enter on a file: its path is
				entries[selected].path */
	CTUI_TREE_ERROR /* the error text was written to the caller's buffer */
} ctui_tree_event;

/* Load root (made absolute) and expand it. */
bool ctui_tree_init(ctui_tree *tree, const char *root, char *error, size_t cap);
void ctui_tree_free(ctui_tree *tree);

/* Expand or collapse the directory at index; a file or link is a no-op. */
bool ctui_tree_toggle(ctui_tree *tree, size_t index, char *error, size_t cap);

/* Apply a key: Up/Down/j/k, Home/End, PgUp/PgDn (by page), Enter/Right
 * to expand or open, Left to collapse or go to the parent folder. */
ctui_tree_event ctui_tree_key(ctui_tree *tree, int key, size_t page,
			      char *error, size_t cap);

/* Scroll so that the selection is inside a window of rows rows. */
void ctui_tree_scroll(ctui_tree *tree, size_t rows);

/* Show the window starting at row top (clamped so the window stays full)
 * and pull the selection into it. */
void ctui_tree_scroll_to(ctui_tree *tree, size_t rows, size_t top);

const char *ctui_tree_icon(const ctui_tree_entry *entry, bool unicode);
ctui_icon_color ctui_tree_icon_color(const ctui_tree_entry *entry);

/* Mark the row at window row `row` (0 is the first visible one) as under the
 * pointer; a negative row, or one past the last entry or past rows, clears
 * it. Returns whether the hovered entry changed. */
bool ctui_tree_hover(ctui_tree *tree, int row, size_t rows);

/* Draw the visible rows in a width x height area; focused keeps the
 * selection highlighted. */
void ctui_tree_draw(const ctui_tree *tree, int y, int x, int width, int height,
		    bool focused);

#endif /* CTUI_TREE_H */
