/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * terminal.h - an embedded terminal: runs a program on a pty and shows its
 * screen inside a rectangle of the TUI.
 *
 * The program's output is interpreted only by the bounded `vt` emulator
 * (ctui/vt.h); the widget draws the vt grid with colours quantised to the
 * terminal's palette (truecolour to the xterm 256 cube), bold / dim /
 * italic / underline / reverse mapped to curses attributes, and the cursor
 * as a reverse cell. Colour pairs come from a pool of its own at the top of
 * the first 256 pairs; when the pool (or COLOR_PAIRS) is used up the
 * remaining colour combinations are drawn without colour.
 *
 * Security: the widget runs an arbitrary program chosen by the caller (argv
 * and environment are passed to execvpe as given, TERM is set to
 * xterm-256color) and shows its output. It does no network or privilege
 * handling: the child has the privileges of the caller. Nothing the program
 * prints reaches the real terminal: escape sequences are parsed by vt and
 * only cells are drawn. Answers to the program's queries (cursor position,
 * device attributes) come from vt and are limited to a few bytes.
 *
 * Ownership: after ctui_terminal_start() the widget owns the pty master and
 * the child's pid. ctui_terminal_free() hangs the child up (SIGHUP to its
 * process group, then SIGKILL after a short grace period), closes the pty
 * and reaps the child: no zombie and no orphan is left. A child that exits
 * by itself is reaped by ctui_terminal_pump() and the widget goes to the
 * "exited" state (status kept) until it is freed or started again.
 *
 * Frame loop: call ctui_terminal_pump() once per frame (it reads at most
 * CTUI_TERMINAL_FRAME_BYTES, so a program flooding the output cannot freeze
 * the UI), then ctui_terminal_draw(); ctui_terminal_wait() can sleep until
 * output arrives. The draw call resizes the pty (TIOCSWINSZ, so the program
 * gets SIGWINCH) when the rectangle changed; the rectangle is one column
 * wider than the terminal, that column holds the scrollbar of the history.
 *
 * Keys: ctui_terminal_key() translates curses key codes into xterm
 * sequences: printable bytes as they come (UTF-8 arrives byte by byte), Enter
 * as CR, Backspace as DEL, arrows / Home / End (application cursor mode
 * aware), Ins / Del / PgUp / PgDn, F1-F12 and, where curses reports them,
 * the Shift and Ctrl variants (KEY_F(13).. are Shift+F1.., KEY_F(25).. are
 * Ctrl+F1..; call ctui_terminal_register_keys() once after initscr() to also
 * receive Shift / Alt / Ctrl arrows, Home, End and Ins / Del / PgUp / PgDn,
 * which curses otherwise leaves as unknown escape sequences). Alt is
 * the ESC prefix. Shift+PgUp / Shift+PgDn (KEY_SPREVIOUS / KEY_SNEXT) scroll
 * the local history instead of reaching the program. Any other key brings
 * the view back to the live screen. Pass the Enter key as '\n' or as
 * KEY_ENTER: curses maps CR to '\n' unless nonl() was called.
 *
 * Mouse: when the program enabled mouse reporting (1000 / 1002 / 1003) the
 * events are forwarded in the SGR (1006) or the legacy X10 encoding; when it
 * did not, the wheel scrolls the local history and the scrollbar can be
 * dragged.
 *
 * Not supported: selection and copy, an alternate-screen wheel emulation,
 * reflow on resize (see vt.h for the escape sequences that are ignored).
 */
#ifndef CTUI_TERMINAL_H
#define CTUI_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include <ctui/scrollbar.h>
#include <ctui/vt.h>

#define CTUI_TERMINAL_FRAME_BYTES                                              \
	65536 /* bytes read per ctui_terminal_pump() by default */
#define CTUI_TERMINAL_KEY_BASE                                                 \
	0x4000 /* codes made by ctui_terminal_register_keys() */

typedef struct ctui_terminal {
	ctui_vt *vt;	 /* the emulator, owned; NULL before start */
	int fd;		 /* pty master, owned; -1 when none */
	pid_t pid;	 /* the child; 0 when none */
	bool reaped;	 /* the child was waited for; wait_status is valid */
	bool exited;	 /* reaped and its output read to the end */
	bool eof;	 /* the pty reported the end of the output */
	int wait_status; /* as set by waitpid() */
	int rows, cols;	 /* size of the screen and of the pty */
	size_t view;	 /* lines the view is scrolled back into the history */
	ctui_scrollbar bar;
	int oy, ox; /* origin of the last draw */
	int button; /* mouse button held (0 left, 1 middle, 2 right), -1 none */
	bool bar_drag;
	size_t scrollback;
} ctui_terminal;

