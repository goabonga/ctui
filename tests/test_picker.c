/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_picker.c - browsing, choosing and mouse handling of the pickers.
 */
#include <curses.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include <ctui/picker.h>

static char root[64];

static void touch(const char *rel, const char *content)
{
	char path[160];
	snprintf(path, sizeof path, "%s/%s", root, rel);
	FILE *f = fopen(path, "w");
	if (f) {
		fputs(content, f);
		fclose(f);
	}
}

static void make_dir(const char *rel)
{
	char path[160];
	snprintf(path, sizeof path, "%s/%s", root, rel);
	mkdir(path, 0755);
}

static void make_fixture(void)
{
	snprintf(root, sizeof root, "/tmp/ctui-picker-XXXXXX");
	if (!mkdtemp(root))
		abort();
	make_dir("src");
	make_dir("Docs");
	make_dir(".cache");
	touch("zeta.txt", "zz");
	touch("Alpha.c", "");
	touch(".hidden", "");
	touch("src/main.c", "int main;");
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

static const char *name_at(const ctui_picker *p, size_t i)
{
	return i < p->count ? p->entries[i].name : "(none)";
}

static void test_file_listing(void)
{
	ctui_picker p;
	CHECK(ctui_picker_open(&p, CTUI_PICKER_FILE, root));
	CHECK_STR(p.path, root);
	/* "..", folders first, then files, case-insensitively, no hidden entry
	 */
	CHECK(p.count == 5);
	CHECK_STR(name_at(&p, 0), "..");
	CHECK_STR(name_at(&p, 1), "Docs");
	CHECK_STR(name_at(&p, 2), "src");
	CHECK_STR(name_at(&p, 3), "Alpha.c");
	CHECK_STR(name_at(&p, 4), "zeta.txt");
	CHECK(p.entries[4].size == 2);
	CHECK(p.selected == 1); /* the first real entry */
	ctui_picker_free(&p);
}

static void test_hidden_toggle(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_picker_key(&p, KEY_END, 10);
	CHECK_STR(name_at(&p, p.selected), "zeta.txt");
	CHECK(ctui_picker_key(&p, '.', 10) == CTUI_DIALOG_OPEN);
	CHECK(p.hidden && p.count == 7);
	CHECK_STR(name_at(&p, p.selected),
		  "zeta.txt"); /* the selection stays on its entry */
	CHECK_STR(name_at(&p, 1), ".cache");
	ctui_picker_key(&p, '.', 10);
	CHECK(!p.hidden && p.count == 5);
	ctui_picker_free(&p);
}

static void test_navigation(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_picker_key(&p, KEY_DOWN, 10); /* src */
	CHECK_STR(name_at(&p, p.selected), "src");
	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_OPEN);
	char want[160];
	snprintf(want, sizeof want, "%s/src", root);
	CHECK_STR(p.path, want);
	CHECK_STR(name_at(&p, 1), "main.c");

	/* going back up selects the folder we left */
	CHECK(ctui_picker_key(&p, KEY_LEFT, 10) == CTUI_DIALOG_OPEN);
	CHECK_STR(p.path, root);
	CHECK_STR(name_at(&p, p.selected), "src");

	ctui_picker_key(&p, KEY_HOME, 10);
	CHECK_STR(name_at(&p, p.selected), "..");
	ctui_picker_key(&p, '\n', 10);
	CHECK(strcmp(p.path, root) != 0);
	CHECK(strlen(p.path) < strlen(root));
	ctui_picker_free(&p);
}

static void test_choose_file(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_picker_key(&p, KEY_END, 10);
	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_ACCEPTED);
	char want[160];
	snprintf(want, sizeof want, "%s/zeta.txt", root);
	CHECK_STR(p.chosen, want);
	CHECK(ctui_picker_key(&p, 27, 10) == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_picker_key(&p, 'q', 10) == CTUI_DIALOG_CANCELLED);
	/* Space does nothing in file mode */
	CHECK(ctui_picker_key(&p, ' ', 10) == CTUI_DIALOG_OPEN);
	ctui_picker_free(&p);
}

static void test_folder_mode(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FOLDER, root);
	/* "..", ".", then the folders only */
	CHECK(p.count == 4);
	CHECK_STR(name_at(&p, 1), ".");
	CHECK_STR(name_at(&p, 2), "Docs");
	CHECK_STR(name_at(&p, 3), "src");
	CHECK(p.selected == 1); /* on "." */

	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_ACCEPTED);
	CHECK_STR(p.chosen, root);

	/* Space chooses the highlighted folder without opening it */
	ctui_picker_key(&p, KEY_DOWN, 10);
	CHECK(ctui_picker_key(&p, ' ', 10) == CTUI_DIALOG_ACCEPTED);
	char want[160];
	snprintf(want, sizeof want, "%s/Docs", root);
	CHECK_STR(p.chosen, want);
	CHECK_STR(p.path, root);

	/* Enter opens it instead */
	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_OPEN);
	CHECK_STR(p.path, want);
	CHECK(p.count == 2); /* ".." and "." */
	ctui_picker_key(&p, KEY_HOME, 10);
	CHECK(ctui_picker_key(&p, ' ', 10) ==
	      CTUI_DIALOG_OPEN); /* never ".." */
	ctui_picker_free(&p);
}

