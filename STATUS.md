# Skyrim MCP Server - Development Status

**Last Updated:** March 18, 2026
**Status:** Phase 2 Implementation Complete — Ready for Build & Test

## Architecture

```
Claude Desktop <--stdio/MCP--> C# MCP Server <--named pipe--> SKSE Plugin (inside Skyrim)
```

Two components:
1. **SKSE Plugin** (`SkyrimMCPPlugin/`) — C++ DLL loaded by SKSE into Skyrim's process. Uses CommonLibSSE-NG for typed engine access. Exposes game state over named pipe `\\.\pipe\SkyrimMCP`.
2. **C# MCP Server** (`SkyrimMCP/`) — .NET 8 app relaying between Claude Desktop (stdio MCP) and the SKSE plugin (named pipe).

## What's Been Completed

### Phase 1: Console Command Injection (replaced)
- Original keyboard-simulation approach has been removed
- Was fragile due to Windows focus-stealing restrictions

### Phase 2: SKSE Plugin + Named Pipe Architecture
- **SKSE Plugin** — Full implementation with:
  - Plugin entry point with SKSE messaging and logging
  - Named pipe server on background thread (auto-reconnect)
  - Thread-safe game-thread dispatch via SKSE task interface
  - JSON protocol for all requests/responses
  - Game interface functions: player info, inventory, gold, console commands, add/remove items, actor values, teleport, quests, nearby NPCs
  - CommonLibSSE-NG for SE 1.5.97 + AE 1.6.x compatibility from one DLL

- **C# MCP Server** — Rewritten with:
  - PipeClient for named pipe communication
  - 15 MCP tools (up from 10)
  - Removed: ConsoleInjector.cs, MemoryReader.cs, WindowsAPI.cs

## MCP Tools (15 total)

| Tool | Description |
|------|-------------|
| CheckConnection | Ping SKSE plugin |
| GetPlayerInfo | Name, race, level, stats, location |
| GetInventory | Full item list with metadata |
| GetGoldCount | Current gold count |
| ExecuteConsoleCommand | Run any console command |
| AddItem | Add items by name/FormID |
| RemoveItem | Remove items by name/FormID |
| Teleport | Move to locations by name |
| ToggleGodMode | Toggle invincibility |
| SetHealth | Set health value |
| SetMagicka | Set magicka value |
| SetStamina | Set stamina value |
| ToggleCollision | Walk through walls |
| ListKnownItems | Show known FormIDs |
| GetQuestInfo | Active quest details |
| GetNearbyNPCs | NPCs in range |

## What Needs To Happen Next

### Build & Test
1. **Build the SKSE plugin** — requires VS2022 C++, CMake, vcpkg
2. **Build the C# MCP server** — `dotnet build` in SkyrimMCP/
3. **Deploy plugin** — copy DLL to `<Skyrim>/Data/SKSE/Plugins/`
4. **Test** — launch Skyrim via SKSE, start Claude Desktop, verify tools work

### Prerequisites
- Visual Studio 2022 with C++ Desktop workload
- CMake 3.21+
- vcpkg with VCPKG_ROOT set
- SKSE64 installed in Skyrim
- Address Library for SKSE (matching game version)

### Future Enhancements
- [ ] Weather control and time-of-day manipulation
- [ ] NPC interaction (dialogue, follow, dismiss)
- [ ] Spell/power management
- [ ] Save game management
- [ ] Map marker discovery
- [ ] Configuration file for pipe name, logging level

## Project Structure

```
skyrim_mcp/
├── SkyrimMCPPlugin/          # C++ SKSE plugin
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── vcpkg.json
│   └── src/
│       ├── Main.cpp          # SKSE entry point
│       ├── PipeServer.h/cpp  # Named pipe server
│       ├── Protocol.h/cpp    # JSON protocol
│       ├── GameInterface.h/cpp # Game state access
│       └── TaskQueue.h/cpp   # Game thread dispatch
├── SkyrimMCP/                # C# MCP server
│   ├── Program.cs            # Host setup
│   ├── SkyrimTools.cs        # MCP tool definitions
│   ├── PipeClient.cs         # Named pipe client
│   ├── SkyrimOffsets.cs      # FormID database
│   └── SkyrimMCP.csproj
├── src/                      # Old Node.js prototype (can be deleted)
├── CLAUDE.md
├── STATUS.md
└── README.md
```
