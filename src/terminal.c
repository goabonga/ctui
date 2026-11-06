/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * terminal.c - the terminal widget: a child on a pty, shown through vt.
 */
#ifndef _GNU_SOURCE
// NOLINTNEXTLINE(bugprone-reserved-identifier): feature test macro
#define _GNU_SOURCE
#endif
#include <ctui/terminal.h>

#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <ctui/text.h>
#include <ctui/theme.h>

extern char **environ;

#define WHEEL_LINES 3
#define POOL_FIRST 160 /* pairs of our own: the top of the first 256 */
#define POOL_MAX 96

/* ---- keys ---- */

enum {
	K_UP,
	K_DOWN,
	K_RIGHT,
	K_LEFT,
	K_HOME,
	K_END,
	K_INS,
	K_DEL,
	K_PGUP,
	K_PGDN
};

static size_t special_key(int idx, int bits, bool app, char out[16])
{
	static const char LETTERS[] = "ABCDHF";
	static const int TILDE[] = {2, 3, 5, 6};
	int mod = bits + 1;
	int n;
	if (idx < 6) {
		if (mod > 1)
			n = snprintf(out, 16, "\033[1;%d%c", mod, LETTERS[idx]);
		else
			n = snprintf(out, 16, app ? "\033O%c" : "\033[%c",
				     LETTERS[idx]);
	} else if (mod > 1) {
		n = snprintf(out, 16, "\033[%d;%d~", TILDE[idx - 6], mod);
	} else {
		n = snprintf(out, 16, "\033[%d~", TILDE[idx - 6]);
	}
	return (size_t)n;
}

static const struct {
	int key, idx, bits;
} KEYS[] = {
    {KEY_UP, K_UP, 0},		{KEY_DOWN, K_DOWN, 0},	{KEY_RIGHT, K_RIGHT, 0},
    {KEY_LEFT, K_LEFT, 0},	{KEY_HOME, K_HOME, 0},	{KEY_END, K_END, 0},
    {KEY_IC, K_INS, 0},		{KEY_DC, K_DEL, 0},	{KEY_PPAGE, K_PGUP, 0},
    {KEY_NPAGE, K_PGDN, 0},	{KEY_SR, K_UP, 1},	{KEY_SF, K_DOWN, 1},
    {KEY_SRIGHT, K_RIGHT, 1},	{KEY_SLEFT, K_LEFT, 1}, {KEY_SHOME, K_HOME, 1},
    {KEY_SEND, K_END, 1},	{KEY_SIC, K_INS, 1},	{KEY_SDC, K_DEL, 1},
    {KEY_SPREVIOUS, K_PGUP, 1}, {KEY_SNEXT, K_PGDN, 1},
};

/* Curses codes of the modified keys, by xterm modifier and key; 0 = none. */
static int g_registered[9][10];

