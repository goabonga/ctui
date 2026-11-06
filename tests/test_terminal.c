/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * Spawns only /bin/sh with fixed harmless commands. Every wait has a
 * deadline and the whole program has an alarm, so a hung child cannot stall
 * make check; every child is reaped by ctui_terminal_free().
 */
#include "check.h"
#include "screen.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <ctui/terminal.h>

#define WAIT_MS 8000

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static bool screen_contains(const ctui_terminal *t, const char *needle)
{
	char row[512];
	for (int r = 0; r < t->rows; r++) {
		ctui_vt_row_text(t->vt, r, row, sizeof row);
		if (strstr(row, needle))
			return true;
	}
	return false;
}

/* Pump until the needle shows on the vt screen; false after WAIT_MS. */
static bool wait_for(ctui_terminal *t, const char *needle)
{
	long end = now_ms() + WAIT_MS;
	while (now_ms() < end) {
		ctui_terminal_pump(t, 0);
		if (screen_contains(t, needle))
			return true;
		ctui_terminal_wait(t, 20);
	}
	return false;
}

static bool wait_exit(ctui_terminal *t)
{
	long end = now_ms() + WAIT_MS;
	while (now_ms() < end) {
		ctui_terminal_pump(t, 0);
		if (t->exited)
			return true;
		ctui_terminal_wait(t, 20);
	}
	return false;
}

static bool sh(ctui_terminal *t, const char *cmd, int rows, int cols)
{
	const char *argv[] = {"/bin/sh", "-c", cmd, NULL};
	return ctui_terminal_start(t, argv, NULL, rows, cols, 200);
}

static bool gone(pid_t pid)
{
	return waitpid(pid, NULL, WNOHANG) == -1 && errno == ECHILD &&
	       kill(pid, 0) == -1 && errno == ESRCH;
}

/* True when a process of the group is still alive. Zombies do not count: a
 * killed child is reparented to init, which may never reap it (a container
 * without an init keeps them forever). Without /proc, any entry counts. */
static bool group_alive(pid_t pgrp)
{
	if (kill(-pgrp, 0) == -1 && errno == ESRCH)
		return false;
	DIR *d = opendir("/proc");
	if (!d)
		return true;
	bool alive = false;
	for (struct dirent *e = readdir(d); e && !alive; e = readdir(d)) {
		char path[64], buf[512];
		snprintf(path, sizeof path, "/proc/%.20s/stat", e->d_name);
		FILE *f = fopen(path, "r");
		if (!f)
			continue;
		size_t n = fread(buf, 1, sizeof buf - 1, f);
		fclose(f);
		buf[n] = '\0';
		/* after "pid (comm)": state, ppid, pgrp */
		const char *p = strrchr(buf, ')');
		if (!p || p[1] != ' ' || !p[2] || p[3] != ' ')
			continue;
		char *end;
		long ppid = strtol(p + 4, &end, 10);
		long group = strtol(end, NULL, 10);
		if (ppid >= 0 && group == (long)pgrp && p[2] != 'Z')
			alive = true;
	}
	closedir(d);
	return alive;
}

/* No live process of the group is left. */
static bool group_gone(pid_t pid)
{
	for (int i = 0; i < 200; i++) {
		if (!group_alive(pid))
			return true;
		usleep(10000);
	}
	return false;
}

