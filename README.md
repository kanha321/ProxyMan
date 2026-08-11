# ⚡ ProxyMan — Network-Aware Transparent Proxy Engine for Windows

[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/Platform-Windows--x64-0078D6.svg)](https://microsoft.com/windows)
[![Kernel Interception](https://img.shields.io/badge/Engine-WinDivert%202.2.2-orange.svg)](https://reqrypt.org/windivert.html)

**ProxyMan** is a zero-configuration, kernel-level transparent proxy engine for Windows. Designed specifically for institutional and campus networks (such as MNNIT EDC gateways), ProxyMan automatically intercepts outbound TCP traffic, injects proxy authentication headers, benchmarks proxy pool latencies, and auto-switches between proxy mode (on campus) and direct passthrough mode (off campus).

---

## ✨ Features

- 📶 **Reachability-Based Dual-Mode Auto Switching**: Evaluates campus network availability in real-time. Automatically switches between Proxy Mode (MNNIT Network) and Direct Passthrough Mode (Personal Wi-Fi/Hotspots) while keeping local listener `127.0.0.1:55555` persistently active so IDEs & background tools (Antigravity, VS Code, Node.js) never need to restart.
- 🚀 **Functional Proxy Probing & Parallel Pool Racing**: Performs authentic HTTP `CONNECT` handshakes. Features single-host probe caching with parallel multi-server pool racing on cache miss.
- ⚖️ **Anti-Flap Debouncing & 45s Background Heartbeat**: Engages Proxy Mode instantly on success and requires 2 consecutive failed probes before falling back to Direct Mode. Includes a periodic 45s background reachability heartbeat.
- 🔒 **Kernel Layer 3/4 Interception**: Uses WinDivert to transparently tunnel applications that do not support proxy settings (raw Python scripts, gRPC binaries, Git CLI, etc.).
- 🌐 **Intranet & Local LAN Bypass**: Automatically bypasses private IP subnets (`172.31.x.x`, `10.x.x.x`, `192.168.x.x`) for direct LAN server access (e.g., `http://172.31.100.110/`).
- 🤖 **AI Extension & Token Streaming Optimization**: Resets socket timeouts (`SO_RCVTIMEO`) to infinite keep-alive after HTTP CONNECT handshakes to prevent drops on Server-Sent Events (SSE) and gRPC AI streams (Antigravity, Gemini, ChatGPT).
- 🚫 **QUIC / UDP:443 Blocker**: Drops UDP port 443 packets at the kernel level (`QuicBlocker`), forcing Chrome, Edge, and Google services to instantly fall back to reliable TCP HTTP/1.1 or HTTP/2 CONNECT tunnels.
- 📊 **Real-time Connection & Bandwidth Tracker**: Resolves local sockets to process PIDs and executable names (`git.exe`, `chrome.exe`). Displays live logs (`ProxyMan --logs -f`) and per-app bandwidth reports (`ProxyMan --stats`).
- 🛡️ **100% Headless Execution**: Compiled as a native Windows GUI subsystem binary (`WIN32_EXECUTABLE`) to run completely in the background without black CMD window popups.
- ⚙️ **Zero-UAC Autostart**: Offers Task Scheduler `ONLOGON` task (`--install-startup`) with `HIGHEST` privileges for startup without UAC popups.

---

## 💻 CLI Usage & Commands

```powershell
# Start ProxyMan background daemon
ProxyMan.exe

# View ProxyMan engine status, active network, & system proxy state
ProxyMan.exe --status

# Tail connection logs (use -f to follow live connection stream)
ProxyMan.exe --logs -f

# View session data usage and per-application bandwidth summary
ProxyMan.exe --stats

# Run MNNIT proxy gateway latency benchmark
ProxyMan.exe --speedtest

# Stop active background ProxyMan engine & restore system proxy
ProxyMan.exe --stop

# Register Task Scheduler Autostart (Zero UAC prompts at logon)
ProxyMan.exe --install-startup

# Update EDC proxy authentication credentials
ProxyMan.exe --set-user edcguest edcguest
```

---

## 🛠️ Configuration (`config.toml`)

ProxyMan loads configuration settings from `%USERPROFILE%\.config\proxyman\config.toml`:

```toml
relay_port = 55555

[auth]
user = "edcguest"
pass = "edcguest"

proxy_pool = [
    "172.31.100.25:3128",
    "172.31.100.27:3128",
    "172.31.102.29:3128",
    "172.31.103.29:3128",
    "172.31.100.14:3128"
]

bypass_list = [
    "172.31.*",
    "10.*",
    "127.*",
    "localhost",
    "mnnit.ac.in"
]
```

---

## 📦 Building from Source

### Prerequisites
- Windows 10 / 11 (x64)
- Visual Studio 2019 / 2022 / 2026 with C++ Desktop Workload
- CMake 3.15+

### Build Steps

```powershell
# 1. Clone the repository
git clone https://github.com/kanha321/ProxyMan.git
cd ProxyMan

# 2. Configure build with CMake
cmake -B build -S .

# 3. Build all Release binaries
cmake --build build --config Release
```

The compiled binaries will be generated in `build\Release\`:
- `ProxyMan.exe` — Main Proxy Engine Binary
- `ProxyManSetup.exe` — Standalone Self-Contained Installer
- `ProxyManUninstall.exe` — Complete System Uninstaller Utility

---

## 📂 Project Architecture

```
proxify-win/
├── CMakeLists.txt                ← Root CMake build system
├── README.md                     ← Project README (this file)
├── docs/
│   ├── STRUCTURE.md              ← Internal file structure & modification rules
│   ├── architecture.md           ← Technical architecture & flowcharts
│   └── prd.md                    ← Product requirements document
├── vendor/WinDivert/             ← Vendored WinDivert kernel driver SDK (v2.2.2)
└── src/                          ← Modular C++17 source files
    ├── main.cpp                  ← WinMain entry point & CLI dispatch
    ├── cli/                      ← CLI command handlers (--help, --status, --logs, etc.)
    ├── core/                     ← Engine controller, local relay, & HTTP tunnel
    ├── net/                      ← WinDivert packet engine, QUIC blocker, & network watcher
    ├── platform/                 ← Windows UAC elevation, IPC, console, & proxy registry settings
    ├── config/                   ← TOML config loader & proxy pool selector
    ├── logging/                  ← Connection logger & bandwidth data tracker
    └── installer/                ← ProxyManSetup & ProxyManUninstall source
```

For developer guidelines and structure conventions, see [`docs/STRUCTURE.md`](docs/STRUCTURE.md).