size_t ctui_terminal_encode_key(int key, bool alt, bool app_cursor,
				char out[16])
{
	int altbit = alt ? 2 : 0;
	if (key < 0)
		return 0;
	if (key >= CTUI_TERMINAL_KEY_BASE &&
	    key < CTUI_TERMINAL_KEY_BASE + 0x100) {
		int mod = (int)((unsigned)(key - CTUI_TERMINAL_KEY_BASE) >> 4U),
		    idx = (int)((unsigned)(key - CTUI_TERMINAL_KEY_BASE) & 15U);
		if (mod < 2 || mod > 8 || idx > K_PGDN)
			return 0;
		return special_key(
		    idx, (int)((unsigned)(mod - 1) | (unsigned)altbit),
		    app_cursor, out);
	}
	if (key < 256) {
		size_t n = 0;
		if (alt)
			out[n++] = '\033';
		out[n++] = key == '\n' ? '\r' : (char)key;
		return n;
	}
	for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
		if (KEYS[i].key == key)
			return special_key(
			    KEYS[i].idx,
			    (int)((unsigned)KEYS[i].bits | (unsigned)altbit),
			    app_cursor, out);
	for (int mod = 2; mod <= 8; mod++)
		for (int idx = 0; idx <= K_PGDN; idx++)
			if (g_registered[mod][idx] == key)
				return special_key(idx,
						   (int)((unsigned)(mod - 1) |
							 (unsigned)altbit),
						   app_cursor, out);
	if (key == KEY_BACKSPACE)
		return (size_t)snprintf(out, 16, alt ? "\033\177" : "\177");
	if (key == KEY_ENTER)
		return (size_t)snprintf(out, 16, alt ? "\033\r" : "\r");
	if (key == KEY_BTAB)
		return (size_t)snprintf(out, 16, "\033[Z");
	if (key >= KEY_F(1) && key <= KEY_F(48)) {
		static const int TILDE_F[] = {15, 17, 18, 19, 20, 21, 23, 24};
		static const int GROUP_BITS[] = {0, 1, 4, 5};
		int n = key - KEY_F0 - 1;
		int f = n % 12, bits = (int)((unsigned)GROUP_BITS[n / 12] |
					     (unsigned)altbit);
		if (f < 4) {
			if (bits)
				return (size_t)snprintf(out, 16, "\033[1;%d%c",
							bits + 1, "PQRS"[f]);
			return (size_t)snprintf(out, 16, "\033O%c", "PQRS"[f]);
		}
		if (bits)
			return (size_t)snprintf(out, 16, "\033[%d;%d~",
						TILDE_F[f - 4], bits + 1);
		return (size_t)snprintf(out, 16, "\033[%d~", TILDE_F[f - 4]);
	}
	return 0;
}

void ctui_terminal_register_keys(void)
{
	static const char FINALS[] = "ABCDHF";
	static const int TILDE[] = {2, 3, 5, 6};
	for (int mod = 2; mod <= 8; mod++)
		for (int idx = 0; idx <= K_PGDN; idx++) {
			char seq[24];
			if (idx < 6)
				snprintf(seq, sizeof seq, "\033[1;%d%c", mod,
					 FINALS[idx]);
			else
				snprintf(seq, sizeof seq, "\033[%d;%d~",
					 TILDE[idx - 6], mod);
			/* a sequence the terminfo entry already names keeps its
			 * code, so the other users of the keyboard are not
			 * disturbed */
			int known = key_defined(seq);
			if (known <= 0) {
				known = CTUI_TERMINAL_KEY_BASE + mod * 16 + idx;
				define_key(seq, known);
			}
			g_registered[mod][idx] = known;
		}
}

/* ---- mouse ---- */

size_t ctui_terminal_encode_mouse(const ctui_vt_modes *modes,
				  ctui_terminal_mouse_kind kind, int button,
				  int y, int x, char out[32])
{
	if (!modes || modes->mouse == 0 || x < 0 || y < 0 || button < 0 ||
	    button > 2)
		return 0;
	if (kind == CTUI_TERMINAL_DRAG && modes->mouse < 1002)
		return 0;
	int cb = button;
	bool release = kind == CTUI_TERMINAL_RELEASE;
	if (kind == CTUI_TERMINAL_WHEEL_UP)
		cb = 64;
	else if (kind == CTUI_TERMINAL_WHEEL_DOWN)
		cb = 65;
	else if (kind == CTUI_TERMINAL_DRAG)
		cb += 32;
	if (modes->mouse_sgr)
		return (size_t)snprintf(out, 32, "\033[<%d;%d;%d%c", cb, x + 1,
					y + 1, release ? 'm' : 'M');
	if (x + 33 > 255 || y + 33 > 255)
		return 0;
	if (release)
		cb = 3;
	out[0] = '\033';
	out[1] = '[';
	out[2] = 'M';
	out[3] = (char)(cb + 32);
	out[4] = (char)(x + 33);
	out[5] = (char)(y + 33);
	return 6;
}

/* ---- colours ---- */

static int nearest_level(int v)
{
	static const int L[6] = {0, 95, 135, 175, 215, 255};
	int best = 0;
	for (int i = 1; i < 6; i++)
		if (abs(v - L[i]) < abs(v - L[best]))
			best = i;
	return best;
}

