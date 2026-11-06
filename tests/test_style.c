/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_style.c - what a component draws with its own style.
 */
#include "check.h"
#include "screen.h"
#include <ctui/ctui.h>

/* A style that paints `pair` red on blue. */
static ctui_style paint(ctui_pair pair)
{
	ctui_style style = {0};
	style.attr[pair] = ctui_attr_color(196, 21);
	return style;
}

static void test_list(void)
{
	static const ctui_list_item items[] = {{"One", NULL, false},
					       {"Two", NULL, false}};
	ctui_style style = paint(CTUI_PAIR_SELECTION);
	ctui_list list;
	ctui_list_init(&list, items, 2, false);

	screen_clear();
	ctui_list_draw(&list, 0, 0, 20, 4);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_SELECTION]));

	list.style = &style;
	screen_clear();
	ctui_list_draw(&list, 0, 0, 20, 4);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));
	CHECK(ctui_attr(CTUI_PAIR_SELECTION, false) !=
	      style.attr[CTUI_PAIR_SELECTION]); /* the style is released */

	screen_clear();
	ctui_menu_draw(&list, 0, 0, 20);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));
}

static void test_burger(void)
{
	static const ctui_list_item items[] = {{"New", NULL, false}};
	ctui_style style = {0};
	style.glyphs.burger = "Z";
	ctui_burger b;
	ctui_burger_init(&b, items, 1);

	screen_clear();
	ctui_burger_draw(&b, 0, 0);
	CHECK(!screen_has_text("Z"));

	b.style = &style;
	screen_clear();
	ctui_burger_draw(&b, 0, 0);
	CHECK(screen_has_text("Z"));
}

static void test_dialog(void)
{
	ctui_style style = {0};
	style.glyphs.close = "Q";
	ctui_confirm c;
	ctui_confirm_init(&c, "Delete?", "vm1");
	ctui_prompt p;
	char value[16] = "";
	ctui_prompt_init(&p, "Name", value, sizeof value);
	ctui_viewer v;
	ctui_viewer_init(&v, "Title", "one\ntwo");

	screen_clear();
	ctui_confirm_draw(&c);
	CHECK(!screen_has_text(" Q "));
	ctui_prompt_draw(&p);
	ctui_viewer_draw(&v, 0, 0, 10, 40);
	CHECK(!screen_has_text(" Q "));

	c.style = &style;
	screen_clear();
	ctui_confirm_draw(&c);
	CHECK(screen_has_text(" Q "));

	p.style = &style;
	screen_clear();
	ctui_prompt_draw(&p);
	CHECK(screen_has_text(" Q "));

	v.style = &style;
	screen_clear();
	ctui_viewer_draw(&v, 0, 0, 10, 40);
	CHECK(screen_has_text(" Q "));
}

static void test_toast(void)
{
	ctui_style style = {0};
	style.glyphs.info = "@";
	ctui_toasts t;
	ctui_toasts_init(&t);
	ctui_toasts_push(&t, CTUI_TOAST_INFO, "hello", 0, 0);

	screen_clear();
	ctui_toasts_draw(&t, 0);
	CHECK(!screen_has_text("@"));

	t.style = &style;
	screen_clear();
	ctui_toasts_draw(&t, 0);
	CHECK(screen_has_text("@"));
}

static void test_tree(void)
{
	ctui_style style = paint(CTUI_PAIR_SELECTION);
	ctui_tree t = {0};
	char error[80];
	CHECK(ctui_tree_init(&t, ".", error, sizeof error));

	screen_clear();
	ctui_tree_draw(&t, 0, 0, 40, 5, true);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_SELECTION]));

	t.style = &style;
	screen_clear();
	ctui_tree_draw(&t, 0, 0, 40, 5, true);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));
	ctui_tree_free(&t);
}

static void test_textview(void)
{
	ctui_style style = paint(CTUI_PAIR_BODY);
	ctui_textview v;
	CHECK(ctui_textview_init(&v, "one\ntwo\n"));

	screen_clear();
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_BODY]));

	v.style = &style;
	screen_clear();
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_BODY]));
	ctui_textview_free(&v);
}

static void test_waveform(void)
{
	static float samples[200];
	for (int i = 0; i < 200; i++)
		samples[i] = (float)i / 200.0f;
	ctui_style style = paint(CTUI_PAIR_CURSOR);
	ctui_waveform w;
	ctui_waveform_init(&w, samples, 200, 1);

	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_CURSOR]));

	w.style = &style;
	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_CURSOR]));
}

