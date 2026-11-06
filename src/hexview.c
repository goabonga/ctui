/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include <ctui/hexview.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctui/text.h>

#define KEY_CTRL_A 1
#define KEY_ESC 27

void ctui_hexview_init(ctui_hexview *v, const void *data, size_t size)
{
	*v = (ctui_hexview){0};
	v->data = data;
	v->size = data ? size : 0;
	v->row_bytes = 16;
}

bool ctui_hexview_load(ctui_hexview *v, const char *path, size_t limit)
{
	ctui_hexview_init(v, NULL, 0);
	if (limit == 0)
		limit = CTUI_HEXVIEW_MAX;
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	size_t cap = 4096, size = 0;
	unsigned char *buf = malloc(cap);
	int err = buf ? 0 : ENOMEM;
	while (!err) {
		size_t got = fread(buf + size, 1, cap - size, f);
		size += got;
		if (size < cap) {
			if (ferror(f))
				err = errno ? errno : EIO;
			break;
		}
		if (size > limit) {
			err = EFBIG;
			break;
		}
		unsigned char *grown = realloc(buf, cap * 2);
		if (!grown) {
			err = ENOMEM;
			break;
		}
		buf = grown;
		cap *= 2;
	}
	if (!err && size > limit)
		err = EFBIG;
	fclose(f);
	if (err) {
		free(buf);
		errno = err;
		return false;
	}
	ctui_hexview_init(v, buf, size);
	v->owned = buf;
	return true;
}

bool ctui_hexview_map(ctui_hexview *v, const char *path)
{
	ctui_hexview_init(v, NULL, 0);
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	struct stat st;
	if (fstat(fd, &st) < 0) {
		int err = errno;
		close(fd);
		errno = err;
		return false;
	}
	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return ctui_hexview_load(v, path, 0);
	}
	if (st.st_size == 0) {
		close(fd);
		return true;
	}
	if ((unsigned long long)st.st_size > (size_t)-1) {
		close(fd);
		errno = EFBIG;
		return false;
	}
	size_t len = (size_t)st.st_size;
	void *m = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	int err = errno;
	close(fd);
	if (m == MAP_FAILED) {
		errno = err;
		return false;
	}
	ctui_hexview_init(v, m, len);
	v->mapping = m;
	v->mapping_len = len;
	return true;
}

