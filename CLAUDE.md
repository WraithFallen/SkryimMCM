# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Skyrim MCP Server — a Model Context Protocol server that lets AI assistants (Claude, OpenAI, etc.) interact with Skyrim SE/AE via an SKSE plugin. The SKSE plugin runs inside Skyrim's process and exposes game state/actions over a named pipe. The C# MCP server relays between Claude Desktop (stdio) and the plugin (pipe). **Windows only.**

## Architecture

```
Claude Desktop <--stdio/MCP--> C# MCP Server <--named pipe--> SKSE Plugin (inside Skyrim)
```

### SKSE Plugin (`SkyrimMCPPlugin/` — C++)
- **Main.cpp** — SKSE entry point, starts pipe server + event system on kDataLoaded
- **PipeServer.h/cpp** — Named pipe server (`\\.\pipe\SkyrimMCP`) on background thread
- **Protocol.h/cpp** — JSON request/response parsing, routes ~35 actions to GameInterface
- **GameInterface.h/cpp** — All game state functions using CommonLibSSE-NG
- **TaskQueue.h/cpp** — Marshals pipe-thread calls to game thread via SKSE task interface
- **EventSystem.h/cpp** — 17 game event sinks (combat, death, quests, location, inventory, etc.)

Uses CommonLibSSE-NG (main branch, local shallow clone in `extern/`), nlohmann-json, spdlog, fmt. Statically linked (x64-windows-static).

### C# MCP Server (`SkyrimMCP/` — .NET 8)
- **Program.cs** — MCP server host, registers PipeClient singleton, auto-discovers tools
- **SkyrimTools.cs** — 27+ MCP tools using `[McpServerTool]` attributes
- **PipeClient.cs** — Named pipe client, handles connection, reconnection, timeouts
- **SkyrimOffsets.cs** — FormID database (30+ items, 20+ locations) for name→ID resolution

## Build Commands

```powershell
# IMPORTANT: Set vcpkg root (VS Dev PowerShell uses its own bundled copy otherwise)
$env:VCPKG_ROOT = "C:\vcpkg"

# C++ plugin — only need --preset default when CMakeLists/vcpkg.json/CMakePresets.json change
cmake --preset default
cmake --build X:\_work\skyrim_mcp\SkyrimMCPPlugin\build --config Release

# C# MCP server — close Claude Desktop first (exe is locked while running)
dotnet build X:\_work\skyrim_mcp\SkyrimMCP

# Deploy DLL to Skyrim
copy X:\_work\skyrim_mcp\SkyrimMCPPlugin\output\SkyrimMCPPlugin.dll "C:\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\" -Force
```

**Requires:** Visual Studio 2022 C++ workload, CMake 3.21+, vcpkg at `C:\vcpkg`, .NET 8 SDK

## Protocol

JSON over named pipe `\\.\pipe\SkyrimMCP`, newline-delimited.

Request: `{"id":"uuid","action":"...","params":{}}`
Response: `{"id":"uuid","success":bool,"data":{},"error":"..."}`

## Key Technical Decisions

- **Console commands:** Use ConsoleUtilSSE-style `CompileAndRunImpl` with `RELOCATION_ID(21416, patch < 1130 ? 21890 : 441582)`. Credit: VersuchDrei/ConsoleUtilSSE (MIT License). CommonLibSSE-NG's built-in CompileAndRun crashes on Skyrim >= 1.6.1130.
- **NPC targeting:** Use dot-notation `refId.command` (e.g., `FE124B0E.kill`). The `prid` command does NOT persist between separate ExecuteConsoleCommand calls.
- **Thread safety:** All game API calls go through TaskQueue to run on the game thread. Only `ping` and `poll_events` are handled directly on the pipe thread.
- **SEH crash protection:** `__try/__except` around CompileAndRun catches access violations safely.
- **Static linking:** `x64-windows-static` vcpkg triplet required — dynamic linking causes SKSE load failure (0x0000007E).
- **CommonLibSSE-NG includes:** Must come BEFORE any Windows API headers. Use `void*` for HANDLE in headers, include `<windows.h>` only in .cpp files after CommonLibSSE.
- **Async save:** Fire-and-forget from Protocol layer — BGSSaveLoadManager::Save() blocks game thread for 10+ seconds with heavy mod lists.

## Key API Patterns (CommonLibSSE-NG main branch)

- Actor values: `AsActorValueOwner()->GetActorValue()`, not `Actor::GetActorValue()`
- Base actor value: `GetBaseActorValue()`, not `GetPermanentActorValue()`
- Quest objectives: use `state` field, not `IsCompleted()`
- Plugin entry: `SKSEPluginInfo()` / `SKSEPluginLoad()` macros, not manual constinit
- Logging: needs `#include <spdlog/sinks/basic_file_sink.h>`
- Always grep actual headers in `extern/CommonLibSSE-NG/include/` when in doubt

## Project Tracking

- **`project_work_items.json`** — Single source of truth for all features, bugs, and status (72+ items, 11 phases)
- **`ROADMAP.md`** — Human-readable feature roadmap
- **`CHARACTER_BLUEPRINT_SPEC.json`** — Character export/import specification

## Prerequisites for Running
- SKSE64 installed in Skyrim
- Address Library for SKSE (SE or AE version)
- SkyrimMCPPlugin.dll in `<Skyrim>/Data/SKSE/Plugins/`
- Claude Desktop config pointing to the C# MCP server
