/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <curses.h>

#include <ctui/dialog.h>

static void test_confirm_keys(void)
{
	ctui_confirm c;
	ctui_confirm_init(&c, "Delete?", "vm1");
	CHECK(!c.yes_selected);
	CHECK(ctui_confirm_key(&c, '\n') ==
	      CTUI_DIALOG_CANCELLED); /* No is the default */
	CHECK(ctui_confirm_key(&c, KEY_LEFT) == CTUI_DIALOG_OPEN &&
	      c.yes_selected);
	CHECK(ctui_confirm_key(&c, ' ') == CTUI_DIALOG_ACCEPTED);
	CHECK(ctui_confirm_key(&c, KEY_RIGHT) == CTUI_DIALOG_OPEN &&
	      !c.yes_selected);
	CHECK(ctui_confirm_key(&c, 'y') == CTUI_DIALOG_ACCEPTED);
	CHECK(ctui_confirm_key(&c, 'n') == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_confirm_key(&c, 27) == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_confirm_key(&c, 'x') == CTUI_DIALOG_OPEN);
}

static void test_dialog_box(void)
{
	ctui_rect box;
	CHECK(ctui_dialog_box(24, 80, 60, 14, &box));
	CHECK(box.y == 5 && box.x == 10 && box.height == 14 && box.width == 60);
	/* shrunk to the screen, keeping a margin */
	CHECK(ctui_dialog_box(24, 80, 200, 50, &box));
	CHECK(box.y == 1 && box.x == 1 && box.height == 22 && box.width == 78);
	CHECK(!ctui_dialog_box(24, 11, 60, 14, &box));
	CHECK(!ctui_dialog_box(4, 80, 60, 14, &box));
}

static void test_confirm_geometry(void)
{
	ctui_confirm c;
	ctui_confirm_init(&c, "Delete?", "vm1");
	ctui_rect box;
	CHECK(!ctui_confirm_geometry(&c, 9, 80, &box));
	CHECK(!ctui_confirm_geometry(&c, 24, 39, &box));
	CHECK(ctui_confirm_geometry(&c, 24, 80, &box));
	/* action, name, blank row, buttons, and the two borders */
	CHECK(box.width == 72 && box.height == 6 && box.x == 4 && box.y == 9);
	CHECK(ctui_confirm_geometry(&c, 24, 200, &box) && box.width == 76);

	ctui_confirm_init(&c, "Delete?", NULL);
	CHECK(ctui_confirm_geometry(&c, 12, 80, &box) && box.height == 5);

	/* a long action wraps on up to three rows */
	char long_action[400];
	memset(long_action, 'x', sizeof long_action - 1);
	long_action[sizeof long_action - 1] = '\0';
	ctui_confirm_init(&c, long_action, "vm1");
	CHECK(ctui_confirm_geometry(&c, 24, 80, &box) && box.height == 8);
}

static void test_confirm_hit(void)
{
	ctui_confirm c;
	ctui_confirm_init(&c, "Delete?", "vm1");
	ctui_rect box;
	ctui_confirm_geometry(&c, 24, 80, &box);
	int by = box.y + box.height - 2, yes = box.x + box.width / 2 - 11;
	CHECK(ctui_confirm_hit(&c, 24, 80, by, yes) == 1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, yes + 8) == 1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, yes + 9) == -1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, box.x + box.width / 2 + 2) == 0);
	CHECK(ctui_confirm_hit(&c, 24, 80, by - 1, yes) == -1);
	CHECK(ctui_confirm_hit(&c, 5, 80, by, yes) == -1);
	/* the close button sits on the top edge, right of the title */
	CHECK(ctui_confirm_hit(&c, 24, 80, box.y, box.x + box.width - 3) == 2);
	CHECK(ctui_confirm_hit(&c, 24, 80, box.y, box.x + 3) == -1);
}

static void test_prompt_geometry(void)
{
	char buf[8] = "";
	ctui_prompt p;
	ctui_prompt_init(&p, "name", buf, sizeof buf);
	ctui_rect box;
	CHECK(!ctui_prompt_geometry(&p, 9, 80, &box));
	CHECK(!ctui_prompt_geometry(&p, 24, 29, &box));
	CHECK(ctui_prompt_geometry(&p, 24, 80, &box));
	/* one label row, the input row and the two borders */
	CHECK(box.width == 70 && box.height == 4 && box.x == 5 && box.y == 10);

	char long_label[400];
	memset(long_label, 'x', sizeof long_label - 1);
	long_label[sizeof long_label - 1] = '\0';
	ctui_prompt_init(&p, long_label, buf, sizeof buf);
	CHECK(ctui_prompt_geometry(&p, 24, 80, &box) && box.height == 6);
}

