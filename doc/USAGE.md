# Usage

visual-traceroute discovers your network topology starting from the local machine and outputs the result in various formats.

> **Note:** Network discovery requires raw socket access. Run with `sudo` on macOS, or grant `CAP_NET_RAW` on Linux.

## Basic Usage

```sh
# Discover local network, output as JSON
sudo visual-traceroute -o json

# Discover local network with verbose output
sudo visual-traceroute -vv -o json -f mynetwork

# Traceroute to a target host
sudo visual-traceroute -t 8.8.8.8 -d 5 -o json

# Multiple output formats at once
sudo visual-traceroute -t 8.8.8.8 -o json,curses,png,html -f network

# Traceroute with hop scanning (discover neighbors at each router)
sudo visual-traceroute -t 8.8.8.8 -d 5 --hop-scan -vvv -o json,html -f hopscan
```

## Command-Line Options

```
Usage: visual-traceroute [OPTIONS]

  -t, --target HOST    Deep traceroute toward HOST
  -d, --depth N        Max hops beyond gateway (default: 1)
  -v                   Increase verbosity (-v to -vvvvv)
  -o, --output FMT     Output formats: json,curses,png,mp4,html
  -f, --file PATH      Output filename base (default: network)
  -4                   IPv4 only
  -6                   IPv6 only
  --no-mdns            Disable mDNS discovery
  --no-arp             Disable ARP cache reading
  --subnet-scan        Enable ping sweep of local subnets
  --hop-scan           Probe /24 around each traceroute hop
  -h, --help           Show help
  --version            Show version
```

## Output Formats

### JSON (`-o json`)

Writes `<file>.json` containing the full network graph:

```json
{
  "hosts": [
    {
      "id": 0,
      "display_name": "my-macbook.local",
      "type": "local",
      "ipv4": "192.168.1.100",
      "mac": "aa:bb:cc:dd:ee:ff",
      "hop_distance": 0
    }
  ],
  "edges": [
    {
      "src": 0,
      "dst": 1,
      "weight": 1.5,
      "type": "gateway",
      "in_mst": true
    }
  ]
}
```

### Curses (`-o curses`)

Interactive terminal tree display. Shows hosts as a BFS tree rooted at the local machine, traversing MST edges. Color-coded by type:
- Green: local host
- Yellow: gateways
- Blue: LAN hosts
- Magenta: remote routers
- Red: target host

Press any key to exit.

### PNG (`-o png`)

Writes `<file>.png` -- a 1920x1080 graph visualization with:
- Radial tree layout based on Kruskal MST
- Color-coded nodes by host type
- Node labels (display names)
- MST edges highlighted, non-MST edges dimmed
- Legend in the top-left corner

Requires cairo and libpng.

### MP4 (`-o mp4`)

Writes `<file>.mp4` -- a 10-second 1920x1080 video at 30fps showing the network graph rotating 360 degrees in 3D space. The traceroute path forms the Z-axis spine.

Requires cairo and FFmpeg.

### HTML (`-o html`)

Writes `<file>.html` -- a self-contained interactive 3D visualization. Open in any modern browser. Features:
- Three.js 3D scene with orbit controls (drag to rotate, scroll to zoom)
- Color-coded sphere nodes with floating text labels
- MST edges as white lines, non-MST edges as gray
- Click a node to see its details in a panel
- Camera controls panel (bottom-left): d-pad panning, elevation, zoom
- Route navigation panel (bottom-right): step node-by-node along the traceroute spine
- Autoplay flythrough (top-center): automatically fly the camera along the route
- Keyboard shortcuts: WASD (pan), QE (elevate), +/- (zoom), N/P (next/prev hop), Space (toggle fly)
- WebXR VR button for VR headset navigation (if supported)

No server required -- opens directly as a local file.

## Verbosity Levels

| Flag | Level | Output |
|------|-------|--------|
| (none) | 0 | Errors only |
| `-v` | 1 | Warnings |
| `-vv` | 2 | Info (recommended) |
| `-vvv` | 3 | Debug |
| `-vvvv` | 4 | Trace |
| `-vvvvv` | 5 | Packet-level detail |

## Discovery Phases

When run, visual-traceroute executes six discovery phases in order:

1. **Interfaces** -- Enumerates local network interfaces, IPs, and MACs into a single consolidated host entry
2. **Routing** -- Reads the routing table for default gateways
3. **LAN** -- Reads the ARP cache (and optionally ping sweeps subnets with `--subnet-scan`)
4. **Names** -- Reverse DNS and mDNS browsing for all discovered hosts
5. **Traceroute** -- ICMP traceroute toward `--target` if specified. With `--hop-scan`, each intermediate router's /24 subnet is pinged to discover neighboring hosts at that hop.
6. **IPv6** -- ICMPv6 multicast neighbor discovery on each interface