static void test_output_and_exit(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "printf 'hello\\nworld\\n'", 10, 40));
	CHECK(ctui_terminal_running(&t));
	CHECK(wait_for(&t, "world"));
	CHECK(screen_contains(&t, "hello"));
	CHECK(wait_exit(&t));
	int code = -2, sig = -2;
	CHECK(ctui_terminal_exited(&t, &code, &sig) && code == 0 && sig == 0);
	CHECK(!ctui_terminal_running(&t) && t.fd == -1);
	pid_t pid = t.pid;
	ctui_terminal_free(&t);
	CHECK(gone(pid));
	ctui_terminal_free(&t); /* safe to repeat */

	CHECK(sh(&t, "exit 7", 5, 20));
	CHECK(wait_exit(&t));
	CHECK(ctui_terminal_exited(&t, &code, &sig) && code == 7 && sig == 0);
	pid = t.pid;
	CHECK(gone(pid)); /* reaped by the widget itself */
	ctui_terminal_free(&t);

	CHECK(sh(&t, "kill -9 $$", 5, 20));
	CHECK(wait_exit(&t));
	CHECK(ctui_terminal_exited(&t, &code, &sig) && code == -1 &&
	      sig == SIGKILL);
	ctui_terminal_free(&t);

	const char *missing[] = {"/nonexistent/ctui-test-program", NULL};
	CHECK(ctui_terminal_start(&t, missing, NULL, 5, 40, 0));
	CHECK(wait_exit(&t));
	CHECK(ctui_terminal_exited(&t, &code, &sig) && code == 127);
	ctui_terminal_free(&t);

	CHECK(!ctui_terminal_start(&t, missing, NULL, 0, 40, 0));
	CHECK(!ctui_terminal_start(&t, missing, NULL, 5, CTUI_VT_MAX_COLS + 1,
				   0));
	CHECK(!ctui_terminal_running(&t));
}

static void test_environment_and_default_shell(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	const char *argv[] = {"/bin/sh", "-c", "echo [$FOO][$TERM]", NULL};
	const char *env[] = {"FOO=bar", "TERM=dumb", NULL};
	CHECK(ctui_terminal_start(&t, argv, env, 5, 40, 0));
	CHECK(wait_for(&t, "[bar][xterm-256color]"));
	ctui_terminal_free(&t);

	setenv("SHELL", "/bin/sh", 1);
	CHECK(ctui_terminal_start(&t, NULL, NULL, 8, 40, 0));
	CHECK(ctui_terminal_write(&t, "echo ab$((20+3))\nexit 5\n", 24));
	CHECK(wait_for(&t, "ab23"));
	CHECK(wait_exit(&t));
	int code = 0;
	CHECK(ctui_terminal_exited(&t, &code, NULL) && code == 5);
	ctui_terminal_free(&t);
}

static void test_utf8_and_colors_in_grid(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t,
		 "printf 'caf\\303\\251 \\346\\227\\245\\033[1;31mR\\033[0m'",
		 5, 40));
	CHECK(wait_exit(&t));
	char row[128];
	ctui_vt_row_text(t.vt, 0, row, sizeof row);
	CHECK_STR(row, "caf\xc3\xa9 \xe6\x97\xa5R");
	CHECK(ctui_vt_cell_at(t.vt, 0, 7)->fg == CTUI_VT_INDEXED(1));
	ctui_terminal_free(&t);
}

static void test_resize_propagates(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(
	    &t,
	    "trap 'stty size' WINCH; stty size; while :; do sleep 0.05; done",
	    10, 40));
	CHECK(wait_for(&t, "10 40"));
	CHECK(ctui_terminal_resize(&t, 7, 33));
	CHECK(wait_for(&t, "7 33"));
	CHECK(ctui_vt_rows(t.vt) == 7 && ctui_vt_cols(t.vt) == 33);
	pid_t pid = t.pid;
	ctui_terminal_free(&t);
	CHECK(gone(pid));
	CHECK(group_gone(pid)); /* nothing of its process group is left */
}

static void test_draw_resizes(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(
	    &t,
	    "trap 'stty size' WINCH; stty size; while :; do sleep 0.05; done",
	    10, 40));
	CHECK(wait_for(&t, "10 40"));
	screen_clear();
	ctui_terminal_draw(
	    &t, 1, 2, 31, 8,
	    true); /* 30 columns for the program, one for the scrollbar */
	CHECK(t.rows == 8 && t.cols == 30);
	CHECK(wait_for(&t, "8 30"));
	ctui_terminal_free(&t);
}