int ctui_terminal_quantize_rgb(int r, int g, int b)
{
	static const int L[6] = {0, 95, 135, 175, 215, 255};
	r = r < 0 ? 0 : r > 255 ? 255 : r;
	g = g < 0 ? 0 : g > 255 ? 255 : g;
	b = b < 0 ? 0 : b > 255 ? 255 : b;
	int ri = nearest_level(r), gi = nearest_level(g), bi = nearest_level(b);
	int cube = 16 + 36 * ri + 6 * gi + bi;
	int dc = (r - L[ri]) * (r - L[ri]) + (g - L[gi]) * (g - L[gi]) +
		 (b - L[bi]) * (b - L[bi]);
	int avg = (r + g + b) / 3;
	int step = (avg - 8 + 5) / 10;
	step = step < 0 ? 0 : step > 23 ? 23 : step;
	int level = 8 + 10 * step;
	int dg = (r - level) * (r - level) + (g - level) * (g - level) +
		 (b - level) * (b - level);
	return dg < dc ? 232 + step : cube;
}

/* A palette index for the terminal's colour count, -1 = default. */
static int color_index(uint32_t c)
{
	int idx;
	switch (CTUI_VT_COLOR_KIND(c)) {
	case 1:
		idx = (int)(c & 0xFFU);
		break;
	case 2:
		idx = ctui_terminal_quantize_rgb((int)(c >> 16U & 0xFFU),
						 (int)(c >> 8U & 0xFFU),
						 (int)(c & 0xFFU));
		break;
	default:
		return -1;
	}
	if (COLORS >= 256)
		return idx;
	if (idx < 16)
		return COLORS >= 16 ? idx : (int)((unsigned)idx & 7U);
	return ctui_theme_resolve(idx, COLORS);
}

static struct {
	int fg, bg;
} g_pool[POOL_MAX];
static int g_pool_count;
static const void *g_pool_term;

/* The attribute of a colour pair from the widget's own pool; 0 (no colour)
 * when colours are off or the pool is used up. */
static int pair_attr(int fg, int bg)
{
	if (!ctui_theme_has_colors())
		return 0;
	if (fg < 0 && bg < 0)
		return ctui_attr(CTUI_PAIR_BODY, false);
	if (g_pool_term != (const void *)stdscr) {
		g_pool_term = stdscr;
		g_pool_count = 0;
	}
	int first = COLOR_PAIRS >= 256 ? POOL_FIRST : COLOR_PAIRS / 2;
	int size = (COLOR_PAIRS >= 256 ? 256 : COLOR_PAIRS) - first;
	if (size > POOL_MAX)
		size = POOL_MAX;
	for (int i = 0; i < g_pool_count; i++)
		if (g_pool[i].fg == fg && g_pool[i].bg == bg)
			return (int)COLOR_PAIR(first + i);
	if (g_pool_count >= size || first < 1)
		return 0;
	g_pool[g_pool_count].fg = fg;
	g_pool[g_pool_count].bg = bg;
	init_pair((short)(first + g_pool_count), (short)fg, (short)bg);
	return (int)COLOR_PAIR(first + g_pool_count++);
}

static int cell_attr(const ctui_vt_cell *c, bool flip)
{
	int attr = pair_attr(color_index(c->fg), color_index(c->bg));
	if (c->attrs & CTUI_VT_BOLD)
		attr = (int)((unsigned)attr | (unsigned)A_BOLD);
	if (c->attrs & CTUI_VT_DIM)
		attr = (int)((unsigned)attr | (unsigned)A_DIM);
#ifdef A_ITALIC
	if (c->attrs & CTUI_VT_ITALIC)
		attr = (int)((unsigned)attr | (unsigned)A_ITALIC);
#endif
	if (c->attrs & CTUI_VT_UNDERLINE)
		attr = (int)((unsigned)attr | (unsigned)A_UNDERLINE);
	if (((c->attrs & CTUI_VT_REVERSE) != 0) != flip)
		attr = (int)((unsigned)attr | (unsigned)A_REVERSE);
	return attr;
}

/* ---- lifecycle ---- */

void ctui_terminal_init(ctui_terminal *t)
{
	*t = (ctui_terminal){.fd = -1, .button = -1};
}

