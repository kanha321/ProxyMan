# AI Agent Guidance & Operating Protocol — ProxyMan

> [!IMPORTANT]
> **MANDATORY INSTRUCTION FOR ALL AI CODING AGENTS (Antigravity / Gemini / Claude / GPT / Copilot):**
> 
> Before inspecting, modifying, adding, or deleting ANY file in this repository, you **MUST ALWAYS READ AND ANALYZE [`docs/STRUCTURE.md`](file:///d:/randoms/proxify-win/docs/STRUCTURE.md) FIRST**.

---

## 🚨 Core Directives for AI Agents

### 1. Always Read `docs/STRUCTURE.md` First
Every task start or prompt invocation must begin by viewing [`docs/STRUCTURE.md`](file:///d:/randoms/proxify-win/docs/STRUCTURE.md). Use `docs/STRUCTURE.md` as the single source of truth for architectural layout, module responsibilities, and file locations.

### 2. Strict File Placement Rules
- **No Markdown files in Root**: ALL `.md` files must strictly reside inside the `docs/` directory. Never place markdown documentation at the project root.
- **No Source files in Root**: ALL C++ `.cpp` and `.h` source files must strictly reside in their designated subdirectories under `src/` (`src/cli/`, `src/core/`, `src/net/`, `src/platform/`, `src/config/`, `src/logging/`, `src/installer/`). Only `src/main.cpp` and `src/version.h` sit directly under `src/`.
- **Root Directory Cleanliness**: The root directory MUST ONLY contain build configuration (`CMakeLists.txt`, `.gitignore`) and directories (`docs/`, `src/`, `vendor/`, `build/`).

### 3. File Size & Responsibility Limits
- Keep files small, modular, and human-readable (~50 to 150 lines max).
- If a new feature or handler requires more than ~150 lines, split it into a new dedicated module file inside the appropriate `src/` subdirectory.

### 4. Include Path Conventions
Always use `src/` relative include paths:
```cpp
// ✅ Correct
#include "core/engine.h"
#include "config/config.h"
#include "platform/proxy_settings.h"

// ❌ Incorrect
#include "engine.h"
#include "../core/engine.h"
```

### 5. Header Guard Standard
Always use `#pragma once` at the top of every `.h` file. Do not use legacy `#ifndef`/`#define` guard blocks.

### 6. Build Target Integrity
Whenever adding a new `.cpp` file to `src/`, update the root [`CMakeLists.txt`](file:///d:/randoms/proxify-win/CMakeLists.txt) accordingly and verify that all 3 build targets (`ProxyMan`, `ProxyManUninstall`, `ProxyManSetup`) compile cleanly.
