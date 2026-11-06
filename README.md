# ctui

[![ci](https://github.com/goabonga/ctui/actions/workflows/ci.yml/badge.svg)](https://github.com/goabonga/ctui/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/goabonga/ctui)](https://github.com/goabonga/ctui/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A C library of reusable terminal user interface (TUI) components.

## Build

Requires a C11 compiler, GNU make, pkg-config and ncursesw.

```bash
make
```

This builds the static library `build/libctui.a`. `make install`
installs it under `$(PREFIX)/lib` and the headers `include/ctui/*.h` under
`$(PREFIX)/include/ctui` (default `/usr/local`), honouring `DESTDIR`.

## Configuration

Everything the components draw comes from a global configuration with
built-in defaults; set it once, before `ctui_theme_init()`:

```c
ctui_palette colors = ctui_palette_default();
colors.info = 24;                      /* xterm-256 indexes */
ctui_theme_set_palette(&colors);       /* NULL restores the default */

ctui_glyphs glyphs = *ctui_theme_glyphs_for(true);
glyphs.scroll_thumb = "#";
ctui_theme_set_glyphs(&glyphs, NULL);  /* Unicode set, ASCII set */

ctui_labels words = {.yes = "Oui", .no = "Non"};
ctui_labels_set(&words);               /* unset fields keep the English text */
```

Glyphs fall back to ASCII when the locale is not UTF-8 or when `CTUI_ASCII`
is set; `NO_COLOR` disables colours.

The texts can also come from language files, one per language, read for the
locale of the machine (`LC_ALL`, `LC_MESSAGES` or `LANG`); the built-in
English stays when the locale is `C` or no file exists:

```c
ctui_labels_load_locale("/usr/share/myapp/lang");  /* fr_FR.UTF-8: fr_FR.lang, then fr.lang */
```

A language file holds lines `field = text`, where `field` is a member name
of `ctui_labels`; blank lines, lines starting with `#` and unknown fields
are ignored, and `ctui_labels_load()` reads a single file.

## Components

- **theme**: Solarized powerline palette, colour pairs, tones, glyph sets
  with ASCII and monochrome fallbacks.
- **labels**: the texts the components draw themselves (buttons, titles, key
  hints), replaceable as a set or per component, and loadable from language
  files.
- **text**: locale-independent UTF-8 decoding and column arithmetic,
  sanitising, word wrapping and clipped drawing helpers.
- **gauge**: zoned bars and labelled `CPU [████░░░░] 12.0%` gauges coloured
  by value.
- **frame**: rounded frames with an optional close button, separators and summary cards with labelled
  gauges that adapt to their height.
- **scrollbar**: vertical or horizontal scrollbar with a proportional thumb,
  grabbing and dragging, page jumps on the track and mouse wheel support.
- **tabbar**: powerline tab bar with icons, disabled and closable tabs,
  right-aligned hints and a pure layout shared with mouse hit testing.
- **statusbar**: powerline status bar with left and right runs of toned
  segments and automatic hard or soft dividers.
- **spinner**: activity indicator in three frame sets (braille, line, dots)
  with an ASCII fallback and an optional label; the frame is a pure function
  of the time the caller passes in (`ctui_spinner_index()`), at 10 frames a second
  unless `fps` says otherwise.
- **sparkline**: chart of a series, a block per value on one row, or bars
  rising in eighths of a row on several.
- **panel**: scrollable panel of titled sections of readings with optional
  gauges and notes, flowing into two columns on wide screens.
- **meter**: vertical or horizontal audio level meter on a -60..0 dBFS scale
  with green, yellow and red zones, a peak-hold marker that falls with a
  time step given by the caller, a clip indicator, stereo pair placement and
  dB / linear conversions; the level is set with keys, the mouse or the wheel.
- **dialog**: non-blocking confirm, prompt and text viewer dialogs driven by
  plain state structs, and a helper centring a dialog of any size.
- **list**: selectable lists with details, scrolling and mouse hit testing,
  and popup menus with separators and disabled entries.
- **toast**: one-row notifications (info, ok, warning, error) stacked in a
  corner, with caller-supplied time for expiry, dismissal and mouse hit testing.
- **burger**: burger button opening a dropdown menu that stays on screen,
  scrolls, and answers to the keyboard and the mouse. Entries can open
  nested submenus (linked with a table of `ctui_menu_link`, marked by an arrow
  glyph with an ASCII fallback) on the right or, when short of room, on the
  left; Right/Enter open and Left/Esc close, and clicks and hover follow.
- **tree**: lazily loaded file tree with sorting, keyboard navigation and
  file-type icons (Nerd Font glyphs with an ASCII fallback).
- **tabpanel**: tab strip for panels inside a page or a dialog, with keyboard
  selection, a marked rule and mouse hit testing.
- **wizard**: multi-step flow frame for dialogs: a step indicator, Cancel /
  Back / Next (Finish) buttons, Ctrl-B / Ctrl-N shortcuts and hit testing.
- **layout**: pure layout engine (no drawing): `ctui_layout_split()` cuts an
  `ctui_rect` into child rectangles along a row or a column from constraints
  (length, min, max, percent, ratio, fill with weights), with a margin,
  spacing and a flex mode for the free space (start, center, end, space
  between, space around, stretch). Priority is length, then min / max, then
  percent / ratio, then fill; sizes never exceed the area, never overlap and
  never overflow. `ctui_layout_shrink()` removes insets and `ctui_layout_center()`
  centres a rectangle in another. Nest by splitting a returned rectangle.
- **scrollview**: scrollable viewport over a virtual area of any size, drawn
  by a callback that receives the visible origin; vertical and horizontal
  scrollbars appear when needed, with keys, wheel and drag, and pure layout
  and hit functions (`ctui_scrollview_layout_for()`, `ctui_scrollview_hit()`,
  `ctui_scrollview_cell_at()`).
- **input**: single-line text input on a caller buffer: cursor moves by
  character, Home / End, word jumps (Shift-Left / Shift-Right), Backspace /
  Delete, Ctrl-W / Ctrl-U / Ctrl-K, UTF-8 typed byte by byte, wide
  characters, an optional `*` mask, a `max_chars` limit, a placeholder,
  sideways scrolling inside its width and a click that places the cursor;
  `style` overrides its colours. A rule (see `inputrule`) limits and lays out
  the text and marks it right or wrong. The editing operations work on byte
  offsets without a screen; there is no selection.
- **inputrule**: rules for the text of an input, as plain structs of settings
  and callbacks: the characters that may be typed, a maximum length, layout
  separators, a layout function and a validity check, applied by
  `ctui_input_set_rule()`. Ready-made rules: `ctui_rule_password` (masked),
  `ctui_rule_email`, `ctui_rule_phone` (`+` and 7 to 15 digits) and
  `ctui_rule_card` (digits in groups of four, Luhn checksum); a program adds
  its own the same way. The functions that apply a rule and
  `ctui_luhn_valid()` are pure.
- **numinput**: integer or decimal field with min, max, step and a unit
  suffix: Up / Down, PgUp / PgDn, Home / End, wheel and the `-` / `+`
  buttons change it, and typed digits are validated as they come (sign,
  decimal point, fraction digits, range). Numbers are read and written
  without the C locale, with at most 15 significant digits, so parsing
  cannot overflow (`ctui_numinput_parse()`, `ctui_numinput_format()`);
  `style` overrides its colours.
- **timeinput**: time of day field `HH:MM`, `HH:MM:SS`, `HH:MM:SS.fff` or
  `HH:MM:SS.ffffff` edited segment by segment, the value being the
  microseconds since midnight cut to the precision of the field, with an
  optional minimum and maximum that typing and stepping respect. Digits are
  typed into the segment (those of the fraction fill it from the left), Up /
  Down and the wheel step it (wrapping without carry), PgUp / PgDn step by ten
  and a click selects a segment. `ctui_timeinput_parse()` and
  `ctui_timeinput_format()` are pure; `style` overrides its colours.
- **durationinput**: duration field such as `012h 30m 05s 250ms 017us` edited
  unit by unit, from hours down to microseconds (the largest and the smallest
  unit shown are chosen), the value being a number of microseconds cut to the
  smallest unit. The largest unit takes what the others do not, Up / Down and
  the wheel step a unit and carry into the larger ones, and an optional
  maximum is respected by typing and stepping and sizes the largest unit.
  `ctui_durationinput_parse()` reads `1h 30m 5s 250ms 10us` and
  `ctui_durationinput_format()` writes the field text; both are pure and
  `style` overrides the colours.
- **form**: labelled fields aligned on their labels: text (optionally masked),
  checkbox, select with a dropdown, slider and list, with focus movement,
  scrolling and mouse support; drawn on the page or inside a dialog. Date
  fields (with the month picker of `dateinput`), time fields and duration
  fields (hours down to microseconds, with optional limits) reuse the inputs
  above: PgUp / PgDn and `+` / `-` step the segment under the cursor while Up
  / Down move between the fields.
- **picker**: centred file, folder and file-or-folder chooser dialogs browsing one folder at a
  time, with hidden-file toggle, sizes, icons and mouse support.
- **slider**: standalone horizontal or vertical fader with a filled track, knob,
  label and value, an optional centre mark for bipolar values, keys, and mouse
  press, drag and wheel mapped to the value by pure functions.
- **knob**: rotary knob drawn in five rows (a dial of Unicode or ASCII cells,
  the label and the value): a value on a step grid, turned with the keys
  (fine adjust with Shift, ten steps with PgUp / PgDn), by dragging or with
  the wheel; optional bipolar mode with a centre detent and a default value
  to reset to. The value-to-position mapping is made of pure functions.
- **keyboard**: piano keyboard over a range of octaves with white and black
  keys, held notes shown pressed, a cursor and a selection; the typing keys
  `a w s e d f t g y h u j k o l p` map to MIDI notes, notes get names such as
  `C#4`, and a mouse press, drag or release holds the key under the pointer.
- **pads**: grid of trigger pads (up to 8 x 8, 4 x 4 by default) with labels,
  a velocity level shown in green, yellow or red that decays with a time given
  by the caller, a cursor, the 1234 / qwer / asdf / zxcv key mapping, and
  mouse press, drag and release with the velocity set by where the pad is hit.
- **stepseq**: step sequencer grid in the style of drum machines: tracks of 16
  or 32 steps held in caller-owned words, optional per-step velocity shown as
  the height of the cell, beat groups, playhead column, cursor, keys, and
  mouse toggling and painting with the wheel for velocity; pure helpers
  (`ctui_stepseq_bit_*`, `ctui_stepseq_wrap()`) let a sequencer engine work on the
  same data.
- **textview**: read-only text view with line-number gutter, cursor,
  selection, tab stops, wide characters and scrolling, sideways too with an
  optional horizontal scrollbar under the text (the gutter stays fixed).
- **textarea**: multi-line UTF-8 editor on the same column arithmetic as
  textview: typing, Enter, Tab, character-wise Backspace and Delete, cursor
  moves with a goal column, selection with Shift+arrows, an internal
  clipboard (cut, copy, paste), bounded undo and redo that coalesces typing,
  a modified flag, an optional size limit, a read-only mode, an optional
  line-number gutter, and mouse click and drag; the editing operations work
  on byte offsets without a screen.
- **richtext**: styled text made of spans (a UTF-8 slice with a theme colour,
  bold / dim / italic / underline / reverse and a link id), lines of spans and
  documents of lines, all in caller-owned arrays. `ctui_rt_parse()` reads a small
  markup (`[b]` `[d]` `[i]` `[u]` `[r]`, `[fg=ok]`, `[link=3]`, `[/]` or `[/b]`
  to close, `\[` for a literal bracket); an unknown, unbalanced or too deeply
  nested tag stays literal text and parsing never fails on its input. Rows wrap
  at spaces across spans, keep their styles and the column widths of wide and
  combining characters, and are aligned left, centre, right or justified;
  rows can be cut with the ellipsis, scrolled with a row offset and drawn
  clipped to a rectangle (`ctui_rt_draw()`). `ctui_rt_height()` measures the
  wrapped height for a width and `ctui_rt_hit()` / `ctui_rt_link_at()` tell which
  span is under a cell. The demo's Rich page cycles the alignment with `a`,
  toggles wrapping with `w` and the ellipsis with `e`, scrolls with the arrows,
  Page keys and wheel, and reports clicks on its links.
- **hexview**: hexadecimal viewer for binary data held in memory or read from
  a file: offsets, bytes and printable text in rows of 16, 8 or 4 bytes
  according to the width, with cursor, selection, scrolling and mouse
  support; non-printable bytes are drawn as `.`. `ctui_hexview_map()` maps a
  file read-only with no size limit (reading it with `ctui_hexview_load()` is
  capped); bytes are read through `ctui_hexview_byte()`. A mapped file that is
  truncated by another process makes reads past the new end raise SIGBUS.
  Bytes can be edited in place of the data: edits are an overlay (the source
  buffer and the mapping are never written), typed as hex digits or as text
  in the pane chosen with Tab, drawn with the error colour, and
  `ctui_hexview_save()` writes the result through a temporary file, so it can
  replace the file being viewed.
  `ctui_hexview_parse()` turns typed text (hex digits, "quoted text" or plain
  text) into a pattern; `ctui_hexview_find()` scans for it in budgeted slices
  so a big file keeps the interface responsive, `ctui_hexview_find_next()`
  wraps once and selects the match, and `ctui_hexview_goto()` moves the cursor
  to an offset. Searches see the edited bytes.
- **logview**: log viewer over a bounded ring of records (time in seconds,
  level trace to error, source tag, message) copied into the ring and
  sanitised, with levels coloured by the theme, a per-level visibility
  filter, a case-insensitive text filter with highlighted matches, follow
  mode that sticks to the newest record and stops when the user scrolls up
  (End or `f` resumes), cut or wrapped messages, a record-number or time
  gutter, a scrollbar, keys, wheel, scrollbar drag and selection of a record
  by click, with pure layout and hit functions. Single UI thread only.
- **table**: table of caller-owned rows read through a cell accessor, with
  fixed and flexible columns, alignment, a header whose click asks for a
  sort (`ctui_table_sort_indices()` gives a stable order), selection by keys
  and mouse, a scrollbar and text cut with the ellipsis.
- **waveform**: zoomable waveform view with cursor, selection and playhead
  over interleaved float samples; a range is selected with keys or by
  pressing and dragging the mouse, and `ctui_waveform_reset()` shows the whole
  signal again; `filled` draws solid bars in half rows instead of thin lines,
  in the info tone of the header up to the playhead and in its idle tone
  after it.
- **bigtext**: large text in a 3 x 5 block font (digits, letters, space and
  `: . - /`; lowercase is drawn as uppercase, any other character blank), with
  `ctui_bigtext_width()` to measure, clipping to a column budget and a colour pair.
- **helpview**: key-binding help overlay: caller-owned groups of (keys,
  description) in a scrollable dialog that uses two columns on wide terminals
  and one on narrow ones, with keys and wheel to scroll and close.
- **palette**: command palette overlay (Ctrl-P in editors): a centred dialog
  with an input line over a filtered, ranked list of caller-owned commands
  (name, optional shortcut and description). The fuzzy match is a
  case-insensitive subsequence by UTF-8 character (ASCII case folding only)
  scored with bonuses for word starts and consecutive characters
  (`ctui_palette_score`, documented in `palette.h`); `ctui_palette_filter` returns
  indices with the matched positions, best first and stable for ties, into a
  caller-provided buffer, so thousands of commands and every keystroke
  allocate nothing. Typing, Backspace, Up / Down, PgUp / PgDn, Enter and Esc
  keys, clicks on entries and the wheel; matched letters are underlined.
- **canvas**: dot canvas (2 x 4 Braille dots per cell, one colour per cell)
  with lines (also clipped ones, with fractional ends far off the canvas),
  polylines, rectangles and circles (outlined or filled), and round axis scales
  for the charts.
- **linechart**: multi-series line chart with a labelled y axis, optional x
  labels and axis titles, a legend on top, below, on the right or hidden,
  series drawn as lines, points (scatter) or both, gaps for missing values and
  a point lookup for the mouse.
- **barchart**: vertical or horizontal bars, side by side or stacked, with
  fine eighth-block lengths and bar lookup for the mouse.
- **piechart**: pie and donut charts with a legend of shares and slice lookup
  for the mouse.
- **histogram**: bins caller-owned samples into equal-width bins (a fixed
  number, or the rule of Sturges, `ceil(log2(n)) + 1`, by default) over the
  data range rounded out by the canvas axis scale, or over a range set by the
  caller with optional clamping; vertical bars in eighth blocks, count and
  value axes with round labels, NaN samples ignored, a pure binning function
  (`ctui_histogram_bin()`) and bin lookup for the mouse.
- **heatmap**: grid of caller-owned values (rows x cols) on a sequential
  colour ramp taken from the palette (info, accent, cyan, green, yellow, red),
  with density characters instead without colours or UTF-8, optional row and
  column labels, a min / max legend, NaN cells left empty, a cursor moved with
  the arrow keys, and cell lookup (row, column, value) for the mouse.
- **treemap**: squarified treemap of weighted items in the colours of the
  icon palette, labels and weights clipped to their rectangle, and a pure,
  deterministic layout function (`ctui_treemap_squarify()`) that ignores zero,
  negative, NaN and infinite weights and tiles the area exactly, plus cell
  lookup for the mouse.
- **calendar**: month grid with a cursor day, marked and selected days, week
  and day views with events, repeat rules and events lasting several days. A dialog adds an event with its title and
  its times, or for the whole day.
- **calendar_dialog**: a framed dialog, built on `ctui_form`, to edit the
  recurrence of an event: repeat (none, daily, weekly, monthly, yearly), every
  N, seven weekday checkboxes (weekly only), start date, end (never, on a
  date, after N times) and OK / Cancel. Dates are typed as `YYYY-MM-DD` and
  checked with real calendar rules (month ends, leap years, years 1 to 9999);
  an invalid date, an end before the start, an interval outside 1 to 10000, a
  weekly repeat with no weekday or a count below 1 show a message in the
  dialog and refuse OK, otherwise the next occurrences are previewed. Fields
  that do not apply are disabled. The values from a rule, the check and the
  application to an `ctui_cal_recur` (which keeps its exceptions) are pure
  functions. The demo's Calendar page opens it with `R`.
- **dateinput**: date field `YYYY-MM-DD` edited segment by segment (digits,
  Up / Down, Left / Right, wheel and click on a segment), always a real
  calendar date (month ends, leap years, years 1 to 9999), with a month
  picker built on `ctui_calendar` opened by Enter or a click on the arrow. The
  value converts to and from the day count of `calendar.h`
  (`ctui_dateinput_days()`); `ctui_dateinput_parse()`, `ctui_dateinput_format()`
  and `ctui_dateinput_valid()` are pure; `style` overrides its colours.
- **geo**: Web Mercator maths and a map viewport: longitude / latitude to
  world and tile coordinates, a centre with a fractional zoom limited to a
  range, pan that wraps over the antimeridian and stops at the poles, zoom
  around a point, and the XYZ tiles that cover an area, nearest the centre
  first, with the column wrapped (all pure functions on pixel sizes).
- **mvt**: decoder for Mapbox Vector Tiles (protobuf, no library): layers, key
  and value tables, features with their tags and geometry decoded to points
  and parts; corrupt or truncated tiles are refused, never read past the end.
- **tilecache**: bounded LRU cache of decoded tiles, limited in number and in
  bytes.
- **map**: world map of Braille dots on a canvas. The whole world is drawn
  offline from an embedded Natural Earth coastline, with a zoom cap; with
  `online` set and a tile callback (raw, decompressed MVT bytes for a z / x /
  y, or "not available") the cap is lifted and vector tiles are drawn in
  categories with their own colours and visibility, over-zoomed beyond the
  source's maximum zoom, with segments hugging the tile edges skipped. Keys,
  drag and wheel pan and zoom; `ctui_map_hit()` and `ctui_map_categorize()` are
  pure. The library has no network, SQLite or zlib dependency.

## Tests

```bash
make test
```

The unit tests run with AddressSanitizer and UndefinedBehaviorSanitizer;
pass `SANITIZE=` to disable them on platforms that reject them.
`make test` first checks that every public header compiles on its own
(`make check-headers`).

## Code quality

```bash
make format-check   # clang-format
make lint           # clang-tidy
make format         # apply clang-format
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)