static void test_any_mode(void)
{
	ctui_picker p;
	CHECK(ctui_picker_open(&p, CTUI_PICKER_ANY, root));
	/* "..", ".", the folders, then the files */
	CHECK(p.count == 6);
	CHECK_STR(name_at(&p, 1), ".");
	CHECK_STR(name_at(&p, 2), "Docs");
	CHECK_STR(name_at(&p, 4), "Alpha.c");
	CHECK(p.selected == 2); /* the first real entry */

	char want[160];
	/* Enter on a file chooses it */
	ctui_picker_key(&p, KEY_DOWN, 10);
	ctui_picker_key(&p, KEY_DOWN, 10);
	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_ACCEPTED);
	snprintf(want, sizeof want, "%s/Alpha.c", root);
	CHECK_STR(p.chosen, want);

	/* Space on a folder chooses it without opening it */
	ctui_picker_key(&p, KEY_HOME, 10);
	ctui_picker_key(&p, KEY_DOWN, 10);
	ctui_picker_key(&p, KEY_DOWN, 10);
	CHECK(ctui_picker_key(&p, ' ', 10) == CTUI_DIALOG_ACCEPTED);
	snprintf(want, sizeof want, "%s/Docs", root);
	CHECK_STR(p.chosen, want);
	CHECK_STR(p.path, root);

	/* Enter on a folder opens it; "." chooses the folder being browsed */
	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_OPEN);
	CHECK_STR(p.path, want);
	ctui_picker_key(&p, KEY_DOWN, 10);
	CHECK_STR(name_at(&p, p.selected), ".");
	CHECK(ctui_picker_key(&p, '\n', 10) == CTUI_DIALOG_ACCEPTED);
	CHECK_STR(p.chosen, want);
	ctui_picker_key(&p, KEY_HOME, 10);
	CHECK(ctui_picker_key(&p, ' ', 10) ==
	      CTUI_DIALOG_OPEN); /* never ".." */
	ctui_picker_free(&p);
}

static void test_errors(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	CHECK(!ctui_picker_enter(&p, "does-not-exist"));
	CHECK(p.error[0] != '\0');
	CHECK_STR(p.path, root); /* unchanged */
	CHECK(ctui_picker_enter(&p, "src"));
	CHECK(p.error[0] == '\0');
	CHECK(!ctui_picker_enter(&p, "does-not-exist"));
	CHECK(strstr(p.error, "Cannot open does-not-exist: "));
	ctui_labels words = {.open_error = "Ouverture impossible de"};
	p.labels = &words;
	CHECK(!ctui_picker_enter(&p, "does-not-exist"));
	CHECK(strstr(p.error, "Ouverture impossible de does-not-exist: "));
	ctui_picker_free(&p);

	/* an unreadable start falls back to the root */
	CHECK(ctui_picker_open(&p, CTUI_PICKER_FILE, "/definitely/not/here"));
	CHECK_STR(p.path, "/");
	CHECK(p.count > 0 &&
	      strcmp(p.entries[0].name, "..") != 0); /* no ".." at the root */
	ctui_picker_free(&p);
}

static void test_scrolling(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_picker_key(&p, '.', 3);
	CHECK(p.count == 7);
	ctui_picker_key(&p, KEY_END, 3);
	CHECK(p.selected == 6 && p.top == 4);
	ctui_picker_key(&p, KEY_HOME, 3);
	CHECK(p.selected == 0 && p.top == 0);
	ctui_picker_key(&p, KEY_NPAGE, 3); /* jumps rows - 1 */
	CHECK(p.selected == 2);
	ctui_picker_key(&p, KEY_PPAGE, 3);
	CHECK(p.selected == 0);
	ctui_picker_free(&p);
}

static void test_geometry_is_centred(void)
{
	ctui_rect box;
	CHECK(!ctui_picker_geometry(9, 80, &box));
	CHECK(!ctui_picker_geometry(24, 39, &box));
	CHECK(ctui_picker_geometry(40, 120, &box));
	CHECK(box.width == 72 && box.height == 20);
	CHECK(box.x == (120 - 72) / 2 && box.y == (40 - 20) / 2);
	CHECK(ctui_picker_geometry(12, 50, &box));
	CHECK(box.width == 48 && box.height == 10 && box.x == 1 && box.y == 1);
	CHECK(ctui_picker_rows(20) == 15);
}

