/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_labels.c - global labels and per-component overrides.
 */
#include "check.h"

#include <ctui/labels.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_defaults(void)
{
	ctui_labels d = ctui_labels_default(), r = ctui_labels_resolve(NULL);
	CHECK_STR(d.yes, "Yes");
	CHECK_STR(r.yes, "Yes");
	CHECK_STR(r.folder_title, "Select folder");
	CHECK_STR(r.any_title, "Open file or folder");
	CHECK_STR(r.not_available, "N/A");
	CHECK_STR(r.this_folder, ". (choose this folder)");
	CHECK_STR(r.access_error, "Unable to access folder");
	CHECK_STR(r.open_error, "Cannot open");
	CHECK_STR(r.not_a_folder, "Select a folder");
	CHECK_STR(r.no_memory, "Out of memory");
	CHECK_STR(r.path_too_long, "Path too long");
	CHECK_STR(r.palette_title, "Command palette");
	CHECK_STR(r.palette_empty, "No matching command");
	CHECK_STR(r.help_title, "Keyboard shortcuts");
	CHECK_STR(r.help_hint, "Up/Down: scroll  Esc: close");
}

static void test_failures(void)
{
	ctui_labels mine = {.access_error = "Dossier inaccessible",
			    .no_memory = "Plus de memoire"};
	ctui_labels_set(&mine);
	ctui_labels r = ctui_labels_resolve(NULL);
	CHECK_STR(r.access_error, "Dossier inaccessible");
	CHECK_STR(r.no_memory, "Plus de memoire");
	CHECK_STR(
	    r.read_error,
	    "Error reading folder"); /* the others keep the built-in text */
	ctui_labels over = {.no_memory = "Memoire"};
	CHECK_STR(ctui_labels_resolve(&over).no_memory, "Memoire");
	CHECK_STR(ctui_labels_resolve(&over).access_error,
		  "Dossier inaccessible");
	ctui_labels_set(NULL);
}

static void test_global(void)
{
	ctui_labels mine = {.yes = "Oui", .no = "Non"};
	ctui_labels_set(&mine);
	ctui_labels r = ctui_labels_resolve(NULL);
	CHECK_STR(r.yes, "Oui");
	CHECK_STR(r.no, "Non");
	CHECK_STR(r.cancel, "Cancel"); /* unset fields keep the built-in text */

	/* a second set replaces the first, it does not add to it */
	ctui_labels other = {.cancel = "Annuler"};
	ctui_labels_set(&other);
	r = ctui_labels_resolve(NULL);
	CHECK_STR(r.yes, "Yes");
	CHECK_STR(r.cancel, "Annuler");
}

static void test_override(void)
{
	ctui_labels global = {
	    .yes = "Oui", .no = "Non", .confirm_title = "Confirmer"};
	ctui_labels_set(&global);
	ctui_labels over = {.yes = "Delete"};
	ctui_labels r = ctui_labels_resolve(&over);
	CHECK_STR(r.yes, "Delete"); /* the override wins */
	CHECK_STR(r.no, "Non");	    /* the rest comes from the global set */
	CHECK_STR(r.confirm_title, "Confirmer");
	CHECK_STR(ctui_labels_resolve(NULL).yes,
		  "Oui"); /* the global set is untouched */

	ctui_labels_set(NULL);
	CHECK_STR(ctui_labels_resolve(NULL).yes, "Yes");
}

static void write_file(const char *dir, const char *name, const char *text)
{
	char path[256];
	snprintf(path, sizeof path, "%s/%s", dir, name);
	FILE *f = fopen(path, "w");
	CHECK(f != NULL);
	if (!f)
		return;
	fputs(text, f);
	fclose(f);
}

static void remove_file(const char *dir, const char *name)
{
	char path[256];
	snprintf(path, sizeof path, "%s/%s", dir, name);
	unlink(path);
}

static void test_load(void)
{
	char dir[] = "/tmp/ctui-labels-XXXXXX";
	CHECK(mkdtemp(dir) != NULL);
	write_file(
	    dir, "fr.lang",
	    "# francais\n\nyes = Oui\r\n  no=Non  \nconfirm_title = Confirmer\n"
	    "unknown = ignored\nno_memory =\nnot a line\n");
	char path[256];
	snprintf(path, sizeof path, "%s/fr.lang", dir);

	CHECK(!ctui_labels_load("/nonexistent/ctui.lang"));
	CHECK_STR(ctui_labels_resolve(NULL).yes,
		  "Yes"); /* a failed load changes nothing */

	CHECK(ctui_labels_load(path));
	ctui_labels r = ctui_labels_resolve(NULL);
	CHECK_STR(r.yes, "Oui");
	CHECK_STR(r.no, "Non");
	CHECK_STR(r.confirm_title, "Confirmer");
	CHECK_STR(r.no_memory, "");
	CHECK_STR(r.cancel, "Cancel"); /* not in the file */

	write_file(dir, "fr.lang", "cancel = Annuler\n");
	CHECK(ctui_labels_load(path));
	CHECK_STR(ctui_labels_resolve(NULL).cancel, "Annuler");
	CHECK_STR(ctui_labels_resolve(NULL).yes,
		  "Yes"); /* a second file replaces the first */

	ctui_labels_set(NULL);
	CHECK_STR(ctui_labels_resolve(NULL).cancel, "Cancel");
	remove_file(dir, "fr.lang");
	rmdir(dir);
}

static void test_locale(void)
{
	char dir[] = "/tmp/ctui-labels-XXXXXX";
	CHECK(mkdtemp(dir) != NULL);
	write_file(dir, "fr.lang", "yes = Oui\n");
	write_file(dir, "pt_BR.lang", "yes = Sim\n");
	unsetenv("LC_ALL");
	unsetenv("LC_MESSAGES");

	setenv("LANG", "fr_FR.UTF-8",
	       1); /* fr_FR.lang is missing, fr.lang answers */
	CHECK(ctui_labels_load_locale(dir));
	CHECK_STR(ctui_labels_resolve(NULL).yes, "Oui");

	setenv("LANG", "pt_BR.UTF-8", 1);
	CHECK(ctui_labels_load_locale(dir));
	CHECK_STR(ctui_labels_resolve(NULL).yes, "Sim");

	setenv("LC_ALL", "fr@euro", 1); /* LC_ALL wins over LANG */
	CHECK(ctui_labels_load_locale(dir));
	CHECK_STR(ctui_labels_resolve(NULL).yes, "Oui");

	ctui_labels_set(NULL);
	setenv("LC_ALL", "C", 1); /* C and POSIX keep the English */
	CHECK(!ctui_labels_load_locale(dir));
	setenv("LC_ALL", "de_DE.UTF-8", 1); /* no file for the language */
	CHECK(!ctui_labels_load_locale(dir));
	unsetenv("LC_ALL");
	unsetenv("LANG");
	CHECK(!ctui_labels_load_locale(dir));
	CHECK_STR(ctui_labels_resolve(NULL).yes, "Yes");

	remove_file(dir, "fr.lang");
	remove_file(dir, "pt_BR.lang");
	rmdir(dir);
}

int main(void)
{
	RUN(test_defaults);
	RUN(test_global);
	RUN(test_override);
	RUN(test_failures);
	RUN(test_load);
	RUN(test_locale);
	return CHECK_DONE();
}
