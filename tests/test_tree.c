/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <curses.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctui/labels.h>
#include <ctui/tree.h>

static char root[64];

static void touch(const char *rel)
{
	char path[160];
	snprintf(path, sizeof path, "%s/%s", root, rel);
	FILE *f = fopen(path, "w");
	if (f)
		fclose(f);
}

static void make_fixture(void)
{
	snprintf(root, sizeof root, "/tmp/ctui-tree-XXXXXX");
	if (!mkdtemp(root))
		abort();
	char path[160];
	snprintf(path, sizeof path, "%s/src", root);
	mkdir(path, 0755);
	snprintf(path, sizeof path, "%s/Docs", root);
	mkdir(path, 0755);
	touch("zeta.txt");
	touch("Alpha.c");
	touch("src/main.c");
	touch("src/util.h");
	snprintf(path, sizeof path, "%s/link", root);
	if (symlink("src", path) != 0)
		abort();
}

static int remove_entry(const char *path, const struct stat *st, int type,
			struct FTW *ftw)
{
	(void)st;
	(void)type;
	(void)ftw;
	return remove(path);
}

static void remove_fixture(void)
{
	if (nftw(root, remove_entry, 16, FTW_DEPTH | FTW_PHYS) != 0)
		abort();
}

static void test_icons(void)
{
	ctui_tree_entry c = {.name = "main.c"}, h = {.name = "x.H"},
			mk = {.name = "Makefile"};
	ctui_tree_entry dir = {.name = "d", .directory = true};
	ctui_tree_entry open = {
	    .name = "d", .directory = true, .expanded = true};
	ctui_tree_entry link = {
	    .name = "l", .symlink = true, .directory = true};
	ctui_tree_entry other = {.name = "data.bin"}, bare = {.name = "noext"};
	CHECK_STR(ctui_tree_icon(&c, false), "[C]");
	CHECK_STR(ctui_tree_icon(&h, false), "[H]");
	CHECK_STR(ctui_tree_icon(&mk, false), "[=]");
	CHECK_STR(ctui_tree_icon(&dir, false), "[D]");
	CHECK_STR(ctui_tree_icon(&link, false), "[@]");
	CHECK_STR(ctui_tree_icon(&other, false), "[F]");
	CHECK_STR(ctui_tree_icon(&bare, false), "[F]");
	CHECK_STR(ctui_tree_icon(&dir, true), "");
	CHECK_STR(ctui_tree_icon(&open, true), "");
	CHECK(ctui_tree_icon_color(&c) == CTUI_ICON_BLUE);
	CHECK(ctui_tree_icon_color(&h) == CTUI_ICON_PURPLE);
	CHECK(ctui_tree_icon_color(&dir) == CTUI_ICON_YELLOW);
	CHECK(ctui_tree_icon_color(&link) == CTUI_ICON_CYAN);
}

static void test_init_sorts(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	CHECK(ctui_tree_init(&t, root, err, sizeof err));
	/* root, then folders (Docs, src), then files (Alpha.c, link, zeta.txt)
	 */
	CHECK(t.count == 1 + 5);
	CHECK(t.entries[0].expanded && t.entries[0].depth == 0);
	CHECK_STR(t.entries[1].name, "Docs");
	CHECK_STR(t.entries[2].name, "src");
	CHECK_STR(t.entries[3].name, "Alpha.c");
	CHECK_STR(t.entries[4].name, "link");
	CHECK(t.entries[4].symlink);
	CHECK_STR(t.entries[5].name, "zeta.txt");
	ctui_tree_free(&t);
	CHECK(t.count == 0 && t.entries == NULL);
}

static void test_toggle(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	ctui_tree_init(&t, root, err, sizeof err);
	CHECK(ctui_tree_toggle(&t, 2, err, sizeof err)); /* src */
	CHECK(t.count == 8 && t.entries[2].expanded);
	CHECK_STR(t.entries[3].name, "main.c");
	CHECK(t.entries[3].depth == 2);
	CHECK(ctui_tree_toggle(&t, 4, err, sizeof err) &&
	      t.count == 8); /* file: no-op */
	t.selected = 4;
	CHECK(ctui_tree_toggle(&t, 2, err, sizeof err)); /* collapse */
	CHECK(t.count == 6 && !t.entries[2].expanded && t.selected == 2);
	CHECK(ctui_tree_toggle(&t, 99, err, sizeof err));
	ctui_tree_free(&t);
}

static void test_errors(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	CHECK(!ctui_tree_init(&t, "/nonexistent-ctui-dir", err, sizeof err));
	CHECK(strstr(err, "Unable to access folder"));
	char file[160];
	snprintf(file, sizeof file, "%s/zeta.txt", root);
	CHECK(!ctui_tree_init(&t, file, err, sizeof err));
	CHECK_STR(err, "Select a folder");
	CHECK(t.count == 0);

	ctui_labels words = {.access_error = "Dossier inaccessible",
			     .not_a_folder = "Choisir un dossier"};
	ctui_labels_set(&words);
	CHECK(!ctui_tree_init(&t, "/nonexistent-ctui-dir", err, sizeof err));
	CHECK(strstr(err, "Dossier inaccessible: "));
	CHECK(!ctui_tree_init(&t, file, err, sizeof err));
	CHECK_STR(err, "Choisir un dossier");
	ctui_labels_set(NULL);
}