static void test_mouse(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_rect box;
	ctui_picker_geometry(40, 120, &box);
	int first = box.y + 3, x = box.x + 5;
	CHECK(ctui_picker_hit(&p, 40, 120, first, x) == 0);
	CHECK(ctui_picker_hit(&p, 40, 120, first + 4, x) == 4);
	CHECK(ctui_picker_hit(&p, 40, 120, first + 5, x) ==
	      -1); /* past the entries */
	CHECK(ctui_picker_hit(&p, 40, 120, first - 1, x) == -1); /* the rule */
	CHECK(ctui_picker_hit(&p, 40, 120, first, box.x) ==
	      -1); /* the border */

	/* the first click selects, the second one acts */
	CHECK(ctui_picker_click(&p, 40, 120, first + 4, x) == CTUI_DIALOG_OPEN);
	CHECK(p.selected == 4);
	CHECK(ctui_picker_click(&p, 40, 120, first + 4, x) ==
	      CTUI_DIALOG_ACCEPTED);
	CHECK(strstr(p.chosen, "zeta.txt") != NULL);
	CHECK(ctui_picker_click(&p, 40, 120, 0, 0) == CTUI_DIALOG_OPEN);
	CHECK(ctui_picker_click(&p, 40, 120, box.y, box.x + box.width - 3) ==
	      CTUI_DIALOG_CANCELLED);
	ctui_picker_free(&p);
}

static void test_scrollbar(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_picker_key(&p, '.', 3);
	CHECK(p.count == 7);
	ctui_rect box;
	CHECK(ctui_picker_geometry(10, 40, &box));
	size_t rows = ctui_picker_rows(box.height);
	CHECK(rows == 3);
	int col = box.x + box.width - 2, top = box.y + 3;

	/* the wheel moves the window and drags the selection along */
	CHECK(ctui_picker_wheel(&p, 10, 40, 2));
	CHECK(p.top == 2 && p.selected == 2);
	CHECK(ctui_picker_wheel(&p, 10, 40, 100));
	CHECK(p.top == p.count - rows && p.selected == p.top);
	CHECK(ctui_picker_wheel(&p, 10, 40, -100));
	CHECK(p.top == 0 && p.selected == 2);

	/* a press on the thumb grabs it, a drag to the last row reaches the end
	 */
	CHECK(ctui_picker_click(&p, 10, 40, top, col) == CTUI_DIALOG_OPEN);
	CHECK(p.bar.dragging);
	CHECK(ctui_picker_drag(&p, 10, 40, top + (int)rows - 1));
	CHECK(p.top == p.count - rows && p.selected >= p.top);
	CHECK(ctui_picker_release(&p));
	CHECK(!ctui_picker_release(&p));
	CHECK(!ctui_picker_drag(&p, 10, 40, top));

	/* a press under the thumb pages down and acts on no entry */
	ctui_picker_wheel(&p, 10, 40, -100);
	CHECK(ctui_picker_click(&p, 10, 40, top + (int)rows - 1, col) ==
	      CTUI_DIALOG_OPEN);
	CHECK(!p.bar.dragging && p.top == 3 && p.selected == 3);
	ctui_picker_free(&p);

	/* nothing to scroll, nothing taken */
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	CHECK(ctui_picker_geometry(40, 120, &box));
	CHECK(ctui_picker_click(&p, 40, 120, box.y + 3,
				box.x + box.width - 2) == CTUI_DIALOG_OPEN);
	CHECK(!p.bar.dragging && p.top == 0);
	CHECK(p.selected == 0); /* the margin is part of the row again */
	ctui_picker_free(&p);
}

static void test_size(void)
{
	ctui_picker p;
	ctui_picker_open(&p, CTUI_PICKER_FILE, root);
	ctui_rect box;
	CHECK(ctui_picker_geometry_for(&p, 40, 120, &box));
	CHECK(box.width == 72 && box.height == 20);
	p.width = 100;
	p.height = 30;
	CHECK(ctui_picker_geometry_for(&p, 50, 130, &box));
	CHECK(box.width == 100 && box.height == 30);
	CHECK(box.x == 15 && box.y == 10);
	/* the screen still bounds it, and a tiny request is raised */
	CHECK(ctui_picker_geometry_for(&p, 24, 80, &box));
	CHECK(box.width == 78 && box.height == 22);
	p.width = 5;
	p.height = 3;
	CHECK(ctui_picker_geometry_for(&p, 24, 80, &box));
	CHECK(box.width == 40 && box.height == 10);
	/* clicks and hits follow the picker's own box */
	p.width = 50;
	p.height = 12;
	ctui_picker_geometry_for(&p, 40, 120, &box);
	CHECK(ctui_picker_hit(&p, 40, 120, box.y + 3, box.x + 5) == 0);
	CHECK(ctui_picker_click(&p, 40, 120, box.y, box.x + box.width - 3) ==
	      CTUI_DIALOG_CANCELLED);
	ctui_picker_free(&p);
}

int main(void)
{
	make_fixture();
	RUN(test_file_listing);
	RUN(test_hidden_toggle);
	RUN(test_navigation);
	RUN(test_choose_file);
	RUN(test_folder_mode);
	RUN(test_any_mode);
	RUN(test_errors);
	RUN(test_scrolling);
	RUN(test_geometry_is_centred);
	RUN(test_mouse);
	RUN(test_scrollbar);
	RUN(test_size);
	remove_fixture();
	return CHECK_DONE();
}