/* Index of the first edit at or after offset (nedits when none). */
static size_t lower_bound(const ctui_hexview *v, size_t offset)
{
	size_t lo = 0, hi = v->nedits;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		if (v->edits[mid].offset < offset)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

static const ctui_hexedit *find_edit(const ctui_hexview *v, size_t offset)
{
	size_t i = lower_bound(v, offset);
	return i < v->nedits && v->edits[i].offset == offset ? &v->edits[i]
							     : NULL;
}

unsigned char ctui_hexview_original(const ctui_hexview *v, size_t offset)
{
	return offset < v->size ? v->data[offset] : 0;
}

unsigned char ctui_hexview_byte(const ctui_hexview *v, size_t offset)
{
	const ctui_hexedit *e = find_edit(v, offset);
	return e ? e->value : ctui_hexview_original(v, offset);
}

bool ctui_hexview_edited(const ctui_hexview *v, size_t offset)
{
	return find_edit(v, offset) != NULL;
}

size_t ctui_hexview_modified(const ctui_hexview *v)
{
	return v->nedits;
}

bool ctui_hexview_set(ctui_hexview *v, size_t offset, unsigned char value)
{
	if (offset >= v->size)
		return false;
	size_t i = lower_bound(v, offset);
	bool have = i < v->nedits && v->edits[i].offset == offset;
	if (value == v->data[offset]) {
		if (have) {
			memmove(&v->edits[i], &v->edits[i + 1],
				(v->nedits - i - 1) * sizeof *v->edits);
			v->nedits--;
		}
		return true;
	}
	if (have) {
		v->edits[i].value = value;
		return true;
	}
	if (v->nedits == v->cap_edits) {
		size_t cap = v->cap_edits ? v->cap_edits * 2 : 64;
		ctui_hexedit *grown = realloc(v->edits, cap * sizeof *grown);
		if (!grown)
			return false;
		v->edits = grown;
		v->cap_edits = cap;
	}
	memmove(&v->edits[i + 1], &v->edits[i],
		(v->nedits - i) * sizeof *v->edits);
	v->edits[i] = (ctui_hexedit){offset, value};
	v->nedits++;
	return true;
}

void ctui_hexview_revert(ctui_hexview *v)
{
	v->nedits = 0;
	v->pending = false;
}

bool ctui_hexview_parse(const char *text, unsigned char *out, size_t cap,
			size_t *len)
{
	const char *a = text, *b = text + strlen(text);
	while (a < b && isspace((unsigned char)*a))
		a++;
	while (b > a && isspace((unsigned char)b[-1]))
		b--;
	if (a == b)
		return false;
	size_t n = 0;
	if (b - a >= 2 && *a == '"' && b[-1] == '"') {
		if (b - a == 2 || (size_t)(b - a - 2) > cap)
			return false;
		memcpy(out, a + 1, (size_t)(b - a - 2));
		*len = (size_t)(b - a - 2);
		return true;
	}
	size_t digits = 0;
	bool hex = true;
	for (const char *c = a; c < b && hex; c++) {
		if (isspace((unsigned char)*c))
			continue;
		hex = isxdigit((unsigned char)*c) != 0;
		digits++;
	}
	if (hex && digits % 2 == 0) {
		if (digits / 2 > cap)
			return false;
		int high = -1;
		for (const char *c = a; c < b; c++) {
			if (isspace((unsigned char)*c))
				continue;
			int d = *c <= '9'
				    ? *c - '0'
				    : (int)((unsigned)*c | 0x20U) - 'a' + 10;
			if (high < 0) {
				high = d;
			} else {
				out[n++] =
				    (unsigned char)((unsigned)high << 4U |
						    (unsigned)d);
				high = -1;
			}
		}
		*len = n;
		return true;
	}
	n = strlen(text);
	if (n > cap)
		return false;
	memcpy(out, text, n);
	*len = n;
	return true;
}

static bool match_at(const ctui_hexview *v, const unsigned char *pattern,
		     size_t len, size_t at)
{
	if (v->nedits == 0)
		return memcmp(v->data + at, pattern, len) == 0;
	for (size_t k = 0; k < len; k++) {
		if (ctui_hexview_byte(v, at + k) != pattern[k])
			return false;
	}
	return true;
}

ctui_hexfind ctui_hexview_find(const ctui_hexview *v,
			       const unsigned char *pattern, size_t len,
			       size_t *pos, bool forward, size_t budget)
{
	if (len == 0 || len > v->size)
		return CTUI_HEXFIND_NOTFOUND;
	size_t last = v->size - len; /* the last offset a match can start at */
	size_t at = *pos;
	if (forward && at > last)
		return CTUI_HEXFIND_NOTFOUND;
	if (!forward && at > last)
		at = last;
	for (size_t done = 0;; done++) {
		if (budget && done == budget) {
			*pos = at;
			return CTUI_HEXFIND_MORE;
		}
		if (match_at(v, pattern, len, at)) {
			*pos = at;
			return CTUI_HEXFIND_FOUND;
		}
		if (forward ? at == last : at == 0)
			return CTUI_HEXFIND_NOTFOUND;
		at += forward ? 1 : (size_t)-1;
	}
}

bool ctui_hexview_find_next(ctui_hexview *v, const unsigned char *pattern,
			    size_t len, bool forward)
{
	if (len == 0 || len > v->size)
		return false;
	size_t pos = 0;
	bool start = forward ? v->cursor + 1 < v->size : v->cursor > 0;
	ctui_hexfind r = CTUI_HEXFIND_NOTFOUND;
	if (start) {
		pos = forward ? v->cursor + 1 : v->cursor - 1;
		r = ctui_hexview_find(v, pattern, len, &pos, forward, 0);
	}
	if (r != CTUI_HEXFIND_FOUND) { /* wrap once */
		pos = forward ? 0 : v->size - 1;
		r = ctui_hexview_find(v, pattern, len, &pos, forward, 0);
	}
	if (r != CTUI_HEXFIND_FOUND)
		return false;
	v->selecting = true;
	v->anchor = pos + len - 1;
	v->cursor = pos;
	v->pending = false;
	return true;
}

bool ctui_hexview_goto(ctui_hexview *v, size_t offset)
{
	if (offset >= v->size)
		return false;
	v->cursor = offset;
	v->selecting = false;
	v->pending = false;
	return true;
}

#define SAVE_CHUNK 65536

static bool write_all(int fd, const unsigned char *p, size_t n)
{
	while (n) {
		ssize_t w = write(fd, p, n);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		p += w;
		n -= (size_t)w;
	}
	return true;
}

bool ctui_hexview_save(const ctui_hexview *v, const char *path)
{
	char tmp[PATH_MAX + 16];
	if (snprintf(tmp, sizeof tmp, "%s.ctui-XXXXXX", path) >=
	    (int)sizeof tmp) {
		errno = ENAMETOOLONG;
		return false;
	}
	int fd = mkstemp(tmp);
	if (fd < 0)
		return false;
	struct stat st;
	mode_t mode = stat(path, &st) == 0 ? st.st_mode & 07777U : 0644;
	unsigned char *buf = malloc(SAVE_CHUNK);
	bool ok = buf && fchmod(fd, mode) == 0;
	if (!buf)
		errno = ENOMEM;
	for (size_t at = 0; ok && at < v->size; at += SAVE_CHUNK) {
		size_t n =
		    v->size - at < SAVE_CHUNK ? v->size - at : SAVE_CHUNK;
		memcpy(buf, v->data + at, n);
		for (size_t i = lower_bound(v, at);
		     i < v->nedits && v->edits[i].offset < at + n; i++)
			buf[v->edits[i].offset - at] = v->edits[i].value;
		ok = write_all(fd, buf, n);
	}
	int err = errno;
	free(buf);
	if (ok && fsync(fd) != 0) {
		ok = false;
		err = errno;
	}
	if (close(fd) != 0 && ok) {
		ok = false;
		err = errno;
	}
	if (ok && rename(tmp, path) != 0) {
		ok = false;
		err = errno;
	}
	if (!ok) {
		unlink(tmp);
		errno = err;
	}
	return ok;
}

void ctui_hexview_free(ctui_hexview *v)
{
	if (v->mapping)
		munmap(v->mapping, v->mapping_len);
	free(v->owned);
	free(v->edits);
	ctui_hexview_init(v, NULL, 0);
}

bool ctui_hexview_layout(const ctui_hexview *v, int width, ctui_hexlayout *out)
{
	static const int sizes[] = {16, 8, 4};
	int digits = v->size > 0xFFFFFFFFu ? 16 : 8;
	for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
		int n = sizes[i];
		int hex_x = digits + 2;
		int text_x = hex_x + n * 3 + (n - 1) / 8 + 1;
		if (text_x + n <= width) {
			*out = (ctui_hexlayout){n, digits, hex_x, text_x,
						text_x + n};
			return true;
		}
	}
	return false;
}