static void test_keys_through_cat(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "cat", 10, 40));
	CHECK(ctui_terminal_key(&t, 'h', false));
	CHECK(ctui_terminal_key(&t, 'i', false));
	CHECK(ctui_terminal_key(&t, '\n', false));
	char row[64] = "";
	long end = now_ms() + WAIT_MS;
	while (now_ms() <
	       end) { /* the echo on row 0, the line cat sent back on row 1 */
		ctui_terminal_pump(&t, 0);
		ctui_vt_row_text(t.vt, 1, row, sizeof row);
		if (row[0])
			break;
		ctui_terminal_wait(&t, 20);
	}
	CHECK_STR(row, "hi");
	ctui_vt_row_text(t.vt, 0, row, sizeof row);
	CHECK_STR(row, "hi");
	CHECK(ctui_terminal_key(&t, 4, false)); /* Ctrl-D */
	CHECK(wait_exit(&t));
	int code = -1;
	CHECK(ctui_terminal_exited(&t, &code, NULL) && code == 0);
	CHECK(!ctui_terminal_key(&t, 'x', false)); /* nobody listens any more */
	ctui_terminal_free(&t);
}

static void test_special_keys_reach_the_program(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "stty -echo; echo ready; cat -v", 10, 60));
	CHECK(wait_for(&t, "ready"));
	ctui_terminal_key(&t, KEY_UP, false);
	ctui_terminal_key(&t, KEY_F(5), false);
	ctui_terminal_key(&t, 'x', true);
	ctui_terminal_key(&t, KEY_DC, false);
	ctui_terminal_key(&t, '\n', false);
	CHECK(wait_for(&t, "^[[A^[[15~^[x^[[3~"));
	ctui_terminal_free(&t);

	/* application cursor keys follow the mode the program set */
	CHECK(sh(&t, "printf '\\033[?1h'; stty -echo; echo ready; cat -v", 10,
		 60));
	CHECK(wait_for(&t, "ready"));
	ctui_terminal_key(&t, KEY_UP, false);
	ctui_terminal_key(&t, '\n', false);
	CHECK(wait_for(&t, "^[OA"));
	ctui_terminal_free(&t);
}

static void test_paste(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "printf '\\033[?2004h'; stty -echo; echo ready; cat -v",
		 10, 60));
	CHECK(wait_for(&t, "ready"));
	CHECK(t.vt && ctui_vt_get_modes(t.vt)->bracketed_paste);
	CHECK(ctui_terminal_paste(&t, "a\033b\n", 4));
	CHECK(wait_for(
	    &t, "^[[200~ab")); /* the escape inside the paste is dropped */
	ctui_terminal_key(&t, '\n', false);
	CHECK(wait_for(&t, "^[[201~"));
	ctui_terminal_free(&t);
}

static void test_mouse_forwarding(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(
	    &t,
	    "printf '\\033[?1000h\\033[?1006h'; stty -echo; echo ready; cat -v",
	    10, 60));
	CHECK(wait_for(&t, "ready"));
	CHECK(ctui_vt_get_modes(t.vt)->mouse == 1000);
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_PRESS, 0, 2, 3));
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_RELEASE, 0, 2, 3));
	CHECK(!ctui_terminal_mouse(&t, CTUI_TERMINAL_DRAG, 0, 2,
				   3)); /* 1000 has no motion */
	CHECK(!ctui_terminal_mouse(&t, CTUI_TERMINAL_PRESS, 0, 99,
				   3)); /* outside */
	ctui_terminal_key(&t, '\n', false);
	CHECK(wait_for(&t, "^[[<0;4;3M^[[<0;4;3m"));
	ctui_terminal_free(&t);
}

static void test_scrollback_and_view(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t,
		 "i=1; while [ $i -le 60 ]; do echo line$i; i=$((i+1)); done",
		 5, 20));
	CHECK(wait_exit(&t));
	CHECK(ctui_vt_history_count(t.vt) >= 50);
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 21, 5, true);
	CHECK(screen_has_text("line60") && !screen_has_text("line54"));
	CHECK(ctui_scrollbar_needed(&t.bar));
	ctui_terminal_scroll(&t, 3);
	CHECK(t.view == 3);
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 21, 5, true);
	CHECK(screen_has_text("line54") && !screen_has_text("line60"));
	/* the wheel scrolls when the program did not ask for the mouse */
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_WHEEL_UP, 0, 1, 1));
	CHECK(t.view == 6);
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_WHEEL_DOWN, 0, 1, 1));
	CHECK(t.view == 3);
	ctui_terminal_scroll(&t, -100);
	CHECK(t.view == 0);
	ctui_terminal_scroll(&t, 100000);
	CHECK(t.view == ctui_vt_history_count(t.vt));
	/* Shift+PgUp / Shift+PgDn page through the history, other keys go live
	 */
	ctui_terminal_scroll(&t, -100000);
	CHECK(ctui_terminal_key(&t, KEY_SPREVIOUS, false) && t.view == 4);
	CHECK(ctui_terminal_key(&t, KEY_SNEXT, false) && t.view == 0);
	/* dragging the scrollbar thumb */
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 21, 5, true);
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_PRESS, 0, 4,
				  20)); /* thumb at the bottom */
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_DRAG, 0, 0, 20));
	CHECK(t.view > 0);
	CHECK(ctui_terminal_mouse(&t, CTUI_TERMINAL_RELEASE, 0, 0, 20) &&
	      !t.bar_drag);
	ctui_terminal_free(&t);
}

