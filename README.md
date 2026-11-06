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
