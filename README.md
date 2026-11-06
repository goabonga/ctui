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

## Code quality

```bash
make format-check   # clang-format
make lint           # clang-tidy
make format         # apply clang-format
```

## Continuous integration and releases

The `ci` workflow lints, builds (gcc and clang) and tests (with the
sanitizers) the project in an Arch Linux container, and validates the
workflows and the release configuration.

Releases are computed by [multicz](https://github.com/goabonga/multicz) from
Conventional Commits. After a green `ci` on `main`, the `release` workflow
bumps the version, tags, creates the GitHub release and attaches a
`ctui-<version>-linux-x86_64.tar.gz` binary built on Arch Linux, with its
SHA-256. The tarball carries a build provenance attestation, checked with
`gh attestation verify <tarball> --repo goabonga/ctui`. The workflows need the `GH_PAT`, `GPG_PRIVATE_KEY` and
`GPG_PASSPHRASE` repository secrets.

Commits on `main` must be signed. The branch ruleset in
`.github/rulesets/main.json` enforces this and the required checks; apply it
with `scripts/apply-rulesets.sh`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)