static void test_waveform_filled(void)
{
	static float samples[200];
	for (int i = 0; i < 200; i++)
		samples[i] = (i % 2 ? 0.8f : -0.8f);
	ctui_waveform w;
	ctui_waveform_init(&w, samples, 200, 1);
	w.filled = true;

	/* without a playhead every bar has the info tone of the header */
	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(screen_has_color(
	    ctui_attr_color(ctui_tone_background(CTUI_TONE_INFO), -1)));
	CHECK(!screen_has_color(
	    ctui_attr_color(ctui_tone_background(CTUI_TONE_IDLE), -1)));

	/* with one, what is after it has the idle tone */
	w.show_playhead = true;
	w.playhead = 50;
	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(screen_has_color(
	    ctui_attr_color(ctui_tone_background(CTUI_TONE_INFO), -1)));
	CHECK(screen_has_color(
	    ctui_attr_color(ctui_tone_background(CTUI_TONE_IDLE), -1)));
}

static void test_panel(void)
{
	ctui_style style = paint(CTUI_PAIR_HEADING);
	ctui_panel p;
	ctui_panel_layout layout;
	ctui_panel_init(&p);
	ctui_panel_section(&p, "Engine");
	ctui_panel_add_line(&p, "  running");
	ctui_panel_arrange(&p, 40, false, &layout);

	screen_clear();
	ctui_panel_draw(&p, &layout, 0, 0, 5, 0);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_HEADING]));

	p.style = &style;
	screen_clear();
	ctui_panel_draw(&p, &layout, 0, 0, 5, 0);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_HEADING]));
	ctui_panel_layout_free(&layout);
	ctui_panel_free(&p);
}

static void test_tabpanel(void)
{
	static const char *const labels[] = {"One", "Two"};
	ctui_style style = paint(CTUI_PAIR_SELECTION);
	ctui_tabpanel p;
	ctui_tabpanel_init(&p, labels, 2);

	screen_clear();
	ctui_tabpanel_draw(&p, 0, 0, 40, true);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_SELECTION]));

	p.style = &style;
	screen_clear();
	ctui_tabpanel_draw(&p, 0, 0, 40, true);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));
}

static void test_wizard(void)
{
	static const char *const titles[] = {"One", "Two"};
	ctui_style style = paint(CTUI_PAIR_SELECTION);
	ctui_wizard w;
	ctui_wizard_init(&w, titles, 2);

	screen_clear();
	ctui_wizard_steps_draw(&w, 0, 0, 40);
	ctui_wizard_buttons_draw(&w, 2, 0, 40, -1);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_SELECTION]));

	w.style = &style;
	screen_clear();
	ctui_wizard_steps_draw(&w, 0, 0, 40);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));

	screen_clear();
	ctui_wizard_buttons_draw(&w, 2, 0, 40, CTUI_WIZARD_NEXT);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));
}

static void test_form(void)
{
	char name[16] = "Ada";
	ctui_field fields[] = {ctui_field_text("Name", name, sizeof name)};
	ctui_style style = paint(CTUI_PAIR_SELECTION);
	ctui_form f;
	ctui_form_init(&f, fields, 1);

	screen_clear();
	ctui_form_draw(&f, 0, 0, 40, 4);
	CHECK(!screen_has_color(style.attr[CTUI_PAIR_SELECTION]));

	f.style = &style;
	screen_clear();
	ctui_form_draw(&f, 0, 0, 40, 4);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_SELECTION]));
}

static void test_picker(void)
{
	ctui_style style = {0};
	style.glyphs.close = "Q";
	ctui_picker p;
	CHECK(ctui_picker_open(&p, CTUI_PICKER_FILE, "."));

	screen_clear();
	ctui_picker_draw(&p);
	CHECK(!screen_has_text(" Q "));

	p.style = &style;
	screen_clear();
	ctui_picker_draw(&p);
	CHECK(screen_has_text(" Q "));
	ctui_picker_free(&p);
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_list);
	RUN(test_burger);
	RUN(test_dialog);
	RUN(test_toast);
	RUN(test_tree);
	RUN(test_textview);
	RUN(test_waveform);
	RUN(test_waveform_filled);
	RUN(test_panel);
	RUN(test_tabpanel);
	RUN(test_wizard);
	RUN(test_form);
	RUN(test_picker);
	endwin();
	return CHECK_DONE();
}
