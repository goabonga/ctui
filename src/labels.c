/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * labels.c - the global label set and its per-component overrides.
 */
#include <ctui/labels.h>

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ctui_labels ctui_labels_default(void)
{
	return (ctui_labels){
	    .yes = "Yes",
	    .no = "No",
	    .cancel = "Cancel",
	    .back = "Back",
	    .next = "Next",
	    .finish = "Finish",
	    .confirm_title = "Confirm",
	    .prompt_title = "Input",
	    .file_title = "Open file",
	    .folder_title = "Select folder",
	    .any_title = "Open file or folder",
	    .file_hint = "Enter: open  Backspace: up  .: hidden  Esc: cancel",
	    .folder_hint = "Enter: open  Space: choose  Backspace: up  .: "
			   "hidden  Esc: cancel",
	    .any_hint = "Enter: open  Space: choose  Backspace: up  .: hidden  "
			"Esc: cancel",
	    .not_available = "N/A",
	    .this_folder = ". (choose this folder)",
	    .access_error = "Unable to access folder",
	    .open_error = "Cannot open",
	    .read_error = "Error reading folder",
	    .depth_error = "Maximum tree depth reached",
	    .size_error = "Tree too large (maximum 10000 entries)",
	    .not_a_folder = "Select a folder",
	    .path_too_long = "Path too long",
	    .no_memory = "Out of memory",
	    .palette_title = "Command palette",
	    .palette_empty = "No matching command",
	    .help_title = "Keyboard shortcuts",
	    .help_hint = "Up/Down: scroll  Esc: close",
	};
}

static ctui_labels g_labels; /* valid once g_set */
static bool g_set;

static const char *pick(const char *wanted, const char *fallback)
{
	return wanted ? wanted : fallback;
}

static ctui_labels overlay(const ctui_labels *over, const ctui_labels *base)
{
	if (!over)
		return *base;
	return (ctui_labels){
	    .yes = pick(over->yes, base->yes),
	    .no = pick(over->no, base->no),
	    .cancel = pick(over->cancel, base->cancel),
	    .back = pick(over->back, base->back),
	    .next = pick(over->next, base->next),
	    .finish = pick(over->finish, base->finish),
	    .confirm_title = pick(over->confirm_title, base->confirm_title),
	    .prompt_title = pick(over->prompt_title, base->prompt_title),
	    .file_title = pick(over->file_title, base->file_title),
	    .folder_title = pick(over->folder_title, base->folder_title),
	    .any_title = pick(over->any_title, base->any_title),
	    .file_hint = pick(over->file_hint, base->file_hint),
	    .folder_hint = pick(over->folder_hint, base->folder_hint),
	    .any_hint = pick(over->any_hint, base->any_hint),
	    .not_available = pick(over->not_available, base->not_available),
	    .this_folder = pick(over->this_folder, base->this_folder),
	    .access_error = pick(over->access_error, base->access_error),
	    .open_error = pick(over->open_error, base->open_error),
	    .read_error = pick(over->read_error, base->read_error),
	    .depth_error = pick(over->depth_error, base->depth_error),
	    .size_error = pick(over->size_error, base->size_error),
	    .not_a_folder = pick(over->not_a_folder, base->not_a_folder),
	    .path_too_long = pick(over->path_too_long, base->path_too_long),
	    .no_memory = pick(over->no_memory, base->no_memory),
	    .palette_title = pick(over->palette_title, base->palette_title),
	    .palette_empty = pick(over->palette_empty, base->palette_empty),
	    .help_title = pick(over->help_title, base->help_title),
	    .help_hint = pick(over->help_hint, base->help_hint),
	};
}

static char *g_store; /* the texts of the file the global set came from */

static void apply(const ctui_labels *labels)
{
	ctui_labels defaults = ctui_labels_default();
	g_labels = overlay(labels, &defaults);
	g_set = true;
}

void ctui_labels_set(const ctui_labels *labels)
{
	apply(labels);
	free(g_store);
	g_store = NULL;
}

#define FIELD(name) {#name, offsetof(ctui_labels, name)}

static const struct {
	const char *name;
	size_t offset;
} FIELDS[] = {
    FIELD(yes),		  FIELD(no),
    FIELD(cancel),	  FIELD(back),
    FIELD(next),	  FIELD(finish),
    FIELD(confirm_title), FIELD(prompt_title),
    FIELD(file_title),	  FIELD(folder_title),
    FIELD(any_title),	  FIELD(file_hint),
    FIELD(folder_hint),	  FIELD(any_hint),
    FIELD(not_available), FIELD(this_folder),
    FIELD(access_error),  FIELD(open_error),
    FIELD(read_error),	  FIELD(depth_error),
    FIELD(size_error),	  FIELD(not_a_folder),
    FIELD(path_too_long), FIELD(no_memory),
    FIELD(palette_title), FIELD(palette_empty),
    FIELD(help_title),	  FIELD(help_hint),
};

/* A language file is a few lines; refuse anything beyond 1 MiB. */
#define LABELS_MAX_FILE 1048576L

static char *trim(char *s)
{
	while (isspace((unsigned char)*s))
		s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return s;
}

bool ctui_labels_load(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return false;
	}
	long size = ftell(f);
	if (size < 0 || size > LABELS_MAX_FILE || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return false;
	}
	char *text = calloc((size_t)size + 1, 1); /* NUL-terminated */
	if (!text || fread(text, 1, (size_t)size, f) != (size_t)size) {
		free(text);
		fclose(f);
		return false;
	}
	fclose(f);

	ctui_labels loaded = {0};
	char *save = NULL;
	for (char *line = strtok_r(text, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(line, '=');
		line = trim(line);
		if (!eq || line[0] == '#')
			continue;
		*eq = '\0';
		const char *name = trim(line);
		char *value = trim(eq + 1);
		for (size_t i = 0; i < sizeof FIELDS / sizeof FIELDS[0]; i++)
			if (!strcmp(name, FIELDS[i].name))
				*(const char **)((char *)&loaded +
						 FIELDS[i].offset) = value;
	}
	apply(&loaded);
	free(g_store);
	g_store = text;
	return true;
}

bool ctui_labels_load_locale(const char *dir)
{
	const char *names[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
	const char *locale = NULL;
	for (size_t i = 0; i < 3 && !locale; i++) {
		const char *v = getenv(names[i]);
		if (v && *v)
			locale = v;
	}
	if (!locale)
		return false;
	char code[64];
	size_t n = strcspn(locale, ".@");
	if (n == 0 || n >= sizeof code)
		return false;
	memcpy(code, locale, n);
	code[n] = '\0';
	if (!strcmp(code, "C") || !strcmp(code, "POSIX") || strchr(code, '/'))
		return false;
	char path[1024];
	snprintf(path, sizeof path, "%s/%s.lang", dir, code);
	if (ctui_labels_load(path))
		return true;
	char *underscore = strchr(code, '_');
	if (!underscore)
		return false;
	*underscore = '\0';
	snprintf(path, sizeof path, "%s/%s.lang", dir, code);
	return ctui_labels_load(path);
}

ctui_labels ctui_labels_resolve(const ctui_labels *override)
{
	ctui_labels defaults = ctui_labels_default();
	return overlay(override, g_set ? &g_labels : &defaults);
}