static void test_keys(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	ctui_tree_init(&t, root, err, sizeof err);
	CHECK(ctui_tree_key(&t, KEY_DOWN, 3, err, sizeof err) ==
		  CTUI_TREE_MOVED &&
	      t.selected == 1);
	CHECK(ctui_tree_key(&t, KEY_END, 3, err, sizeof err) ==
		  CTUI_TREE_MOVED &&
	      t.selected == 5);
	CHECK(ctui_tree_key(&t, KEY_DOWN, 3, err, sizeof err) ==
	      CTUI_TREE_NONE);
	CHECK(ctui_tree_key(&t, '\n', 3, err, sizeof err) ==
	      CTUI_TREE_OPEN_FILE);
	CHECK(ctui_tree_key(&t, KEY_RIGHT, 3, err, sizeof err) ==
	      CTUI_TREE_NONE);
	CHECK(ctui_tree_key(&t, KEY_HOME, 3, err, sizeof err) ==
		  CTUI_TREE_MOVED &&
	      t.selected == 0);
	ctui_tree_key(&t, KEY_DOWN, 3, err, sizeof err);
	ctui_tree_key(&t, KEY_DOWN, 3, err, sizeof err); /* src */
	CHECK(ctui_tree_key(&t, KEY_RIGHT, 3, err, sizeof err) ==
		  CTUI_TREE_TOGGLED &&
	      t.count == 8);
	ctui_tree_key(&t, KEY_DOWN, 3, err, sizeof err); /* src/main.c */
	CHECK(t.selected == 3);
	CHECK(ctui_tree_key(&t, KEY_LEFT, 3, err, sizeof err) ==
		  CTUI_TREE_MOVED &&
	      t.selected == 2);
	CHECK(ctui_tree_key(&t, KEY_LEFT, 3, err, sizeof err) ==
		  CTUI_TREE_TOGGLED &&
	      t.count == 6);
	CHECK(ctui_tree_key(&t, KEY_PPAGE, 2, err, sizeof err) ==
		  CTUI_TREE_MOVED &&
	      t.selected == 0);
	CHECK(ctui_tree_key(&t, KEY_NPAGE, 4, err, sizeof err) ==
		  CTUI_TREE_MOVED &&
	      t.selected == 4);
	ctui_tree_free(&t);
}

static void test_scroll(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	ctui_tree_init(&t, root, err, sizeof err);
	t.selected = 5;
	ctui_tree_scroll(&t, 3);
	CHECK(t.top == 3);
	t.selected = 0;
	ctui_tree_scroll(&t, 3);
	CHECK(t.top == 0);
	t.selected = 5;
	ctui_tree_scroll(&t, 100);
	CHECK(t.top == 0);
	ctui_tree_free(&t);
}

static void test_scroll_to(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	ctui_tree_init(&t, root, err, sizeof err);
	CHECK(t.count > 3);
	t.selected = 5;
	ctui_tree_scroll_to(&t, 3, 0);
	CHECK(t.top == 0 && t.selected == 2);
	ctui_tree_scroll_to(&t, 3, 99);
	CHECK(t.top == t.count - 3 && t.selected == t.top);
	t.selected = t.count - 1;
	ctui_tree_scroll_to(&t, 3, 1);
	CHECK(t.top == 1 && t.selected == 3);
	ctui_tree_scroll_to(&t, 100, 4);
	CHECK(t.top == 0);
	ctui_tree_free(&t);
}

static void test_hover(void)
{
	ctui_tree t = {0};
	char err[128] = "";
	ctui_tree_init(&t, root, err, sizeof err);
	CHECK(!t.hovering);
	CHECK(ctui_tree_hover(&t, 1, 3) && t.hovering && t.hover == t.top + 1);
	CHECK(!ctui_tree_hover(&t, 1, 3));
	CHECK(ctui_tree_hover(&t, 2, 3) && t.hover == t.top + 2);
	CHECK(ctui_tree_hover(&t, 3, 3) && !t.hovering);
	CHECK(!ctui_tree_hover(&t, -1, 3));
	CHECK(ctui_tree_hover(&t, 0, 3));
	CHECK(ctui_tree_hover(&t, (int)t.count + 5, 200) && !t.hovering);
	ctui_tree_free(&t);
}

int main(void)
{
	make_fixture();
	RUN(test_icons);
	RUN(test_init_sorts);
	RUN(test_toggle);
	RUN(test_errors);
	RUN(test_keys);
	RUN(test_scroll);
	RUN(test_scroll_to);
	RUN(test_hover);
	remove_fixture();
	return CHECK_DONE();
}