static void test_draw_colors_and_pair_budget(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t,
		 "printf '\\033[31mred\\033[0m \\033[48;2;0;255;0mgrn\\033[0m "
		 "\\033[7mrev\\033[0m'",
		 5, 40));
	CHECK(wait_exit(&t));
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 41, 5, true);
	CHECK(screen_has_text("red"));
	cchar_t a, b;
	wchar_t wc[CCHARW_MAX];
	attr_t attrs;
	short pa, pb;
	mvin_wch(0, 0, &a);
	mvin_wch(0, 4, &b);
	getcchar(&a, wc, &attrs, &pa, NULL);
	getcchar(&b, wc, &attrs, &pb, NULL);
	CHECK(pa != pb);
	mvin_wch(0, 8, &a);
	getcchar(&a, wc, &attrs, &pa, NULL);
	CHECK(attrs & A_REVERSE);
	ctui_terminal_free(&t);

	/* far more colour combinations than pairs: still draws, without
	 * crashing */
	CHECK(sh(
	    &t,
	    "i=16; while [ $i -lt 232 ]; do printf "
	    "\"\\033[48;5;${i}m\\033[38;5;$((255-i%200))mx\"; i=$((i+1)); done",
	    10, 79));
	CHECK(wait_exit(&t));
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 80, 10, true);
	CHECK(screen_has_text("xxxxxxxx"));
	ctui_terminal_free(&t);
}

static void test_cursor_cell(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "printf ab; sleep 5", 5, 20));
	CHECK(wait_for(&t, "ab"));
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 21, 5, true);
	cchar_t c;
	wchar_t wc[CCHARW_MAX];
	attr_t attrs;
	short pair;
	mvin_wch(0, 2, &c); /* the cursor is after "ab" */
	getcchar(&c, wc, &attrs, &pair, NULL);
	CHECK(attrs & A_REVERSE);
	screen_clear();
	ctui_terminal_draw(&t, 0, 0, 21, 5, false);
	mvin_wch(0, 2, &c);
	getcchar(&c, wc, &attrs, &pair, NULL);
	CHECK(!(attrs & A_REVERSE));
	ctui_terminal_free(&t);
}

static void test_answers_to_queries(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t,
		 "stty -echo -icanon min 1; printf '\\033[6n'; dd bs=1 count=6 "
		 "2>/dev/null | od -An -c; echo done",
		 10, 60));
	CHECK(wait_for(&t, "1   ;   1   R"));
	ctui_terminal_free(&t);
}

static void test_bounded_pump(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "yes", 20, 80));
	CHECK(ctui_terminal_wait(&t, WAIT_MS));
	long end = now_ms() + 2000;
	while (now_ms() < end && ctui_vt_generation(t.vt) == 0) {
		ctui_terminal_wait(&t, 20);
		ctui_terminal_pump(&t,
				   99); /* two bytes per line: about 49 lines */
	}
	int ys = 0;
	for (int r = 0; r < 20; r++)
		for (int c = 0; c < 80; c++)
			ys += ctui_vt_cell_at(t.vt, r, c)->cp == 'y';
	CHECK(ys > 0 && ys <= 99);
	CHECK(ctui_terminal_running(&t));
	ctui_terminal_free(&t);
}

