# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Skyrim MCP Server — a Model Context Protocol server that lets Claude interact with Skyrim SE/AE via an SKSE plugin. The SKSE plugin runs inside Skyrim's process and exposes game state/actions over a named pipe. The C# MCP server relays between Claude Desktop (stdio) and the plugin (pipe). **Windows only.**

## Architecture

```
Claude Desktop <--stdio/MCP--> C# MCP Server <--named pipe--> SKSE Plugin (inside Skyrim)
```

### SKSE Plugin (`SkyrimMCPPlugin/` — C++)
- **Main.cpp** — SKSE entry point, starts pipe server on kDataLoaded
- **PipeServer.h/cpp** — Named pipe server (`\\.\pipe\SkyrimMCP`) on background thread, handles client connect/disconnect/reconnect
- **Protocol.h/cpp** — JSON request/response parsing, routes actions to GameInterface
- **GameInterface.h/cpp** — All game state functions using CommonLibSSE-NG (player info, inventory, quests, NPCs, console commands, add/remove items, set actor values, teleport)
- **TaskQueue.h/cpp** — Marshals pipe-thread calls to game thread via `SKSE::GetTaskInterface()->AddTask()` with promise/future bridging

Uses CommonLibSSE-NG (supports both SE 1.5.97 and AE 1.6.x from one DLL) and nlohmann-json.

### C# MCP Server (`SkyrimMCP/` — .NET 8)
- **Program.cs** — MCP server host, registers PipeClient singleton, auto-discovers tools
- **SkyrimTools.cs** — 15 MCP tools using `[McpServerTool]` attributes. Each tool calls `PipeClient.SendRequestAsync()`
- **PipeClient.cs** — Named pipe client, handles connection, reconnection, timeouts, serialization
- **SkyrimOffsets.cs** — FormID database (30+ items, 20+ locations) for name→ID resolution

## Build Commands

### SKSE Plugin
```bash
cd SkyrimMCPPlugin
cmake --preset default          # configure (uses vcpkg)
cmake --build build --config Release
# Output: SkyrimMCPPlugin/output/SkyrimMCPPlugin.dll
# Deploy: copy DLL to <Skyrim>/Data/SKSE/Plugins/
```
**Requires:** Visual Studio 2022 C++ workload, CMake 3.21+, vcpkg (set VCPKG_ROOT env var)

### C# MCP Server
```bash
cd SkyrimMCP
dotnet build
dotnet run          # run the MCP server (stdio transport)
```

## Protocol

JSON over named pipe `\\.\pipe\SkyrimMCP`, newline-delimited.

Request: `{"id":"uuid","action":"...","params":{}}`
Response: `{"id":"uuid","success":bool,"data":{},"error":"..."}`

Actions: `ping`, `get_player_info`, `get_inventory`, `get_gold_count`, `execute_command`, `add_item`, `remove_item`, `set_actor_value`, `teleport`, `get_quest_info`, `get_nearby_npcs`

## Key Conventions

- All game API calls go through TaskQueue to run on the game thread (thread safety)
- `ping` is the only action handled directly on the pipe thread
- FormID lookups still happen in C# via `SkyrimOffsets.cs` before sending to plugin
- Plugin requires Address Library for SKSE installed in Skyrim
- Console commands (tgm, tcl) go through `execute_command` action; direct engine actions preferred where available

## Prerequisites for Running
- SKSE64 installed in Skyrim
- Address Library for SKSE (SE or AE version)
- SkyrimMCPPlugin.dll in `<Skyrim>/Data/SKSE/Plugins/`
- Claude Desktop config pointing to the C# MCP server
