# ProxyMan — Project Structure & Contribution Guide

## Overview

ProxyMan is a network-aware transparent proxy engine for Windows. It intercepts outbound TCP traffic at the kernel level using WinDivert, tunnels it through an authenticated HTTP CONNECT proxy, and automatically switches between proxy and direct modes based on whether the machine is connected via Ethernet or Wi-Fi.

---

## Directory Structure

```
proxify-win/
│
├── CMakeLists.txt                ← Root build file (all 3 targets defined here)
├── .gitignore
│
├── docs/                         ← Documentation & design references
│   ├── STRUCTURE.md              ← Project structure & contribution guide
│   ├── AGENT.md                  ← AI Agent strict instructions & guidelines
│   ├── architecture.md           ← Technical architecture overview
│   └── prd.md                    ← Original product requirements document
│
├── vendor/                       ← Third-party vendored dependencies
│   └── WinDivert/                ← WinDivert kernel driver SDK (v2.2.2)
│       ├── include/              ← Header files
│       └── x64/                  ← Pre-built DLL, LIB, and SYS driver
│
├── build/                        ← CMake build output (gitignored)
│   └── Release/
│       ├── ProxyMan.exe          ← Main proxy engine binary
│       ├── ProxyManSetup.exe     ← Self-contained installer
│       └── ProxyManUninstall.exe ← Complete system uninstaller
│
└── src/                          ← All source code lives here
    │
    ├── main.cpp                  ← Entry point: WinMain, console attach, engine loop
    ├── version.h                 ← Version string constants
    │
    ├── cli/                      ← CLI command handlers
    │   ├── parser.cpp/h          ← Argument parsing & command dispatch
    │   ├── help.cpp/h            ← --help output
    │   ├── status.cpp/h          ← --status (engine & proxy state)
    │   ├── stop.cpp/h            ← --stop (signal shutdown, kill processes)
    │   ├── logs.cpp/h            ← --logs / --logs -f (tail & follow)
    │   ├── stats.cpp/h           ← --stats (bandwidth summary)
    │   ├── diagnostics.cpp/h     ← --speedtest, --check-proxies
    │   └── autostart.cpp/h       ← --install-startup, --install-service, etc.
    │
    ├── core/                     ← Proxy engine internals
    │   ├── engine.cpp/h          ← EngineController — orchestrates start/stop
    │   ├── relay.cpp/h           ← Local TCP relay server (127.0.0.1:55555)
    │   ├── http_tunnel.cpp/h     ← HTTP CONNECT tunnel to upstream proxy
    │   └── conn_table.h          ← Connection tracking table (original → redirect)
    │
    ├── net/                      ← Network interception & packet manipulation
    │   ├── packet_engine.cpp/h   ← WinDivert TCP packet redirect engine
    │   ├── packet_headers.h      ← Raw IP/TCP header structs
    │   ├── quic_blocker.cpp/h    ← Drops UDP:443 to force TCP fallback
    │   ├── dns_interceptor.cpp/h ← DNS query interception & caching
    │   └── network_watcher.cpp/h ← Ethernet/Wi-Fi detection & change callbacks
    │
    ├── platform/                 ← Windows OS integration (Win32 API)
    │   ├── elevation.cpp/h       ← UAC elevation check & relaunch
    │   ├── ipc.cpp/h             ← Global mutex, shutdown event, process kill
    │   ├── console.cpp/h         ← Console attach (headless support), Ctrl+C handler
    │   ├── proxy_settings.cpp/h  ← Windows registry proxy enable/disable + DNS flush
    │   ├── process_utils.cpp/h   ← Resolve local port → PID → process name
    │   └── wifi_utils.cpp/h      ← WLAN API SSID detection
    │
    ├── config/                   ← Configuration management
    │   ├── config.cpp/h          ← TOML config loader, Config struct, defaults
    │   └── pool.cpp/h            ← Proxy pool health testing & auto-selection
    │
    ├── logging/                  ← Connection logging & bandwidth tracking
    │   └── data_tracker.cpp/h    ← Per-app data usage, log file writer
    │
    └── installer/                ← Installer & Uninstaller (separate build targets)
        ├── installer.cpp         ← ProxyManSetup.exe — interactive setup wizard
        ├── uninstaller.cpp       ← ProxyManUninstall.exe — full system cleanup
        ├── resources.h           ← Embedded resource IDs
        └── resources.rc.in       ← Resource compiler template (CMake configured)
```

