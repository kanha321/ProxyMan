# ProxyBridge — Complete Documentation

> **ProxyBridge** is a transparent network proxy tool for Windows. It automatically routes all your internet traffic through a corporate/institutional proxy server — without you having to configure each app manually. Just run it and forget it.

---

## Table of Contents

1. [What Problem Does It Solve?](#1-what-problem-does-it-solve)
2. [How It Works — The Big Picture](#2-how-it-works--the-big-picture)
3. [Features](#3-features)
   - [Network Detection](#31-network-detection)
   - [Auto System Proxy Toggle](#32-auto-system-proxy-toggle)
   - [Local Loopback Relay](#33-local-loopback-relay)
   - [Transparent NAT Redirect Engine](#34-transparent-nat-redirect-engine)
   - [QUIC Blocker](#35-quic-blocker)
   - [Network-Aware Lifecycle Controller](#36-network-aware-lifecycle-controller)
4. [Architecture Overview](#4-architecture-overview)
5. [Requirements](#5-requirements)
6. [Configuration](#6-configuration)
7. [How to Run](#7-how-to-run)
8. [What Happens Step by Step](#8-what-happens-step-by-step)
9. [Frequently Asked Questions](#9-frequently-asked-questions)
10. [File Reference](#10-file-reference)

---

## 1. What Problem Does It Solve?

Imagine you're on a college or corporate network where the internet only works if you go through a **proxy server** (a middleman that checks and forwards your traffic).

The problem is:
- Every app (Chrome, VS Code, curl, git...) needs to be configured **individually** to use the proxy.
- Some apps completely **ignore** proxy settings.
- Some traffic (like QUIC/HTTP3) **bypasses** the proxy silently.

**ProxyBridge solves all of this at the network level.** It intercepts all traffic before it even leaves your computer and silently routes it through the proxy — no per-app configuration needed.

---

## 2. How It Works — The Big Picture

```
  Your App (Chrome, VS Code, curl...)
         │
         │ tries to connect to google.com:443
         ▼
  ┌─────────────────────────┐
  │  WinDivert NAT Engine   │  ← intercepts the packet
  │  (Transparent Redirect) │
  └────────────┬────────────┘
               │ rewrites destination to 127.0.0.1:55555
               ▼
  ┌─────────────────────────┐
  │    Local Relay Server   │  ← runs on your machine
  │   (127.0.0.1:55555)     │
  └────────────┬────────────┘
               │ sends HTTP CONNECT tunnel request
               ▼
  ┌─────────────────────────┐
  │   Corporate Proxy       │  ← e.g. 172.31.100.25:3128
  │   (with auth)           │
  └────────────┬────────────┘
               │ forwards to real destination
               ▼
         google.com:443
```

Your app never knows any of this happened — it just sees a normal working connection.

---

## 3. Features

### 3.1 Network Detection

**What it does:** Automatically detects what kind of network you are currently connected to — Ethernet cable, Wi-Fi, or neither.

**Why it matters:** The proxy is typically only needed on your institution's wired Ethernet network. If you're on Wi-Fi (e.g. at home), you don't need the proxy and it would actually break your internet if left on.

**How it works:** Uses the Windows `GetBestInterface()` and `GetAdaptersAddresses()` APIs to check which network adapter is currently active and what type it is.

**Result:** ProxyBridge knows whether to turn ON or stay OFF automatically.

---

### 3.2 Auto System Proxy Toggle

**What it does:** Automatically sets (and clears) the Windows system proxy setting — the same setting you'd normally configure in `Settings → Network → Proxy`.

**Why it matters:** Apps that respect Windows system proxy (like Edge, Chrome, Microsoft Store apps, curl) will immediately start routing through the proxy without any manual configuration.

**How it works:**
- Writes `ProxyEnable=1` and `ProxyServer=172.31.100.25:3128` to the Windows registry.
- Calls `InternetSetOptionW` to broadcast the change so running apps pick it up instantly — no restart needed.
- When Ethernet disconnects, sets `ProxyEnable=0` to restore normal internet.

---

### 3.3 Local Loopback Relay

**What it does:** Runs a mini-server on your own computer at `127.0.0.1:55555`. Every connection that arrives here gets forwarded through the real proxy to the original destination.

**Why it matters:** The NAT engine redirects all traffic to this relay. The relay handles the actual job of authenticating with the upstream proxy and tunneling the connection through.

**How it works:**
1. A connection arrives (e.g. your app was trying to reach `google.com:443`).
2. The relay looks up what the original destination was (stored in a connection table).
3. It sends an `HTTP CONNECT google.com:443` request to the upstream proxy, including your username and password encoded in Base64.
4. The proxy accepts and opens a tunnel.
5. The relay then pipes data in both directions between your app and the tunnel.

**Result:** Your app's data flows seamlessly through the proxy tunnel without knowing about it.

---

### 3.4 Transparent NAT Redirect Engine

**What it does:** Intercepts every outgoing TCP connection at the Windows kernel level and silently redirects it to the local relay.

**Why it matters:** Without this, apps that don't support proxies (many command-line tools, games, update services) would bypass the proxy entirely. This engine captures everything regardless.

**How it works (using WinDivert):**
- Opens a Windows kernel-level packet filter for all outbound TCP traffic.
- When it sees a new connection going to, say, `8.8.8.8:443`:
  - Saves the original destination into a **connection table**, keyed by the app's local port.
  - Rewrites the destination in the packet to `127.0.0.1:55555` (the relay).
  - Recalculates checksums and reinserts the packet into the network stack.
- When the relay's reply comes back:
  - Rewrites the source address back to the original destination so the app thinks it's talking directly to the server.

**Safety guard:** Traffic going to the proxy server itself (`172.31.100.25:3128`) is never redirected — that would cause an infinite loop.

---

### 3.5 QUIC Blocker

**What it does:** Silently drops all outgoing UDP packets on port 443, forcing browsers to fall back from QUIC (HTTP/3) to standard TCP (HTTP/2).

**Why it matters:** Modern browsers like Chrome and Edge use a newer protocol called **QUIC** that runs over UDP instead of TCP. Since the NAT engine only handles TCP, QUIC traffic would sneak past the proxy undetected. The QUIC Blocker prevents this.

**How it works:**
- Opens a WinDivert filter for `outbound UDP port 443`.
- Receives matching packets and **intentionally does not reinject them** — they are silently dropped.
- Chrome/Edge detect within milliseconds that QUIC isn't working and automatically switch to TCP.
- The NAT engine then catches the TCP connection and proxies it normally.

**User experience:** No error messages. Pages load normally. No perceptible slowdown.

---

### 3.6 Network-Aware Lifecycle Controller

**What it does:** Ties everything together. Watches for network changes and starts or stops the entire engine automatically. Supports clean shutdown without crashing or leaving stale state.

**Why it matters:** Without this, you'd have to manually start and stop ProxyBridge every time you plug/unplug your Ethernet cable. With it, everything is fully automatic.

**How each component stops cleanly:**

| Component | Blocking Call | How It Stops |
|:---|:---|:---|
| `StoppablePacketEngine` | `WinDivertRecv()` | Closes WinDivert handle → unblocks the recv call |
| `StoppableQuicBlocker` | `WinDivertRecv()` | Same — handle closure unblocks the loop |
| `StoppableRelay` | `accept()` | Closes listener socket → unblocks accept, then waits up to 2s for active connections to drain |

**Start order:** Relay → Packet Engine → QUIC Blocker
**Stop order:** Packet Engine → QUIC Blocker → Relay (relay last so in-flight connections can finish)

**Ctrl+C handling:** On Ctrl+C, cleanly stops the engine and clears the system proxy before exiting.

---

## 4. Architecture Overview

```
proxifier_lite.exe
│
├── main.cpp                      ← Entry point, Ctrl+C handler
│   ├── NetworkWatcher            ← Detects Ethernet / Wi-Fi / None
│   └── EngineController          ← Starts / stops entire engine
│       ├── StoppableRelay        ← Local proxy relay (port 55555)
│       │   └── http_proxy_client ← HTTP CONNECT + Basic Auth
│       ├── StoppablePacketEngine ← WinDivert TCP NAT redirect
│       │   └── ConnTable         ← Maps app ports to original destinations
│       └── StoppableQuicBlocker  ← WinDivert UDP:443 drop
│
└── proxy_settings.cpp            ← Windows registry proxy toggle
```

---

## 5. Requirements

| Requirement | Details |
|:---|:---|
| **OS** | Windows 10 / Windows 11 (64-bit) |
| **Privileges** | Must run as **Administrator** (WinDivert requires kernel access) |
| **WinDivert** | `WinDivert.dll` and `WinDivert64.sys` must be in the same folder as the `.exe` (copied automatically by the build) |
| **Network** | Ethernet connection to your institutional network |

---

## 6. Configuration

ProxyBridge reads a plain text file called `proxy-config.txt`.

**Location:** One directory above the executable, or the current directory as fallback.

**Format:**
```
proxy_ip=172.31.100.25
proxy_port=3128
proxy_user=edcguest
proxy_pass=edcguest
relay_port=55555
```

| Field | Description |
|:---|:---|
| `proxy_ip` | IP address of your upstream proxy server |
| `proxy_port` | Port of your upstream proxy server |
| `proxy_user` | Username for proxy authentication |
| `proxy_pass` | Password for proxy authentication |
| `relay_port` | Local port for the relay server (default: 55555) |

---

## 7. How to Run

```powershell
# 1. Open PowerShell or CMD as Administrator

# 2. Navigate to the build output folder
cd d:\randoms\proxify-win\proxifier-lite\build\Release

# 3. Run ProxyBridge
.\proxifier_lite.exe
```

**Expected output when on Ethernet:**
```
==========================================================
  ProxyBridge — Network-Aware Transparent Proxy Engine
==========================================================
  Proxy:  172.31.100.25:3128  (user: edcguest)
  Relay:  127.0.0.1:55555
  Mode:   Auto (Ethernet=ON, Wi-Fi=OFF)
==========================================================

[Main] Initial network: Ethernet
[Main] Ethernet detected — starting engine and setting system proxy.
[Relay] Started — listening on 127.0.0.1:55555
[PacketEngine] Started — intercepting TCP flows to 127.0.0.1:55555
[QuicBlocker] Started — dropping outbound UDP:443

[Main] ProxyBridge is running. Press Ctrl+C to exit.
```

**To stop:** Press `Ctrl+C` — ProxyBridge cleanly shuts down and restores proxy settings.

---

## 8. What Happens Step by Step

Complete trace of what happens when you open `https://google.com` in Chrome:

1. **Chrome tries to connect** to `142.250.80.46:443` via QUIC (UDP).
2. **QUIC Blocker** intercepts the UDP packet and drops it silently.
3. **Chrome detects QUIC failure** and automatically retries with TCP.
4. **NAT Engine** intercepts the TCP SYN packet from Chrome.
5. NAT Engine records `{ srcPort=54321 → 142.250.80.46:443 }` in the connection table.
6. NAT Engine rewrites the destination to `127.0.0.1:55555` and recomputes checksums.
7. **Local Relay** receives the connection from Chrome on port 55555.
8. Relay looks up port 54321 → finds original destination `142.250.80.46:443`.
9. Relay connects to `172.31.100.25:3128` and sends: `CONNECT 142.250.80.46:443 HTTP/1.1` with `Authorization: Basic ZWRjZ3Vlc3Q6ZWRjZ3Vlc3Q=`.
10. **Upstream proxy** verifies credentials and opens a tunnel to Google.
11. Relay pipes data between Chrome and the proxy tunnel in both directions.
12. NAT Engine rewrites return packets so Chrome sees them as coming from `142.250.80.46:443`.
13. **Chrome loads Google** — completely unaware any of this happened.

---

## 9. Frequently Asked Questions

**Q: Do I need to configure Chrome or any other app?**
> No. ProxyBridge works at the network level. All apps are proxied automatically.

**Q: Will it slow down my internet?**
> Barely. The NAT redirect and relay overhead is negligible (under 1ms). The QUIC→TCP fallback adds a few milliseconds on the first load of a site per session, after which browsers remember and use TCP directly.

**Q: What happens when I switch from Ethernet to Wi-Fi?**
> ProxyBridge detects the change within about 1.5 seconds, stops the engine, and clears the system proxy automatically. Your Wi-Fi works normally.

**Q: What if I close the terminal window?**
> The close event is handled — ProxyBridge stops the engine and clears system proxy settings before the process exits.

**Q: What if the proxy server is down?**
> Individual connections will fail (apps see a connection error), but ProxyBridge itself keeps running and retrying for new connections.

**Q: Does it work with HTTPS?**
> Yes. The relay uses `HTTP CONNECT` which creates an opaque tunnel — HTTPS traffic passes through encrypted and the proxy cannot read it.

**Q: Does it affect localhost or LAN traffic?**
> No. The NAT filter only intercepts non-loopback outbound traffic. Local addresses (`127.x.x.x`, `192.168.x.x`, etc.) are never redirected.

**Q: What is WinDivert and is it safe?**
> WinDivert is an open-source Windows packet interception library, widely used in security tools, VPNs, and proxies. It requires Administrator privileges to install its kernel driver. The signed official build (v2.2.2) is used here.

---

## 10. File Reference

### Production Files
| File | Description |
|:---|:---|
| [`proxifier_lite.exe`](file:///d:/randoms/proxify-win/proxifier-lite/build/Release/proxifier_lite.exe) | Main executable — run this as Administrator |
| [`WinDivert.dll`](file:///d:/randoms/proxify-win/proxifier-lite/build/Release/WinDivert.dll) | WinDivert user-mode library |
| [`WinDivert64.sys`](file:///d:/randoms/proxify-win/proxifier-lite/build/Release/WinDivert64.sys) | WinDivert kernel driver |
| [`proxy-config.txt`](file:///d:/randoms/proxify-win/proxy-config.txt) | Your proxy credentials and settings |

### Source Files
| File | Description |
|:---|:---|
| [main.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/main.cpp) | Entry point, Ctrl+C handler, NetworkWatcher wiring |
| [engine_controller.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/engine_controller.cpp) | Orchestrates ordered start/stop of all components |
| [packet_engine.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/packet_engine.cpp) | WinDivert TCP NAT redirect engine |
| [quic_blocker.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/quic_blocker.cpp) | WinDivert UDP:443 drop engine |
| [relay.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/relay.cpp) | Local TCP relay with HTTP CONNECT + Basic Auth |
| [conn_table.h](file:///d:/randoms/proxify-win/proxifier-lite/src/conn_table.h) | Thread-safe connection mapping table |
| [network_watcher.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/network_watcher.cpp) | Debounced OS network interface watcher |
| [proxy_settings.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/proxy_settings.cpp) | Windows registry system proxy toggle |
| [http_proxy_client.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/http_proxy_client.cpp) | HTTP CONNECT client with Base64 Basic Auth |
| [config.cpp](file:///d:/randoms/proxify-win/proxifier-lite/src/config.cpp) | Config file loader |

---

*ProxyBridge — Built with WinDivert 2.2.2 · Windows 10/11 x64 · Proxy: 172.31.100.25:3128*