typedef enum ctui_terminal_mouse_kind {
	CTUI_TERMINAL_PRESS,
	CTUI_TERMINAL_RELEASE,
	CTUI_TERMINAL_DRAG,
	CTUI_TERMINAL_WHEEL_UP,
	CTUI_TERMINAL_WHEEL_DOWN
} ctui_terminal_mouse_kind;

/* Zero state. */
void ctui_terminal_init(ctui_terminal *t);

/* Run argv (NULL or empty: $SHELL, else /bin/sh) in a rows x cols pty, with
 * the environment envp (NULL-terminated "NAME=value" strings; NULL inherits
 * the caller's) plus TERM=xterm-256color. A running terminal is stopped
 * first. False (nothing running) when the sizes are out of range or the
 * fork fails; an exec failure shows as an exited child with status 127. */
bool ctui_terminal_start(ctui_terminal *t, const char *const argv[],
			 const char *const envp[], int rows, int cols,
			 size_t scrollback);

/* Hang up, kill if needed, reap, close and free everything. Safe to repeat. */
void ctui_terminal_free(ctui_terminal *t);

/* True while the child runs (started and not yet exited). */
bool ctui_terminal_running(const ctui_terminal *t);

/* True once the child has ended and its output was read; *code receives the
 * exit code (-1 when killed by a signal) and *sig the signal (0 otherwise);
 * either may be NULL. */
bool ctui_terminal_exited(const ctui_terminal *t, int *code, int *sig);

/* Read up to max_bytes (0 = CTUI_TERMINAL_FRAME_BYTES) of output into vt,
 * answer its queries and notice the child's end. True when the screen may
 * have changed. */
bool ctui_terminal_pump(ctui_terminal *t, size_t max_bytes);

/* Wait up to ms milliseconds for output; true when there is some. */
bool ctui_terminal_wait(ctui_terminal *t, int ms);

/* Send bytes to the program; false when it is gone. Blocks a second at most. */
bool ctui_terminal_write(ctui_terminal *t, const void *data, size_t len);

/* Send text as a paste: wrapped in bracketed-paste markers when the program
 * asked for them (and then without escape bytes). */
bool ctui_terminal_paste(ctui_terminal *t, const char *text, size_t len);

/* Send a curses key code (alt: with the Alt key). True when it was used. */
bool ctui_terminal_key(ctui_terminal *t, int key, bool alt);

/* Make curses report Shift / Alt / Ctrl arrows, Home, End, Ins, Del, PgUp
 * and PgDn. A sequence the terminfo entry already names keeps its code
 * (KEY_SRIGHT, ...); the others get a code from CTUI_TERMINAL_KEY_BASE up, so
 * nothing else that reads the keyboard changes. Call after initscr() with
 * keypad enabled. */
void ctui_terminal_register_keys(void);

/* A mouse event at (y, x) relative to the rectangle given to
 * ctui_terminal_draw(); button 0 left, 1 middle, 2 right. True when used. */
bool ctui_terminal_mouse(ctui_terminal *t, ctui_terminal_mouse_kind kind,
			 int button, int y, int x);

/* Scroll the view back (positive) or forward (negative) by lines. */
void ctui_terminal_scroll(ctui_terminal *t, long lines);

/* Resize the screen and the pty. */
bool ctui_terminal_resize(ctui_terminal *t, int rows, int cols);

/* Draw in the rectangle; the pty follows its size (one column less for the
 * scrollbar). focused draws the cursor. */
void ctui_terminal_draw(ctui_terminal *t, int y, int x, int width, int height,
			bool focused);

/* Pure helpers, public for testing. The key encoder writes at most 16 bytes
 * and returns their count (0 = not a key of the terminal); the mouse encoder
 * at most 32 (0 = the program asked for nothing of the sort). */
size_t ctui_terminal_encode_key(int key, bool alt, bool app_cursor,
				char out[16]);
size_t ctui_terminal_encode_mouse(const ctui_vt_modes *modes,
				  ctui_terminal_mouse_kind kind, int button,
				  int y, int x, char out[32]);

/* Nearest xterm-256 index (cube or grey ramp) of an RGB colour. */
int ctui_terminal_quantize_rgb(int r, int g, int b);

#endif /* CTUI_TERMINAL_H */