static void sleep_ms(long ms)
{
	struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
	nanosleep(&ts, NULL);
}

static bool reap(ctui_terminal *t, bool block)
{
	if (t->pid <= 0 || t->reaped)
		return t->reaped;
	int st;
	pid_t r;
	do {
		r = waitpid(t->pid, &st, block ? 0 : WNOHANG);
	} while (r < 0 && errno == EINTR);
	if (r == t->pid) {
		t->reaped = true;
		t->wait_status = st;
	} else if (r < 0) { /* somebody else reaped it */
		t->reaped = true;
		t->wait_status = 0;
	}
	return t->reaped;
}

void ctui_terminal_free(ctui_terminal *t)
{
	if (t->pid > 0 && !t->reaped) {
		kill(-t->pid, SIGHUP);
		kill(t->pid, SIGHUP);
		kill(-t->pid, SIGCONT);
		if (t->fd >= 0) {
			close(t->fd);
			t->fd = -1;
		}
		for (int i = 0; i < 60 && !reap(t, false); i++)
			sleep_ms(5);
		if (!t->reaped) {
			kill(-t->pid, SIGKILL);
			kill(t->pid, SIGKILL);
			reap(t, true);
		}
	}
	if (t->fd >= 0)
		close(t->fd);
	ctui_vt_free(t->vt);
	ctui_terminal_init(t);
}

static char **build_env(const char *const envp[])
{
	char *const *base = envp ? (char *const *)envp : environ;
	size_t n = 0;
	while (base && base[n])
		n++;
	char **env = (char **)malloc((n + 2) * sizeof *env);
	if (!env)
		return NULL;
	size_t k = 0;
	for (size_t i = 0; i < n; i++)
		if (strncmp(base[i], "TERM=", 5) != 0)
			env[k++] = base[i];
	env[k++] = "TERM=xterm-256color";
	env[k] = NULL;
	return env;
}

