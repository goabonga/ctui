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
