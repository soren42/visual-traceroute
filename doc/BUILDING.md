# Building

## Requirements

### Required
- C11 compiler (gcc or clang)
- POSIX shell (for configure script)
- pkg-config (recommended, for library detection)

### Optional Dependencies

| Library | Purpose | Package (macOS) | Package (Debian/Ubuntu) |
|---------|---------|-----------------|------------------------|
| ncurses | Terminal tree output | Built-in or `brew install ncurses` | `libncurses-dev` |
| cairo | PNG rendering | `brew install cairo` | `libcairo2-dev` |
| libpng | PNG support (via cairo) | `brew install libpng` | `libpng-dev` |
| FFmpeg (libavcodec, libavformat, libswscale, libavutil) | MP4 video output | `brew install ffmpeg` | `libavcodec-dev libavformat-dev libswscale-dev libavutil-dev` |
| dns_sd | mDNS on macOS | Built-in (macOS SDK) | N/A |
| avahi-client | mDNS on Linux | N/A | `libavahi-client-dev` |

Features are automatically detected and enabled/disabled at configure time.

## Quick Start

```sh
./configure
make
```

The binary is built at `bin/visual-traceroute`.

## Configure Options

```
./configure [OPTIONS]

Options:
  --prefix=DIR          Install prefix [/usr/local]
  --cc=COMPILER         C compiler [cc]
  --without-ncurses     Disable ncurses output
  --without-cairo       Disable PNG/MP4 output
  --without-ffmpeg      Disable MP4 output
  --without-dns-sd      Disable macOS mDNS
  --without-avahi       Disable Linux mDNS
  --without-libpng      Disable libpng
```

The configure script generates two files:
- `config.h` -- C preprocessor feature flags (`HAVE_NCURSES`, `HAVE_CAIRO`, etc.)
- `config.mk` -- Makefile variables (compiler flags, library paths)

## Build Targets

```sh
make          # Build bin/visual-traceroute
make test     # Build and run unit tests (no root needed)
make clean    # Remove all build artifacts (including config.h and config.mk)
```

## Platform Notes

### macOS (ARM and x86)

Uses `_DARWIN_C_SOURCE` for full POSIX + BSD API access. The system provides `dns_sd.h` for mDNS. Raw ICMP sockets require root (`sudo`).

```sh
# Install optional dependencies
brew install cairo libpng ffmpeg ncurses pkg-config

./configure
make
make test
```

### Linux (ARM and x86)

Uses `_POSIX_C_SOURCE=200809L` and `_DEFAULT_SOURCE`. Raw ICMP sockets require root or `CAP_NET_RAW`.

```sh
# Install dependencies (Debian/Ubuntu)
sudo apt-get install build-essential pkg-config \
    libncurses-dev libcairo2-dev libpng-dev \
    libavcodec-dev libavformat-dev libswscale-dev libavutil-dev \
    libavahi-client-dev

./configure
make
make test
```

To run without full root, grant raw socket capability:
```sh
sudo setcap cap_net_raw+ep bin/visual-traceroute
```

## Minimal Build

To build with only JSON and HTML output (no optional libraries):

```sh
./configure --without-ncurses --without-cairo --without-ffmpeg --without-libpng
make
```

## Troubleshooting

**configure can't find libraries**: Ensure pkg-config is installed and that library `.pc` files are on the `PKG_CONFIG_PATH`.

**Permission denied on ICMP**: Run with `sudo` or grant `CAP_NET_RAW` on Linux.

**make clean removes config.mk**: Re-run `./configure` after `make clean`.