---

## Rules for Future Modifications

### 1. One File, One Responsibility

Every `.cpp` file should do **one thing**. If a file grows beyond **~150 lines**, it's a signal to split it. Ask yourself: *"Can I describe what this file does in one sentence?"* If not, it needs splitting.

### 2. Where to Put New Code

| You're adding... | Put it in... |
|---|---|
| A new CLI command (e.g. `--update`) | `src/cli/` — create `update.cpp/h`, register in `parser.cpp` |
| A new network interception feature | `src/net/` |
| A new Windows OS integration (registry, services, etc.) | `src/platform/` |
| A new proxy tunneling protocol | `src/core/` |
| A new config option | `src/config/config.h` (struct) + `config.cpp` (parsing) |
| A new logging/tracking feature | `src/logging/` |
| Changes to the installer/uninstaller | `src/installer/` |
| A new third-party dependency | `vendor/<dependency_name>/` |

### 3. Include Path Convention

All `#include` paths are **relative to `src/`**. Use the directory prefix:

```cpp
// ✅ Correct
#include "core/engine.h"
#include "config/config.h"
#include "platform/proxy_settings.h"

// ❌ Wrong — no flat includes
#include "engine.h"
#include "config.h"
```

### 4. Header Guard Convention

Use `#pragma once` in all headers. Do not use `#ifndef` / `#define` guards.

```cpp
// ✅ Correct
#pragma once

// ❌ Wrong
#ifndef MY_HEADER_H
#define MY_HEADER_H
...
#endif
```

### 5. Adding a New CLI Command

1. Create `src/cli/mycommand.cpp` and `src/cli/mycommand.h`
2. Implement the handler function (e.g. `int HandleMyCommand();`)
3. Register it in `src/cli/parser.cpp` inside `ParseAndDispatch()` or `ParseElevatedCommands()`
4. Add the new `.cpp` to `CMakeLists.txt` under the ProxyMan target
5. Document it in `src/cli/help.cpp`

### 6. Build Targets

The project produces **3 independent executables**, all defined in the root `CMakeLists.txt`:

| Target | Output | Description |
|---|---|---|
| `ProxyMan` | `ProxyMan.exe` | Main proxy engine (WIN32 subsystem, headless) |
| `ProxyManUninstall` | `ProxyManUninstall.exe` | Standalone uninstaller (no dependencies on other src/) |
| `ProxyManSetup` | `ProxyManSetup.exe` | Installer with embedded binaries (depends on ProxyMan + ProxyManUninstall) |

### 7. Things to Never Do

- **Don't put source files in the project root** — everything goes under `src/`
- **Don't create deeply nested subdirectories** — max depth is `src/<category>/<file>`
- **Don't add code to `main.cpp`** unless it's directly related to the engine startup loop
- **Don't modify `vendor/`** — vendored dependencies are upstream snapshots
- **Don't commit build artifacts** — `build/` is gitignored
- **Don't use global variables** — the only exception is `g_shutdownRequested` (defined in `main.cpp`, declared extern in `platform/console.h`)

### 8. Building

```powershell
# Configure
cmake -B build -S .

# Build all targets (Release)
cmake --build build --config Release

# Build a specific target
cmake --build build --config Release --target ProxyMan
cmake --build build --config Release --target ProxyManSetup
cmake --build build --config Release --target ProxyManUninstall
```