static void test_close_hits(void)
{
	char buf[8] = "";
	ctui_prompt p;
	ctui_prompt_init(&p, "name", buf, sizeof buf);
	ctui_rect box;
	ctui_prompt_geometry(&p, 24, 80, &box);
	CHECK(ctui_prompt_close_hit(&p, 24, 80, box.y, box.x + box.width - 3));
	CHECK(!ctui_prompt_close_hit(&p, 24, 80, box.y + 1,
				     box.x + box.width - 3));
	CHECK(!ctui_prompt_close_hit(&p, 9, 80, box.y, box.x + box.width - 3));

	ctui_viewer v;
	ctui_viewer_init(&v, "t", "text");
	CHECK(ctui_viewer_close_hit(&v, 3, 5, 10, 40, 3, 5 + 40 - 3));
	CHECK(!ctui_viewer_close_hit(&v, 3, 5, 10, 40, 4, 5 + 40 - 3));
}

static void test_prompt_edit(void)
{
	char buf[8] = "ab";
	ctui_prompt p;
	ctui_prompt_init(&p, "name", buf, sizeof buf);
	CHECK(p.used == 2);
	CHECK(ctui_prompt_key(&p, 'c') == CTUI_DIALOG_OPEN);
	CHECK_STR(buf, "abc");
	ctui_prompt_key(&p, KEY_BACKSPACE);
	CHECK_STR(buf, "ab");
	ctui_prompt_key(&p, 127);
	ctui_prompt_key(&p, 8);
	ctui_prompt_key(&p, 8);
	CHECK_STR(buf, "");
	CHECK(p.used == 0);
	for (int i = 0; i < 20; i++)
		ctui_prompt_key(&p, 'z');
	CHECK(p.used == 7 && strlen(buf) == 7); /* capacity bound */
	ctui_prompt_key(&p, 21);
	CHECK_STR(buf, "");
	CHECK(ctui_prompt_key(&p, '\n') == CTUI_DIALOG_ACCEPTED);
	CHECK(ctui_prompt_key(&p, 27) == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_prompt_key(&p, 3) == CTUI_DIALOG_CANCELLED);
}

static void test_prompt_utf8_backspace(void)
{
	char buf[16] = "";
	ctui_prompt p;
	ctui_prompt_init(&p, "x", buf, sizeof buf);
	ctui_prompt_key(&p, 'a');
	ctui_prompt_key(&p, 0xC3); /* é as two bytes, as getch delivers them */
	ctui_prompt_key(&p, 0xA9);
	CHECK_STR(buf, "a\xc3\xa9");
	ctui_prompt_key(&p, KEY_BACKSPACE);
	CHECK_STR(buf, "a");
	ctui_prompt_key(&p, KEY_BACKSPACE);
	CHECK_STR(buf, "");
}

static void test_viewer(void)
{
	ctui_viewer v;
	ctui_viewer_init(&v, "t", "1\n2\n3\n4\n5\n");
	CHECK(ctui_viewer_lines(&v) == 5);
	CHECK(ctui_viewer_key(&v, KEY_DOWN, 3) == CTUI_DIALOG_OPEN &&
	      v.top == 1);
	ctui_viewer_key(&v, 'j', 3);
	ctui_viewer_key(&v, 'j', 3);
	CHECK(v.top == 2); /* clamped to lines - rows */
	ctui_viewer_key(&v, KEY_HOME, 3);
	CHECK(v.top == 0);
	ctui_viewer_key(&v, KEY_NPAGE, 3);
	CHECK(v.top == 2);
	ctui_viewer_key(&v, KEY_PPAGE, 3);
	CHECK(v.top == 0);
	ctui_viewer_key(&v, KEY_END, 3);
	CHECK(v.top == 2);
	ctui_viewer_key(&v, KEY_UP, 3);
	CHECK(v.top == 1);
	CHECK(ctui_viewer_key(&v, 'q', 3) == CTUI_DIALOG_CANCELLED);
	ctui_viewer_init(&v, "t", NULL);
	CHECK(ctui_viewer_lines(&v) == 0);
	ctui_viewer_init(&v, "t", "no newline");
	CHECK(ctui_viewer_lines(&v) == 1);
}

static void test_confirm_labels(void)
{
	ctui_confirm c;
	ctui_confirm_init(&c, "Delete?", NULL);
	ctui_labels over = {.yes = "Delete forever", .no = "Keep"};
	c.labels = &over;
	ctui_rect box;
	ctui_confirm_geometry(&c, 24, 80, &box);
	int by = box.y + box.height - 2, mid = box.x + box.width / 2;
	int bw = 14 + 4; /* the longer label plus its margins */
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid - 2 - bw) == 1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid - 3) == 1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid - 2 - bw - 1) == -1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid + 2) == 0);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid + 2 + bw - 1) == 0);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid + 2 + bw) == -1);

	/* short labels keep the standard button */
	ctui_labels shorter = {.yes = "Ok", .no = "No"};
	c.labels = &shorter;
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid - 11) == 1);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid + 2 + 8) == 0);
	CHECK(ctui_confirm_hit(&c, 24, 80, by, mid + 2 + 9) == -1);
}

int main(void)
{
	RUN(test_confirm_keys);
	RUN(test_dialog_box);
	RUN(test_confirm_geometry);
	RUN(test_confirm_hit);
	RUN(test_confirm_labels);
	RUN(test_prompt_geometry);
	RUN(test_close_hits);
	RUN(test_prompt_edit);
	RUN(test_prompt_utf8_backspace);
	RUN(test_viewer);
	return CHECK_DONE();
}