bool ctui_terminal_start(ctui_terminal *t, const char *const argv[],
			 const char *const envp[], int rows, int cols,
			 size_t scrollback)
{
	ctui_terminal_free(t);
	ctui_vt *vt = ctui_vt_new(rows, cols, scrollback);
	if (!vt)
		return false;
	const char *shell[2] = {NULL, NULL};
	if (!argv || !argv[0]) {
		const char *s = getenv("SHELL");
		shell[0] = s && *s ? s : "/bin/sh";
		argv = shell;
	}
	char **env = build_env(envp);
	if (!env) {
		ctui_vt_free(vt);
		return false;
	}
	struct winsize ws = {.ws_row = (unsigned short)rows,
			     .ws_col = (unsigned short)cols};
	int fd;
	pid_t pid = forkpty(&fd, NULL, NULL, &ws);
	if (pid < 0) {
		free((void *)env);
		ctui_vt_free(vt);
		return false;
	}
	if (pid == 0) {
		signal(SIGPIPE, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		sigset_t none;
		sigemptyset(&none);
		sigprocmask(SIG_SETMASK, &none, NULL);
#if defined(__GLIBC__) &&                                                      \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
		close_range(3, ~0U, 0);
#endif
		// NOLINTNEXTLINE(clang-analyzer-optin.taint.GenericTaint)
		execvpe(argv[0], (char *const *)argv, env);
		const char *msg = "cannot run the program\r\n";
		if (write(2, msg, strlen(msg)) < 0)
			_exit(127);
		_exit(127);
	}
	free((void *)env);
	fcntl(fd, F_SETFL,
	      (int)((unsigned)fcntl(fd, F_GETFL) | (unsigned)O_NONBLOCK));
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	ctui_terminal_init(t);
	t->vt = vt;
	t->fd = fd;
	t->pid = pid;
	t->rows = rows;
	t->cols = cols;
	t->scrollback = scrollback;
	return true;
}

bool ctui_terminal_running(const ctui_terminal *t)
{
	return t->vt && t->pid > 0 && !t->exited;
}

bool ctui_terminal_exited(const ctui_terminal *t, int *code, int *sig)
{
	if (!t->exited)
		return false;
	if (code)
		*code = WIFEXITED(t->wait_status) ? WEXITSTATUS(t->wait_status)
						  : -1;
	if (sig)
		*sig =
		    WIFSIGNALED(t->wait_status) ? WTERMSIG(t->wait_status) : 0;
	return true;
}

/* ---- I/O ---- */

static bool put_raw(ctui_terminal *t, const char *p, size_t len)
{
	while (len > 0 && t->fd >= 0) {
		ssize_t n = write(t->fd, p, len);
		if (n > 0) {
			p += n;
			len -= (size_t)n;
		} else if (n < 0 && errno == EINTR) {
			continue;
		} else {
			return false;
		}
	}
	return len == 0;
}

static void flush_responses(ctui_terminal *t)
{
	char buf[CTUI_VT_RESPONSE_MAX];
	size_t n;
	while (t->fd >= 0 &&
	       (n = ctui_vt_take_response(t->vt, buf, sizeof buf)) > 0)
		put_raw(t, buf,
			n); /* best effort: a full pty drops the answer */
}

bool ctui_terminal_pump(ctui_terminal *t, size_t max_bytes)
{
	if (!t->vt)
		return false;
	if (max_bytes == 0)
		max_bytes = CTUI_TERMINAL_FRAME_BYTES;
	bool changed = false, drained = t->fd < 0;
	size_t total = 0, pushed = ctui_vt_history_pushed(t->vt);
	char buf[4096];
	while (t->fd >= 0 && !t->eof) {
		if (total >= max_bytes)
			break;
		size_t want = max_bytes - total < sizeof buf ? max_bytes - total
							     : sizeof buf;
		ssize_t n = read(t->fd, buf, want);
		if (n > 0) {
			ctui_vt_feed(t->vt, buf, (size_t)n);
			total += (size_t)n;
			changed = true;
		} else if (n < 0 && errno == EINTR) {
			continue;
		} else if (n < 0 && errno == EAGAIN) {
			drained = true;
			break;
		} else {
			t->eof = true; /* 0 or EIO: the other side is closed */
		}
	}
	if (t->eof)
		drained = true;
	flush_responses(t);
	size_t grown = ctui_vt_history_pushed(t->vt) - pushed;
	if (t->view > 0 &&
	    grown > 0) /* keep the lines in view where they are */
		t->view += grown;
	if (!t->reaped && reap(t, false))
		changed = true;
	if (t->reaped && drained && !t->exited) {
		t->exited = true;
		if (t->fd >= 0) {
			close(t->fd);
			t->fd = -1;
		}
		changed = true;
	}
	return changed;
}

bool ctui_terminal_wait(ctui_terminal *t, int ms)
{
	if (!t->vt || t->fd < 0)
		return false;
	struct pollfd p = {.fd = t->fd, .events = POLLIN};
	return poll(&p, 1, ms) > 0 &&
	       (((unsigned)p.revents &
		 ((unsigned)POLLIN | (unsigned)POLLHUP)) != 0);
}

bool ctui_terminal_write(ctui_terminal *t, const void *data, size_t len)
{
	const char *p = data;
	for (int attempt = 0; len > 0 && attempt < 100 && t->fd >= 0;
	     attempt++) {
		ssize_t n = write(t->fd, p, len);
		if (n > 0) {
			p += n;
			len -= (size_t)n;
		} else if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
			ctui_terminal_pump(
			    t, 0); /* let the program's output out of the way */
			struct pollfd pf = {.fd = t->fd, .events = POLLOUT};
			poll(&pf, 1, 10);
		} else {
			return false;
		}
	}
	return len == 0;
}

bool ctui_terminal_paste(ctui_terminal *t, const char *text, size_t len)
{
	if (!t->vt || t->fd < 0)
		return false;
	bool marked = ctui_vt_get_modes(t->vt)->bracketed_paste;
	t->view = 0;
	if (!marked)
		return ctui_terminal_write(t, text, len);
	bool ok = ctui_terminal_write(t, "\033[200~", 6);
	size_t from = 0;
	for (size_t i = 0; ok && i <= len; i++) {
		if (i == len || text[i] == '\033') {
			ok = ctui_terminal_write(t, text + from, i - from);
			from = i + 1;
		}
	}
	return ok && ctui_terminal_write(t, "\033[201~", 6);
}