size_t ctui_hexview_rows(const ctui_hexview *v)
{
	size_t rb = v->row_bytes ? v->row_bytes : 16;
	return v->size ? (v->size + rb - 1) / rb : 1;
}

bool ctui_hexview_range(const ctui_hexview *v, size_t *first, size_t *last)
{
	if (!v->selecting || v->size == 0)
		return false;
	size_t a = v->anchor < v->size ? v->anchor : v->size - 1;
	size_t c = v->cursor < v->size ? v->cursor : v->size - 1;
	*first = a < c ? a : c;
	*last = a < c ? c : a;
	return true;
}

bool ctui_hexview_selected(const ctui_hexview *v, size_t offset)
{
	size_t a, b;
	return ctui_hexview_range(v, &a, &b) && offset >= a && offset <= b;
}

/* Cursor moved by n bytes, up or down, staying on the data. */
static size_t step(const ctui_hexview *v, bool down, size_t n)
{
	size_t rb = v->row_bytes ? v->row_bytes : 16, c = v->cursor;
	if (down) {
		if (c + n < v->size)
			return c + n;
		/* A short last row is reached by the column it ends on. */
		return n > 1 && c / rb < (v->size - 1) / rb ? v->size - 1 : c;
	}
	if (c >= n)
		return c - n;
	return n > 1 ? c % rb : c;
}

static bool is_backspace(int key)
{
	return key == KEY_BACKSPACE || key == 127 || key == 8;
}

static void advance(ctui_hexview *v)
{
	if (v->cursor + 1 < v->size)
		v->cursor++;
}

