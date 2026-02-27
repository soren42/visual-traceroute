# Architecture

visual-traceroute is a modular ANSI C11 tool that discovers network topology and renders it in multiple output formats. This document describes the high-level design and key abstractions.

## Module Layout

```
src/
├── main.c              Entry point, dispatches to scanner and output modules
├── cli.c/h             Argument parsing (getopt_long) -> ri_config_t
├── log.c/h             Six-level logging to stderr (ERROR..PACKET)
├── core/
│   ├── host.c/h        ri_host_t: network host with IPv4/IPv6/MAC/hostnames
│   ├── edge.c/h        ri_edge_t: weighted graph edge with type and MST flag
│   ├── graph.c/h       ri_graph_t: adjacency list graph with Kruskal MST
│   ├── scan.c/h        Six-phase discovery orchestrator
│   └── json_out.c/h    Graph -> cJSON serialization
├── net/
│   ├── iface.c/h       Interface enumeration via getifaddrs()
│   ├── ri_route.h      Routing table API (platform-neutral)
│   ├── route_darwin.c   macOS: sysctl(CTL_NET, PF_ROUTE)
│   ├── route_linux.c    Linux: /proc/net/route
│   ├── arp.h           ARP cache API (platform-neutral)
│   ├── arp_darwin.c     macOS: sysctl RTF_LLINFO
│   ├── arp_linux.c      Linux: /proc/net/arp
│   ├── icmp.h          ICMP ping/probe API (platform-neutral)
│   ├── icmp_darwin.c    macOS: struct icmp, IP header in recv buffer
│   ├── icmp_linux.c     Linux: struct icmphdr, no IP header
│   ├── icmp6.c/h       ICMPv6 ping, probe, and multicast neighbor discovery
│   ├── dns.c/h         Reverse DNS via getnameinfo()
│   ├── mdns.h          mDNS browsing API (platform-neutral)
│   ├── mdns_darwin.c    macOS: dns_sd.h (DNSServiceBrowse/Resolve)
│   ├── mdns_linux.c     Linux: avahi-browse fallback
│   └── ping.c/h        ICMP ping sweep for subnet discovery
├── output/
│   ├── out_json.c/h        JSON file output
│   ├── out_curses.c/h      ncurses BFS tree with color
│   ├── out_png.c/h         Cairo 2D graph rendering
│   ├── out_mp4.c/h         FFmpeg H.264 rotating 3D video
│   ├── out_html.c/h        Three.js + WebXR self-contained HTML
│   ├── layout.c/h          Radial-tree + force-directed layout
│   └── threejs_template.h  HTML/JS template as C string constant
└── util/
    ├── alloc.c/h       Checked malloc/realloc/free (abort on failure)
    ├── strutil.c/h     Safe string operations
    └── platform.h      Cross-platform ICMP macros, constants
```

## Data Structures

### ri_host_t (core/host.h)

Represents a network host. Fields include:
- IPv4 address, IPv6 address, MAC address (primary + secondary arrays)
- Hostname, mDNS name, DNS reverse name, display name
- Host type enum: `LOCAL`, `LAN`, `GATEWAY`, `REMOTE`, `TARGET`
- Hop distance from local host, RTT in milliseconds
- 3D coordinates (x, y, z) for layout
- Comma-separated interfaces list (for consolidated LOCAL hosts)

### ri_edge_t (core/edge.h)

A weighted directed edge between two hosts:
- Source and destination host IDs (indices into graph)
- Weight (RTT-based)
- Edge type: `LAN`, `ROUTE`, `GATEWAY`
- `in_mst` flag set by Kruskal's algorithm

### ri_graph_t (core/graph.h)

The network topology graph:
- Dynamic arrays of hosts and edges
- Adjacency list (array of edge-index lists per host)
- Key operations: `add_host`, `find_by_ipv4`/`ipv6`/`mac`, `add_edge`, `has_edge`
- `ri_graph_kruskal_mst()`: Union-Find with path compression and rank
- `ri_graph_bfs_mst()`: BFS traversal over MST edges only

## Discovery Pipeline (scan.c)

The scanner runs six phases sequentially:

1. **Interface enumeration** -- `getifaddrs()` collects local IPs, MACs, interface names. Creates a single consolidated LOCAL host node with all addresses.

2. **Routing table** -- Platform-specific code reads the routing table to find default gateways. Creates GATEWAY nodes and GATEWAY edges.

3. **LAN discovery** -- Reads the ARP cache for known neighbors. Optionally runs a ping sweep (`--subnet-scan`) to populate the ARP cache first.

4. **Name resolution** -- Reverse DNS (`getnameinfo`) on all discovered hosts. mDNS browsing for `_services._dns-sd._udp.local` if not disabled. mDNS hosts are matched by IP (resolved via `getaddrinfo`).

5. **Remote traceroute** -- If `--target` is specified, sends ICMP echo requests with incrementing TTL (1..30) to discover intermediate routers. With `--hop-scan`, probes the /24 subnet around each intermediate hop to discover neighboring hosts.

6. **IPv6 augmentation** -- ICMPv6 multicast echo to `ff02::1` on each interface to discover link-local IPv6 neighbors.

After scanning, display names are computed for all hosts (priority: DNS > hostname > mDNS > IP > "(unknown)") and Kruskal MST is run.

## Cross-Platform Strategy

Platform differences are isolated behind neutral C headers that declare the API. Separate `_darwin.c` and `_linux.c` files implement the platform-specific details. The Makefile compiles only the correct files based on `PLATFORM` from configure.

Key platform differences:
- **Routing table**: macOS uses `sysctl(CTL_NET, PF_ROUTE)` with `rt_msghdr`; Linux parses `/proc/net/route`
- **ARP cache**: macOS uses `sysctl` with `RTF_LLINFO` and `sockaddr_dl`; Linux parses `/proc/net/arp`
- **ICMP**: macOS uses `struct icmp` and includes the IP header in received buffers; Linux uses `struct icmphdr` without IP header
- **mDNS**: macOS uses `dns_sd.h` (DNSServiceBrowse/Resolve); Linux uses avahi-browse via `popen()`
- **Interface MAC**: macOS uses `AF_LINK`/`sockaddr_dl`; Linux uses `AF_PACKET`/`sockaddr_ll`

## Layout Algorithm (layout.c)

1. **Radial 2D**: BFS from local host over MST. Depth 0 at origin. Gateways placed in [0, pi], LAN hosts in [pi, 2*pi]. Each depth ring at increasing radius.

2. **3D extension**: Z coordinate assigned by depth (traceroute spine along Z-axis), XY radial for branches.

3. **Force-directed refinement**: 50 iterations of Fruchterman-Reingold to reduce edge crossings while preserving the radial structure.

## Output Formats

- **JSON**: Full graph serialization via cJSON (hosts array + edges array + metadata)
- **Curses**: BFS tree over MST with color-coded host types, using ncurses
- **PNG**: Cairo 1920x1080 rendering with auto-scaling, legend, and node labels
- **MP4**: FFmpeg H.264 encoding of 3D layout rotating 360 degrees over 10 seconds (30fps, 1920x1080). Cairo renders each frame, swscale converts ARGB to YUV420P.
- **HTML**: Self-contained file with Three.js (CDN importmap), OrbitControls, CSS2DRenderer labels, raycaster click detection, WebXR VRButton for VR headset navigation, camera controls panel, route navigation, and autoplay flythrough.
