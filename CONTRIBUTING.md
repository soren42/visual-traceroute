# Contributing

Contributions are welcome! Here's how to get started.

## Development Setup

```sh
git clone https://github.com/<you>/visual-traceroute.git
cd visual-traceroute
./configure
make
make test
```

See [doc/BUILDING.md](doc/BUILDING.md) for dependency details.

## Code Style

- ANSI C11 (`-std=c11 -Wall -Wextra -Wpedantic`)
- 4-space indentation, K&R brace style
- All public symbols prefixed with `ri_`
- Platform-specific code in separate `_darwin.c` / `_linux.c` files
- No global mutable state outside of explicitly scoped contexts

## Adding a New Network Module

1. Declare a platform-neutral API in `src/net/yourmodule.h`
2. Implement platform files: `src/net/yourmodule_darwin.c` and `src/net/yourmodule_linux.c`
3. Add the source files to `Makefile` under the appropriate `PLAT_SRC` block
4. Wire into the discovery pipeline in `src/core/scan.c`

## Adding a New Output Format

1. Create `src/output/out_yourformat.c/h`
2. Add a flag to `RI_OUT_*` in `src/cli.h`
3. Register the format string in `parse_output_formats()` in `src/cli.c`
4. Call the output function from `src/main.c`
5. Add the source to `OPT_SRC` in `Makefile` (conditionally if it has library deps)

## Testing

Unit tests live in `tests/` and use a minimal assertion framework. Tests must not require root or network access -- use `tests/mock_net.c` for canned data.

```sh
make test    # All tests must pass
```

## Pull Requests

- Create a feature branch from `main`
- Keep commits focused and well-described
- Ensure `make test` passes with zero failures
- Ensure `make` produces zero warnings (aside from the overlength-strings cosmetic warning in `threejs_template.h`)