/* A key typed while editing; false when it is not an editing key. */
static bool edit_key(ctui_hexview *v, int key)
{
	if (key == '\t') {
		v->pane = v->pane == CTUI_HEXPANE_HEX ? CTUI_HEXPANE_TEXT
						      : CTUI_HEXPANE_HEX;
		v->pending = false;
		return true;
	}
	if (is_backspace(key)) {
		if (v->pending) {
			ctui_hexview_set(v, v->cursor,
					 ctui_hexview_original(v, v->cursor));
			v->pending = false;
		} else if (v->cursor > 0) {
			v->cursor--;
			ctui_hexview_set(v, v->cursor,
					 ctui_hexview_original(v, v->cursor));
		}
		v->selecting = false;
		return true;
	}
	if (v->pane == CTUI_HEXPANE_HEX) {
		if (key < 0 || key > 127 || !isxdigit(key))
			return false;
		unsigned d =
		    (unsigned)(key <= '9'
				   ? key - '0'
				   : (int)((unsigned)key | 0x20U) - 'a' + 10);
		unsigned char cur = ctui_hexview_byte(v, v->cursor);
		v->selecting = false;
		if (!v->pending) {
			if (ctui_hexview_set(
				v, v->cursor,
				(unsigned char)(d << 4U |
						((unsigned)cur & 0x0FU))))
				v->pending = true;
		} else {
			ctui_hexview_set(
			    v, v->cursor,
			    (unsigned char)(((unsigned)cur & 0xF0U) | d));
			v->pending = false;
			advance(v);
		}
		return true;
	}
	if (key < 0x20 || key >= 0x7F)
		return false;
	v->selecting = false;
	ctui_hexview_set(v, v->cursor, (unsigned char)key);
	advance(v);
	return true;
}

bool ctui_hexview_key(ctui_hexview *v, int key, size_t rows)
{
	if (v->size == 0)
		return false;
	if (v->editing && edit_key(v, key))
		return true;
	v->pending = false;
	size_t rb = v->row_bytes ? v->row_bytes : 16;
	if (rows == 0)
		rows = 1;
	bool extend = false;
	size_t to;
	switch (key) {
	case KEY_SLEFT:
		extend = true; /* fall through */
	case KEY_LEFT:
		to = step(v, false, 1);
		break;
	case KEY_SRIGHT:
		extend = true; /* fall through */
	case KEY_RIGHT:
		to = step(v, true, 1);
		break;
	case KEY_SR:
		extend = true; /* fall through */
	case KEY_UP:
		to = step(v, false, rb);
		break;
	case KEY_SF:
		extend = true; /* fall through */
	case KEY_DOWN:
		to = step(v, true, rb);
		break;
	case KEY_PPAGE:
		to = step(v, false, rb * rows);
		break;
	case KEY_NPAGE:
		to = step(v, true, rb * rows);
		break;
	case KEY_HOME:
		to = v->cursor / rb * rb;
		break;
	case KEY_END:
		to = v->cursor / rb * rb + rb - 1;
		if (to >= v->size)
			to = v->size - 1;
		break;
	case KEY_CTRL_A:
		v->selecting = true;
		v->anchor = 0;
		v->cursor = v->size - 1;
		return true;
	case KEY_ESC: {
		bool was = v->selecting;
		v->selecting = false;
		return was;
	}
	default:
		return false;
	}
	bool changed = to != v->cursor || (!extend && v->selecting);
	if (extend && !v->selecting) {
		v->selecting = true;
		v->anchor = v->cursor;
		changed = true;
	} else if (!extend) {
		v->selecting = false;
	}
	v->cursor = to;
	return changed;
}

void ctui_hexview_reveal(ctui_hexview *v, size_t rows)
{
	size_t rb = v->row_bytes ? v->row_bytes : 16;
	if (rows == 0)
		rows = 1;
	size_t row = v->cursor / rb;
	if (row < v->top)
		v->top = row;
	else if (row >= v->top + rows)
		v->top = row - rows + 1;
}

void ctui_hexview_scroll_to(ctui_hexview *v, size_t rows, size_t top)
{
	size_t rb = v->row_bytes ? v->row_bytes : 16;
	if (rows == 0)
		rows = 1;
	size_t total = ctui_hexview_rows(v);
	size_t max = total > rows ? total - rows : 0;
	v->top = top > max ? max : top;
	size_t row = v->cursor / rb;
	if (row < v->top)
		v->cursor = v->top * rb + v->cursor % rb;
	else if (row >= v->top + rows)
		v->cursor = (v->top + rows - 1) * rb + v->cursor % rb;
	if (v->size && v->cursor >= v->size)
		v->cursor = v->size - 1;
}

/* Column of byte i of a row, relative to the first byte. */
static int hex_col(int i)
{
	return i * 3 + i / 8;
}

/* The byte at (y, x). Strict: only on a hex digit or a text character;
 * otherwise the nearest byte, on any row. */