bool ctui_terminal_key(ctui_terminal *t, int key, bool alt)
{
	if (!t->vt)
		return false;
	if (key == KEY_SPREVIOUS || key == KEY_SNEXT) {
		long page = t->rows > 1 ? t->rows - 1 : 1;
		ctui_terminal_scroll(t, key == KEY_SPREVIOUS ? page : -page);
		return true;
	}
	if (t->fd < 0)
		return false;
	char out[16];
	size_t n = ctui_terminal_encode_key(
	    key, alt, ctui_vt_get_modes(t->vt)->app_cursor, out);
	if (n == 0)
		return false;
	t->view = 0;
	ctui_terminal_write(t, out, n);
	return true;
}

/* ---- view ---- */

static size_t history_lines(const ctui_terminal *t)
{
	return t->vt && !ctui_vt_get_modes(t->vt)->alt_screen
		   ? ctui_vt_history_count(t->vt)
		   : 0;
}

void ctui_terminal_scroll(ctui_terminal *t, long lines)
{
	if (!t->vt)
		return;
	long view = (long)t->view + lines;
	long max = (long)history_lines(t);
	t->view = (size_t)(view < 0 ? 0 : view > max ? max : view);
}

bool ctui_terminal_resize(ctui_terminal *t, int rows, int cols)
{
	if (!t->vt)
		return false;
	if (rows == t->rows && cols == t->cols)
		return true;
	if (!ctui_vt_resize(t->vt, rows, cols))
		return false;
	t->rows = rows;
	t->cols = cols;
	if (t->fd >= 0) {
		struct winsize ws = {.ws_row = (unsigned short)rows,
				     .ws_col = (unsigned short)cols};
		ioctl(t->fd, TIOCSWINSZ, &ws); /* the kernel signals SIGWINCH */
	}
	if (t->view > history_lines(t))
		t->view = history_lines(t);
	return true;
}

static void apply_bar(ctui_terminal *t)
{
	size_t hist = history_lines(t);
	t->view = hist > t->bar.offset ? hist - t->bar.offset : 0;
}

bool ctui_terminal_mouse(ctui_terminal *t, ctui_terminal_mouse_kind kind,
			 int button, int y, int x)
{
	if (!t->vt)
		return false;
	if (kind == CTUI_TERMINAL_PRESS && button == 0 &&
	    ctui_scrollbar_press(&t->bar, y + t->oy, x + t->ox)) {
		t->bar_drag = true;
		apply_bar(t);
		return true;
	}
	if (t->bar_drag) {
		if (kind == CTUI_TERMINAL_DRAG) {
			if (ctui_scrollbar_drag(&t->bar, y + t->oy))
				apply_bar(t);
		} else if (kind == CTUI_TERMINAL_RELEASE) {
			ctui_scrollbar_release(&t->bar);
			t->bar_drag = false;
		}
		return true;
	}
	const ctui_vt_modes *modes = ctui_vt_get_modes(t->vt);
	if (modes->mouse != 0) {
		if (t->fd < 0 || y < 0 || y >= t->rows || x < 0 || x >= t->cols)
			return false;
		char out[32];
		size_t n =
		    ctui_terminal_encode_mouse(modes, kind, button, y, x, out);
		if (n == 0)
			return false;
		t->view = 0;
		ctui_terminal_write(t, out, n);
		return true;
	}
	if (kind == CTUI_TERMINAL_WHEEL_UP ||
	    kind == CTUI_TERMINAL_WHEEL_DOWN) {
		ctui_terminal_scroll(t, kind == CTUI_TERMINAL_WHEEL_UP
					    ? WHEEL_LINES
					    : -WHEEL_LINES);
		return true;
	}
	return false;
}

/* ---- drawing ---- */

