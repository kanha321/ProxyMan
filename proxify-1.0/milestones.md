# ProxyBridge — Build Milestones

Companion to [`prd.md`](file:///d:/randoms/proxify-win/prd.md). Ten milestones, ordered from easiest to hardest, structured so each one produces something independently testable before the next adds complexity. Milestones 1–9 correspond to code already delivered in this project; Milestone 10 is the integration work still called out as **Planned** in the PRD.

Difficulty reflects a mix of: new concepts introduced, how hard failures are to debug, and how much of the result depends on correctness you can't fully verify without a real Windows machine.

---

## Milestone 1 — Detect the active network type
**Difficulty:** ★☆☆☆☆☆☆☆☆☆ (1/10)

**Goal:** A console app that prints "Ethernet", "Wi-Fi", or "Other" for whatever adapter is actually carrying your default route right now.

**Tasks:**
- `GetBestInterface()` against a dummy destination to find the real default-route adapter (not just "what's up").
- `GetAdaptersAddresses()` to read that adapter's `IfType`.

**Definition of done:** Run it with Ethernet plugged in → prints "Ethernet". Unplug, connect Wi-Fi → run again → prints "Wi-Fi".

**Maps to:** `network_watcher.cpp` — `GetActiveLinkType()`.

**Why it's first:** Pure request/response Win32 API calls. No threads, no callbacks, no drivers, nothing to get wrong asynchronously.

---

## Milestone 2 — React to network changes live
**Difficulty:** ★★☆☆☆☆☆☆☆☆ (2/10)

**Goal:** The same detection, but event-driven — the app sits idle and prints a new line automatically whenever you plug/unplug or switch networks, without polling in a loop.

**Tasks:**
- Register a callback via `NotifyIpInterfaceChange()`.
- Debounce: network transitions fire in bursts of intermediate events; wait until things settle (~1.5s of quiet) before acting once.

**Definition of done:** Unplug Ethernet → exactly one "Wi-Fi" line appears within ~2 seconds, not five flickering lines.

**Maps to:** `network_watcher.cpp` — `NetworkWatcher` class.

**Why it's next:** Introduces a background thread and a callback contract you didn't design (must match the OS's exact signature), plus your first real debounce logic — a small step up, not a big one.

---

## Milestone 3 — Auto-toggle the Windows system proxy
**Difficulty:** ★★★☆☆☆☆☆☆☆ (3/10)

**Goal:** A complete, if limited, working product: when Ethernet is active, turn on the Windows system proxy setting; when it isn't, turn it off — live, tied to Milestone 2's watcher.

**Tasks:**
- Write `ProxyEnable` / `ProxyServer` / `ProxyOverride` to the WinINet registry key.
- Call `InternetSetOption` with `INTERNET_OPTION_SETTINGS_CHANGED` and `INTERNET_OPTION_REFRESH` so running apps notice.

**Definition of done:** Switch to Ethernet, open a WinINet-respecting app (e.g. an app using system proxy settings), confirm it starts using the proxy — without restarting that app.

**Maps to:** `proxy_settings.cpp`.

**Why it's next:** New API surface (registry + WinINet) but still no threading complexity beyond what M2 gave you. This is also where you first hit the *limitation* that motivates everything after it: some apps don't reliably honor this without a restart, which is exactly why the project continues.

---

## Milestone 4 — SOCKS5 handshake, standalone
**Difficulty:** ★★★★☆☆☆☆☆☆ (4/10)

**Goal:** A small test program that connects to a real SOCKS5 proxy and fetches one URL through it — proving the protocol implementation works before it's buried inside anything else.

**Tasks:**
- Implement the method-negotiation + CONNECT request/response per RFC 1928.
- Test against a real or local SOCKS5 server.

**Definition of done:** Program prints the fetched page's first line, proxied, with no other moving parts in the system.

**Maps to:** `socks5_client.cpp`.

**Why it's next:** First time you're implementing a wire protocol by hand and parsing a variable-length response. Isolated and easy to debug with a packet sniffer if it goes wrong — good place to build that muscle before adding auth and interception on top.

---

## Milestone 5 — HTTP CONNECT with Basic auth, standalone
**Difficulty:** ★★★★★☆☆☆☆☆ (5/10)

**Goal:** Same idea as Milestone 4, but for an HTTP proxy requiring credentials — the backend this project actually uses.

**Tasks:**
- Base64 encoding (by hand, no library).
- Build and send the `CONNECT` request with `Proxy-Authorization: Basic`.
- Byte-by-byte header parsing so you never read past the blank line into the tunnel's actual data.

**Definition of done:** Program fetches a URL through a real credential-protected HTTP proxy, and fails cleanly (not hangs) on wrong credentials.

**Maps to:** `http_proxy_client.cpp`.

**Why it's harder than M4:** SOCKS5's framing is fixed-size and binary; HTTP's is text, variable-length, and you're responsible for not over-reading into the payload that follows the headers — a subtle correctness trap if you buffer in bulk instead of reading carefully.

---

## Milestone 6 — Local relay, tested the easy way
**Difficulty:** ★★★★★★☆☆☆☆ (6/10)

**Goal:** A relay listening on `127.0.0.1`, that authenticates upstream via Milestone 5's client — validated by manually pointing a browser's *own* proxy setting at it, before attempting anything transparent.

**Tasks:**
- Config file loader (credentials out of the command line entirely).
- Accept loop + bidirectional byte-pumping between two sockets.
- Point a browser directly at `127.0.0.1:<relay port>` and browse normally.

**Definition of done:** Full browsing session works through the relay, with the real proxy's credentials never entered into the browser.

**Maps to:** `relay.cpp`, `config.cpp`.

**Why it's a deliberate step before interception:** This isolates "does my relay + auth logic work at all" from "does my packet redirection work" — if something's wrong here, you find out with normal browser dev tools instead of a packet capture.

---

## Milestone 7 — Packet capture, read-only
**Difficulty:** ★★★★★★★☆☆☆ (7/10)

**Goal:** Get WinDivert installed and working: open a capture handle, log every outbound TCP SYN's destination, and reinject every packet completely unmodified.

**Tasks:**
- Download/link the WinDivert SDK; confirm the driver loads.
- Open a `WINDIVERT_LAYER_NETWORK` handle, write the capture loop.
- **Specifically verify** the loopback-packets-are-outbound-only behavior described in the PRD, on your actual machine, before trusting it.

**Definition of done:** Every connection any app makes shows up in your log with the right destination, and nothing on the machine notices you're running — because you haven't changed anything yet.

**Maps to:** `packet_engine.cpp` (capture loop only, no rewrite logic yet).

**Why the jump in difficulty:** First kernel-driver dependency, first Administrator requirement, and your first exposure to an API whose exact behavior you can only fully confirm by testing on a real machine, not by reading code.

---

## Milestone 8 — The actual transparent redirect
**Difficulty:** ★★★★★★★★★☆ (9/10)

**Goal:** Combine Milestones 6 and 7 for real: rewrite intercepted packets' destinations to the relay, and rewrite the relay's replies back to look like they came from the original destination — so an app that never configured any proxy gets proxied anyway.

**Tasks:**
- Connection table (client port → original destination).
- Forward-leg rewrite (every packet of the flow, not just the SYN).
- Return-leg rewrite (source address correction).
- Checksum recalculation after every modification.
- Loop prevention (never redirect the relay's own upstream connection).

**Definition of done:** Open a browser *without touching its settings at all*, browse normally, and confirm (via the real proxy's own access log) that its traffic is actually going through the proxy.

**Maps to:** `packet_engine.cpp` (full version), `conn_table.h`.

**Why this is the hardest core piece:** You're hand-building what Linux gets for free from `iptables`/conntrack. Bugs here don't crash loudly — they manifest as "this one app's connections randomly hang" or "works for HTTP but not HTTPS," which is much harder to root-cause than a compile error.

---

## Milestone 9 — Close the QUIC gap, then harden
**Difficulty:** ★★★★★★★★☆☆ (8/10)

**Goal:** Stop QUIC-capable apps from silently routing around your TCP-only redirect, and shake out edge cases across real, varied apps.

**Tasks:**
- Second WinDivert handle dropping outbound UDP:443.
- Stale connection-table sweeping for crashed/ungraceful closes.
- Test across several real apps (browser, a CLI tool, a game or downloader) concurrently, not just one at a time.

**Definition of done:** `chrome://net-internals/#quic` shows no active QUIC sessions, the equivalent traffic completes over TCP instead, and running several different apps simultaneously doesn't produce cross-talk or stuck connections in the connection table.

**Maps to:** `quic_blocker.cpp`, plus hardening passes on `packet_engine.cpp`/`relay.cpp`.

**Why it's slightly easier than M8 despite depending on it:** The new code here (the QUIC blocker) is genuinely simple — the difficulty is almost entirely in the *testing* breadth, not new mechanism.

---

## Milestone 10 — Tie it all together: network-aware lifecycle
**Difficulty:** ★★★★★★★★★★ (10/10)

**Goal:** Milestone 3's automatic behavior (Ethernet → on, Wi-Fi → off), but driving the *real* redirect engine from Milestones 7–9 instead of the WinINet setting — the fully seamless end state described in the PRD.

**Tasks:**
- Make `RunPacketEngine()` and `RunQuicBlocker()` cancellable (their capture loops currently block forever) — e.g. calling `WinDivertClose()` from a controller thread to unblock a pending `WinDivertRecv()`.
- Make `RunRelay()`'s accept loop cancellable the same way.
- Decide and implement what happens to in-flight connections on a network transition: drain vs. hard-close.
- Wire `NetworkWatcher`'s debounced callback to start/stop the whole engine, and make sure rapid flapping (e.g. a laptop docking/undocking repeatedly) doesn't thrash it.

**Definition of done:** Unplug Ethernet while actively browsing through the proxy → redirection stops cleanly within the debounce window, with no hung connections or crashed process. Plug back in → resumes automatically.

**Maps to:** New controller code, not yet written — this is the one milestone with no existing implementation to lean on.

**Why it's hardest:** Every earlier milestone assumed "runs until you kill the process." This one requires retrofitting clean shutdown into loops that were written to block forever, correctly handling partially-torn-down state (a relay with connections still active, an engine mid-rewrite), and getting all of that right under a start/stop cycle that can happen repeatedly and without warning — the most concurrency-sensitive work in the whole project.
