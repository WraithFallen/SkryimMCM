# Skyrim MCP

A Model Context Protocol (MCP) server that lets AI assistants (Claude, etc.) interact directly with Skyrim SE/AE's runtime engine. Read game state, manipulate inventory, manage quests, control NPCs — all through a structured API.

## Architecture

```
Claude Desktop <--stdio/MCP--> C# MCP Server <--named pipe--> SKSE Plugin (inside Skyrim)
```

**SKSE Plugin** (C++ DLL) — runs inside Skyrim's process via SKSE. Uses CommonLibSSE-NG for typed engine access. Exposes game state and actions over a named pipe (`\\.\pipe\SkyrimMCP`).

**C# MCP Server** (.NET 8) — bridges Claude Desktop (stdio MCP) and the SKSE plugin (named pipe). Auto-discovers tools via `[McpServerTool]` attributes.

## Features

### 30+ Game Actions via Named Pipe

| Category | Actions |
|----------|---------|
| **Player State** | Get player info, inventory, gold count, active effects |
| **Items** | Add/remove items, equip/unequip, FormID lookup |
| **Combat & Stats** | Set health/magicka/stamina, check combat status, god mode |
| **Quests** | Get quest info, get/set quest stage, start/stop/complete quests |
| **NPCs** | Get nearby NPCs, get actor info, set actor values, move actors, set relationships |
| **Spells & Perks** | Add/remove spells, add/remove perks |
| **World** | Teleport, get/set game time, toggle collision |
| **Console** | Execute any console command via engine API |

### 15 MCP Tools for Claude Desktop

CheckConnection, GetPlayerInfo, GetInventory, GetGoldCount, ExecuteConsoleCommand, AddItem, RemoveItem, Teleport, ToggleGodMode, SetHealth/Magicka/Stamina, ToggleCollision, ListKnownItems, GetQuestInfo, GetNearbyNPCs

## Requirements

- **Windows** (Skyrim is Windows-only)
- **Skyrim SE or AE** with SKSE64 installed
- **Address Library for SKSE** (matching your game version)
- **.NET 8 SDK** (for the MCP server)
- **Visual Studio 2022** with C++ Desktop workload (to build the SKSE plugin)
- **CMake 3.21+** and **vcpkg**

## Building

### SKSE Plugin

```bash
# Clone CommonLibSSE-NG into extern/ (one-time)
git clone https://github.com/CharmedBaryon/CommonLibSSE-NG.git SkyrimMCPPlugin/extern/CommonLibSSE-NG --depth 1

# Configure and build
cd SkyrimMCPPlugin
cmake --preset default
cmake --build build --config Release

# Deploy — copy DLL to Skyrim
copy output\SkyrimMCPPlugin.dll "<Skyrim>\Data\SKSE\Plugins\"
```

### C# MCP Server

```bash
cd SkyrimMCP
dotnet build
```

## Claude Desktop Configuration

Add to `%APPDATA%\Claude\claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "skyrim": {
      "command": "dotnet",
      "args": ["run", "--project", "X:\\_work\\skyrim_mcp\\SkyrimMCP\\SkyrimMCP.csproj"]
    }
  }
}
```

## Usage

1. Launch Skyrim via SKSE
2. Start Claude Desktop
3. Ask Claude to interact with your game:

```
"What's my current health and location?"
"Give me 5000 gold"
"Teleport me to Solitude"
"What quests am I on?"
"Who's nearby?"
```

## Protocol

JSON over named pipe `\\.\pipe\SkyrimMCP`, newline-delimited.

```json
// Request
{"id": "uuid", "action": "get_player_info", "params": {}}

// Response
{"id": "uuid", "success": true, "data": {"name": "Dragonborn", "level": 42, ...}}
```

## Project Structure

```
├── SkyrimMCPPlugin/          # C++ SKSE plugin
│   ├── src/
│   │   ├── Main.cpp          # SKSE entry point
│   │   ├── PipeServer.h/cpp  # Named pipe server
│   │   ├── Protocol.h/cpp    # JSON message routing
│   │   ├── GameInterface.h/cpp # Engine API calls
│   │   └── TaskQueue.h/cpp   # Game thread dispatch
│   ├── CMakeLists.txt
│   └── vcpkg.json
├── SkyrimMCP/                # C# MCP server
│   ├── Program.cs
│   ├── SkyrimTools.cs        # MCP tool definitions
│   ├── PipeClient.cs         # Named pipe client
│   └── SkyrimOffsets.cs      # FormID database
└── src/                      # Legacy Node.js prototype (deprecated)
```

## License

MIT
