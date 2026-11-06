# ctui

[![ci](https://github.com/goabonga/ctui/actions/workflows/ci.yml/badge.svg)](https://github.com/goabonga/ctui/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/goabonga/ctui)](https://github.com/goabonga/ctui/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A C library of reusable terminal user interface (TUI) components.

## Build

```bash
make
```

This builds the static library `build/libctui.a`. `make install`
installs it under `$(PREFIX)/lib` and the header `include/ctui/ctui.h` under
`$(PREFIX)/include/ctui` (default `/usr/local`), honouring `DESTDIR`.

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