static void test_free_kills_stubborn_children(void)
{
	ctui_terminal t;
	ctui_terminal_init(&t);
	CHECK(sh(&t, "trap '' HUP; echo ready; while :; do sleep 1; done", 5,
		 30));
	CHECK(wait_for(&t, "ready"));
	pid_t pid = t.pid;
	long start = now_ms();
	ctui_terminal_free(&t);
	CHECK(now_ms() - start < 5000);
	CHECK(gone(pid));
	CHECK(group_gone(pid));
	CHECK(t.vt == NULL && t.fd == -1 && t.pid == 0);

	/* restarting reuses the structure and stops the previous child */
	CHECK(sh(&t, "sleep 30", 5, 30));
	pid = t.pid;
	CHECK(sh(&t, "echo second", 5, 30));
	CHECK(gone(pid));
	CHECK(wait_for(&t, "second"));
	ctui_terminal_free(&t);
}

static void test_pure_helpers(void)
{
	CHECK(ctui_terminal_quantize_rgb(0, 0, 0) == 16);
	CHECK(ctui_terminal_quantize_rgb(255, 255, 255) == 231);
	CHECK(ctui_terminal_quantize_rgb(255, 0, 0) == 196);
	CHECK(ctui_terminal_quantize_rgb(95, 135, 175) == 67);
	CHECK(ctui_terminal_quantize_rgb(128, 128, 128) == 244);
	CHECK(ctui_terminal_quantize_rgb(-5, 999, 0) == 46);

	static const struct {
		int key;
		bool alt, app;
		const char *want;
	} KEYS[] = {
	    {KEY_UP, false, false, "\033[A"},
	    {KEY_UP, false, true, "\033OA"},
	    {KEY_DOWN, false, false, "\033[B"},
	    {KEY_RIGHT, false, false, "\033[C"},
	    {KEY_LEFT, false, false, "\033[D"},
	    {KEY_HOME, false, false, "\033[H"},
	    {KEY_HOME, false, true, "\033OH"},
	    {KEY_END, false, false, "\033[F"},
	    {KEY_IC, false, false, "\033[2~"},
	    {KEY_DC, false, false, "\033[3~"},
	    {KEY_PPAGE, false, false, "\033[5~"},
	    {KEY_NPAGE, false, false, "\033[6~"},
	    {KEY_F(1), false, false, "\033OP"},
	    {KEY_F(4), false, false, "\033OS"},
	    {KEY_F(5), false, false, "\033[15~"},
	    {KEY_F(10), false, false, "\033[21~"},
	    {KEY_F(12), false, false, "\033[24~"},
	    {KEY_F(13), false, false, "\033[1;2P"},
	    {KEY_F(17), false, false, "\033[15;2~"},
	    {KEY_F(25), false, false, "\033[1;5P"},
	    {KEY_F(1), true, false, "\033[1;3P"},
	    {KEY_SLEFT, false, false, "\033[1;2D"},
	    {KEY_SRIGHT, false, true, "\033[1;2C"},
	    {KEY_SR, false, false, "\033[1;2A"},
	    {KEY_SDC, false, false, "\033[3;2~"},
	    {KEY_UP, true, false, "\033[1;3A"},
	    {KEY_DC, true, false, "\033[3;3~"},
	    {'a', false, false, "a"},
	    {'a', true, false, "\033a"},
	    {'\n', false, false, "\r"},
	    {3, false, false, "\003"},
	    {0xc3, false, false, "\xc3"},
	    {KEY_BACKSPACE, false, false, "\177"},
	    {KEY_ENTER, false, false, "\r"},
	    {KEY_BTAB, false, false, "\033[Z"},
	    {CTUI_TERMINAL_KEY_BASE + 5 * 16 + 0, false, false, "\033[1;5A"},
	    {CTUI_TERMINAL_KEY_BASE + 3 * 16 + 7, false, false, "\033[3;3~"},
	    {CTUI_TERMINAL_KEY_BASE + 5 * 16 + 5, true, false, "\033[1;7F"},
	};
	for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
		char out[16];
		size_t n = ctui_terminal_encode_key(KEYS[i].key, KEYS[i].alt,
						    KEYS[i].app, out);
		out[n] = '\0';
		if (strcmp(out, KEYS[i].want) != 0)
			fprintf(stderr, "key %d: got n=%zu\n", KEYS[i].key, n);
		CHECK_STR(out, KEYS[i].want);
	}
	char out[16];
	/* registered keys: the codes terminfo already gave stay, the rest are
	 * new */
	int shift_right = key_defined("\033[1;2C");
	ctui_terminal_register_keys();
	CHECK(key_defined("\033[1;2C") == shift_right);
	CHECK(key_defined("\033[1;5A") > 0 && key_defined("\033[1;8F") > 0);
	size_t n = ctui_terminal_encode_key(key_defined("\033[1;5A"), false,
					    false, out);
	out[n] = '\0';
	CHECK_STR(out, "\033[1;5A");
	n = ctui_terminal_encode_key(key_defined("\033[3;7~"), false, false,
				     out);
	out[n] = '\0';
	CHECK_STR(out, "\033[3;7~");
	CHECK(ctui_terminal_encode_key(KEY_MOUSE, false, false, out) == 0);
	CHECK(ctui_terminal_encode_key(KEY_RESIZE, false, false, out) == 0);
	CHECK(ctui_terminal_encode_key(-1, false, false, out) == 0);
	CHECK(ctui_terminal_encode_key(CTUI_TERMINAL_KEY_BASE + 0x1F, false,
				       false, out) == 0);

	ctui_vt *vt = ctui_vt_new(5, 5, 0);
	char m[32];
	CHECK(ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
					 CTUI_TERMINAL_PRESS, 0, 2, 3, m) == 0);
	const char *on = "\033[?1000h\033[?1006h";
	ctui_vt_feed(vt, on, strlen(on));
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_PRESS, 0, 2, 3, m);
	m[n] = '\0';
	CHECK_STR(m, "\033[<0;4;3M");
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_RELEASE, 2, 0, 0, m);
	m[n] = '\0';
	CHECK_STR(m, "\033[<2;1;1m");
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_WHEEL_UP, 0, 0, 0, m);
	m[n] = '\0';
	CHECK_STR(m, "\033[<64;1;1M");
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_WHEEL_DOWN, 0, 0, 0, m);
	m[n] = '\0';
	CHECK_STR(m, "\033[<65;1;1M");
	CHECK(ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
					 CTUI_TERMINAL_DRAG, 0, 1, 1, m) == 0);
	const char *motion = "\033[?1002h";
	ctui_vt_feed(vt, motion, strlen(motion));
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_DRAG, 0, 1, 1, m);
	m[n] = '\0';
	CHECK_STR(m, "\033[<32;2;2M");
	const char *legacy = "\033[?1006l";
	ctui_vt_feed(vt, legacy, strlen(legacy));
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_PRESS, 0, 2, 3, m);
	CHECK(n == 6 && memcmp(m, "\033[M \x24\x23", 6) == 0);
	n = ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
				       CTUI_TERMINAL_RELEASE, 0, 2, 3, m);
	CHECK(n == 6 && m[3] == '#');
	CHECK(ctui_terminal_encode_mouse(ctui_vt_get_modes(vt),
					 CTUI_TERMINAL_PRESS, 0, 2, 300,
					 m) == 0);
	ctui_vt_free(vt);
}

int main(void)
{
	alarm(240); /* last resort: SIGALRM ends a hung run */
	screen_open(24, 80);
	keypad(stdscr, TRUE);
	RUN(test_pure_helpers);
	RUN(test_output_and_exit);
	RUN(test_environment_and_default_shell);
	RUN(test_utf8_and_colors_in_grid);
	RUN(test_resize_propagates);
	RUN(test_draw_resizes);
	RUN(test_keys_through_cat);
	RUN(test_special_keys_reach_the_program);
	RUN(test_paste);
	RUN(test_mouse_forwarding);
	RUN(test_scrollback_and_view);
	RUN(test_draw_colors_and_pair_budget);
	RUN(test_cursor_cell);
	RUN(test_answers_to_queries);
	RUN(test_bounded_pump);
	RUN(test_free_kills_stubborn_children);
	return CHECK_DONE();
}
