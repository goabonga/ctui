/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * labels.h - the texts the components draw themselves: button names, dialog
 * titles, key hints.
 *
 * There is one global set, English by default, replaced with
 * ctui_labels_set(). A component that offers an override (a `labels` pointer
 * in its state) uses the fields that are set there and falls back to the
 * global set for the others, so an override only names what differs.
 * Strings must outlive their use. Buttons grow to fit their label.
 *
 * Translations can also come from text files, one per language, loaded
 * from the locale of the machine (ctui_labels_load_locale()). A file holds
 * lines "field = text", where field is a member name of ctui_labels (yes,
 * confirm_title, no_memory...); blank lines and lines starting with '#'
 * are ignored, and so are unknown fields. The library keeps the texts.
 */
#ifndef CTUI_LABELS_H
#define CTUI_LABELS_H

#include <stdbool.h>

typedef struct ctui_labels {
	const char *yes, *no;			   /* confirm dialog */
	const char *cancel, *back, *next, *finish; /* wizard buttons */
	const char *confirm_title, *prompt_title;  /* dialog titles */
	const char *file_title, *folder_title;	   /* pickers */
	const char *any_title;		     /* picker of a file or a folder */
	const char *file_hint, *folder_hint; /* picker key hints */
	const char *any_hint;
	const char *not_available; /* gauge without a reading */
	const char *this_folder;   /* folder picker, first row */
	/* Failures reported in the error text of the tree and the pickers; the
	 * system reason, when there is one, follows after ": ". */
	const char *access_error,
	    *open_error; /* "Unable to access folder", "Cannot open <dir>" */
	const char *read_error, *depth_error;
	const char *size_error, *not_a_folder;
	const char *path_too_long, *no_memory;
	const char *palette_title,
	    *palette_empty; /* command palette: title, no match */
	const char *help_title,
	    *help_hint; /* key-binding help: title, key hint */
} ctui_labels;

/* The built-in English texts. */
ctui_labels ctui_labels_default(void);

/* Replace the global set; fields left NULL keep the built-in text, and NULL
 * restores them all. */
void ctui_labels_set(const ctui_labels *labels);

/* Read a language file and make it the global set (the fields it does not
 * name keep the built-in text). False, with the set unchanged, when the
 * file cannot be read. */
bool ctui_labels_load(const char *path);

/* Load dir/<language>.lang for the locale of the machine, taken from
 * LC_ALL, LC_MESSAGES or LANG: "fr_FR.UTF-8" tries fr_FR.lang then
 * fr.lang. False, with the set unchanged, when the locale is C or POSIX or
 * when no such file exists, so the built-in English stays. */
bool ctui_labels_load_locale(const char *dir);

/* The global set with the fields of override (may be NULL) applied. */
ctui_labels ctui_labels_resolve(const ctui_labels *override);

#endif /* CTUI_LABELS_H */
