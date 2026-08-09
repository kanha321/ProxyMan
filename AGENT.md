# Agent Instructions & Guidelines

> [!IMPORTANT]
> **CRITICAL DIRECTIVE**: Always read and analyze [`STRUCTURE.md`](file:///d:/randoms/proxify-win/STRUCTURE.md) FIRST before proposing, designing, or implementing any code changes, new features, file additions, or refactoring in this repository.

---

## Mandatory Rules for AI Agents

### 1. Analyze `STRUCTURE.md` First
Before starting any task, read [`STRUCTURE.md`](file:///d:/randoms/proxify-win/STRUCTURE.md) to understand:
- The modular directory structure (`src/cli/`, `src/core/`, `src/net/`, `src/platform/`, `src/config/`, `src/logging/`, `src/installer/`)
- Directory placement conventions for new files
- Include path rules and header guard policies

### 2. File Granularity & Scope
- Keep files small, single-purpose, and human-readable (~50–150 lines max).
- Do NOT dump CLI commands, OS integration logic, or helper functions into `main.cpp`.
- If a file exceeds ~150 lines, split it logically.

### 3. Include Path Convention
- All `#include` directives MUST be relative to `src/` (e.g. `#include "core/engine.h"`, `#include "config/config.h"`).

### 4. Header Guards
- Use `#pragma once` in all `.h` header files. Do not use `#ifndef` guards.

### 5. Build Verification
- Always verify changes by building all 3 targets (`ProxyMan`, `ProxyManSetup`, `ProxyManUninstall`):
  ```powershell
  cmake -B build -S .
  cmake --build build --config Release
  ```