static bool locate(const ctui_hexview *v, int width, int y, int x, bool strict,
		   size_t *offset)
{
	ctui_hexlayout lay;
	if (v->size == 0 || !ctui_hexview_layout(v, width, &lay))
		return false;
	long row = (long)v->top + y;
	if (strict ? (y < 0 || row < 0) : false)
		return false;
	if (row < 0)
		row = 0;
	int i = -1;
	if (x >= lay.text_x) {
		i = x - lay.text_x;
		if (i >= lay.per_row)
			i = strict ? -1 : lay.per_row - 1;
	} else if (x >= lay.hex_x) {
		int rel = x - lay.hex_x;
		for (int k = 0; k < lay.per_row; k++) {
			if (rel >= hex_col(k) &&
			    (rel <= hex_col(k) + 1 || !strict))
				i = k;
		}
	} else if (!strict) {
		i = 0;
	}
	if (i < 0)
		return false;
	size_t at = (size_t)row * (size_t)lay.per_row + (size_t)i;
	if (at >= v->size) {
		if (strict)
			return false;
		at = v->size - 1;
	}
	*offset = at;
	return true;
}

bool ctui_hexview_hit(const ctui_hexview *v, int width, int y, int x,
		      size_t *offset)
{
	return locate(v, width, y, x, true, offset);
}

bool ctui_hexview_press(ctui_hexview *v, int width, int y, int x)
{
	size_t at;
	if (!locate(v, width, y, x, true, &at))
		return false;
	v->cursor = at;
	v->anchor = at;
	v->selecting = false;
	v->pending = false;
	v->dragging = true;
	return true;
}

bool ctui_hexview_drag(ctui_hexview *v, int width, int y, int x)
{
	size_t at;
	if (!v->dragging || !locate(v, width, y, x, false, &at))
		return false;
	if (at == v->cursor)
		return false;
	v->cursor = at;
	v->selecting = true;
	return true;
}

bool ctui_hexview_release(ctui_hexview *v)
{
	bool was = v->dragging;
	v->dragging = false;
	return was;
}

static void hexview_draw_impl(ctui_hexview *v, int y, int x, int width,
			      int height, bool focused)
{
	ctui_hexlayout lay;
	if (height <= 0 || !ctui_hexview_layout(v, width, &lay))
		return;
	v->row_bytes = (size_t)lay.per_row;
	ctui_hexview_reveal(v, (size_t)height);
	int body = ctui_attr(CTUI_PAIR_BODY, false),
	    muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int sel = ctui_attr(CTUI_PAIR_SELECTION, false),
	    edited = ctui_attr(CTUI_PAIR_ERROR, false);
	for (int r = 0; r < height; r++) {
		size_t first = (v->top + (size_t)r) * (size_t)lay.per_row;
		ctui_fill(y + r, x, width, body);
		if (first >= v->size)
			continue;
		char text[40];
		snprintf(text, sizeof text, "%0*zx", lay.digits, first);
		ctui_put(y + r, x, lay.digits, text, muted);
		for (int i = 0; i < lay.per_row && first + (size_t)i < v->size;
		     i++) {
			size_t at = first + (size_t)i;
			unsigned char b = ctui_hexview_byte(v, at);
			bool printable = b >= 0x20 && b < 0x7F;
			bool chosen = ctui_hexview_selected(v, at),
			     changed = ctui_hexview_edited(v, at);
			int attr = chosen ? sel
					  : (changed ? edited
						     : (b == 0 ? muted : body));
			int attr_text =
			    chosen ? sel
				   : (changed ? edited
					      : (printable ? body : muted));
			if (focused && at == v->cursor) {
				/* While editing, the pane that takes the keys
				 * is reversed and the other one only
				 * underlined. */
				bool hex_on =
				    !v->editing || v->pane == CTUI_HEXPANE_HEX;
				bool text_on =
				    !v->editing || v->pane == CTUI_HEXPANE_TEXT;
				attr = (int)((unsigned)attr |
					     (unsigned)(hex_on ? A_REVERSE
							       : A_UNDERLINE));
				attr_text =
				    (int)((unsigned)attr_text |
					  (unsigned)(text_on ? A_REVERSE
							     : A_UNDERLINE));
			}
			char hex[3], ch[2] = {printable ? (char)b : '.', '\0'};
			snprintf(hex, sizeof hex, "%02x", b);
			ctui_put(y + r, x + lay.hex_x + hex_col(i), 2, hex,
				 attr);
			ctui_put(y + r, x + lay.text_x + i, 1, ch, attr_text);
		}
	}
}

void ctui_hexview_draw(ctui_hexview *v, int y, int x, int width, int height,
		       bool focused)
{
	ctui_style_begin(v->style);
	hexview_draw_impl(v, y, x, width, height, focused);
	ctui_style_end();
}
