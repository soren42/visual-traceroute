# visual-traceroute

A network topology discovery tool that maps your route to any host and renders the results as interactive 3D visualizations, static images, video, and structured data.

Built in portable C11. Runs on macOS (ARM/x86) and Linux (ARM/x86).

## What It Does

visual-traceroute starts at your machine and works outward:

1. **Discovers local interfaces** -- IPs, MACs, interface names
2. **Reads the routing table** -- finds your default gateway(s)
3. **Scans the LAN** -- ARP cache, optional ping sweep
4. **Resolves names** -- reverse DNS and mDNS browsing
5. **Traces the route** -- ICMP traceroute with incrementing TTL toward a target host
6. **Discovers IPv6 neighbors** -- ICMPv6 multicast on each interface

With `--hop-scan`, it also probes the /24 subnet around each intermediate router, revealing neighboring infrastructure at every hop.

The result is a weighted graph of hosts and edges, rendered as a Kruskal minimum spanning tree.

## Output Formats

| Format | Flag | Description |
|--------|------|-------------|
| **JSON** | `-o json` | Full graph data (hosts + edges + metadata) |
| **HTML** | `-o html` | Interactive 3D visualization (Three.js + WebXR) |
| **PNG** | `-o png` | Static 1920x1080 graph image (Cairo) |
| **MP4** | `-o mp4` | 10-second rotating 3D video (FFmpeg) |
| **Curses** | `-o curses` | Color-coded terminal tree (ncurses) |

Multiple formats can be combined: `-o json,html,png`

### HTML Viewer

The HTML output is a self-contained file (no server needed) with:
- Three.js 3D scene with orbit controls
- Color-coded nodes: green (local), yellow (gateway), blue (LAN), magenta (remote), red (target)
- Click any node to inspect its details
- **Camera controls** (bottom-left): d-pad panning, elevation, zoom -- hold to repeat
- **Route navigation** (bottom-right): step through traceroute hops one by one
- **Fly Route** (top-center): automated flythrough along the traceroute spine
- **Keyboard**: WASD (pan), QE (up/down), +/- (zoom), N/P (next/prev hop), Space (toggle fly)
- WebXR VR support for headset navigation

## Quick Start

```sh
# Build
./configure
make

# Discover local network
sudo ./bin/visual-traceroute -vv -o json,html -f network

# Trace route to Google DNS with 3D output
sudo ./bin/visual-traceroute -t 8.8.8.8 -d 5 -vvv -o json,html,png -f route

# Deep scan -- probe neighbors at every hop
sudo ./bin/visual-traceroute -t 8.8.8.8 -d 5 --hop-scan -vvv -o html -f hopscan

# Open the result
open hopscan.html    # macOS
xdg-open hopscan.html  # Linux
```

> **Note:** Raw ICMP sockets require root. Use `sudo` on macOS, or grant `CAP_NET_RAW` on Linux:
> ```sh
> sudo setcap cap_net_raw+ep bin/visual-traceroute
> ```

## Options

```
Usage: visual-traceroute [OPTIONS]

  -t, --target HOST    Traceroute toward HOST
  -d, --depth N        Max hops beyond gateway (default: 1)
  -v                   Increase verbosity (-v to -vvvvv)
  -o, --output FMT     Output: json,curses,png,mp4,html
  -f, --file PATH      Output filename base (default: network)
  -4                   IPv4 only
  -6                   IPv6 only
  --no-mdns            Disable mDNS discovery
  --no-arp             Disable ARP cache reading
  --subnet-scan        Ping sweep local subnets
  --hop-scan           Probe /24 around each traceroute hop
  -n, --nameserver IP  Use custom DNS server for reverse lookups
  -h, --help           Show help
  --version            Show version
```

## Dependencies

**Required:** C11 compiler, POSIX shell, pkg-config

**Optional** (auto-detected by `./configure`):

| Library | Enables |
|---------|---------|
| ncurses | `-o curses` terminal tree |
| cairo + libpng | `-o png` static image |
| cairo + FFmpeg | `-o mp4` video |
| dns_sd (macOS) | mDNS host discovery |
| avahi (Linux) | mDNS host discovery |

JSON and HTML output work with no optional dependencies.

See [doc/BUILDING.md](doc/BUILDING.md) for full build instructions.

## Project Structure

```
src/
  main.c, cli.c/h, log.c/h      Core entry point and CLI
  core/                           Graph data structures, scanner, JSON serialization
  net/                            Platform-abstracted networking (ICMP, ARP, DNS, mDNS, routing)
  output/                         Renderers (JSON, HTML/Three.js, PNG/Cairo, MP4/FFmpeg, curses)
  util/                           Allocator, string utilities, platform macros
tests/                            Unit tests (96 tests, no root needed)
vendor/cJSON/                     Embedded MIT-licensed JSON library
doc/                              Architecture, build, and usage docs
```

## Documentation

- [doc/USAGE.md](doc/USAGE.md) -- Full CLI reference and examples
- [doc/BUILDING.md](doc/BUILDING.md) -- Build instructions for macOS and Linux
- [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) -- Code structure, data model, and design decisions

## License

MIT. See [LICENSE](LICENSE).

The vendored [cJSON](vendor/cJSON/) library is also MIT licensed.