static size_t put_utf8(uint32_t cp, char *out)
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xC0U | cp >> 6U);
		out[1] = (char)(0x80U | (cp & 0x3FU));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xE0U | cp >> 12U);
		out[1] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
		out[2] = (char)(0x80U | (cp & 0x3FU));
		return 3;
	}
	out[0] = (char)(0xF0U | cp >> 18U);
	out[1] = (char)(0x80U | ((cp >> 12U) & 0x3FU));
	out[2] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
	out[3] = (char)(0x80U | (cp & 0x3FU));
	return 4;
}

static const ctui_vt_cell *source_cell(const ctui_terminal *t, int r, int c)
{
	long line = (long)r - (long)t->view;
	if (line >= 0)
		return ctui_vt_cell_at(t->vt, (int)line, c);
	return ctui_vt_history_cell(t->vt, (size_t)(-line - 1), c);
}

void ctui_terminal_draw(ctui_terminal *t, int y, int x, int width, int height,
			bool focused)
{
	int cols = width - 1, rows = height;
	if (cols < 1 || rows < 1)
		return;
	if (!t->vt) {
		for (int r = 0; r < height; r++)
			ctui_fill(y + r, x, width,
				  ctui_attr(CTUI_PAIR_BODY, false));
		return;
	}
	if (cols > CTUI_VT_MAX_COLS)
		cols = CTUI_VT_MAX_COLS;
	if (rows > CTUI_VT_MAX_ROWS)
		rows = CTUI_VT_MAX_ROWS;
	ctui_terminal_resize(t, rows, cols);
	t->oy = y;
	t->ox = x;
	size_t hist = history_lines(t);
	if (t->view > hist)
		t->view = hist;
	for (int r = 0; r < rows; r++) {
		char run[256];
		size_t len = 0;
		int run_attr = 0, run_col = 0;
		ctui_fill(y + r, x, width, ctui_attr(CTUI_PAIR_BODY, false));
		for (int c = 0; c <= t->cols; c++) {
			const ctui_vt_cell *cell =
			    c < t->cols ? source_cell(t, r, c) : NULL;
			if (cell && cell->width == 0)
				continue;
			int attr = cell ? cell_attr(cell, false) : 0;
			bool wide = cell && cell->width == 2;
			if (len > 0 && (!cell || attr != run_attr || wide ||
					len > sizeof run - 12)) {
				run[len] = '\0';
				ctui_put(y + r, x + run_col, t->cols - run_col,
					 run, run_attr);
				len = 0;
			}
			if (!cell)
				break;
			if (len == 0) {
				run_col = c;
				run_attr = attr;
			}
			uint32_t cp =
			    (cell->attrs & CTUI_VT_HIDDEN) || cell->cp == 0
				? ' '
				: cell->cp;
			len += put_utf8(cp, run + len);
			if (cell->comb && !(cell->attrs & CTUI_VT_HIDDEN))
				len += put_utf8(cell->comb, run + len);
			if (wide) {
				run[len] = '\0';
				ctui_put(y + r, x + run_col, t->cols - run_col,
					 run, run_attr);
				len = 0;
			}
		}
	}
	if (focused && t->view == 0 && t->fd >= 0 &&
	    ctui_vt_get_modes(t->vt)->cursor_visible) {
		int cr = ctui_vt_cursor_row(t->vt),
		    cc = ctui_vt_cursor_col(t->vt);
		const ctui_vt_cell *cell = ctui_vt_cell_at(t->vt, cr, cc);
		if (cell && cell->width == 0 && cc > 0) {
			cc--;
			cell = ctui_vt_cell_at(t->vt, cr, cc);
		}
		if (cell) {
			char glyph[16];
			size_t n = put_utf8(cell->cp ? cell->cp : ' ', glyph);
			if (cell->comb)
				n += put_utf8(cell->comb, glyph + n);
			glyph[n] = '\0';
			ctui_put(y + cr, x + cc, 2, glyph,
				 cell_attr(cell, true));
		}
	}
	ctui_scrollbar_set(&t->bar, hist + (size_t)t->rows, (size_t)t->rows,
			   hist - t->view);
	ctui_scrollbar_place(&t->bar, y, x + t->cols, t->rows);
	ctui_scrollbar_draw(&t->bar, false);
}
