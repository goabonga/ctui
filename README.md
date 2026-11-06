# ctui

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
