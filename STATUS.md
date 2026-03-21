# Skyrim MCP Server - Development Status

**Last Updated:** March 21, 2026
**Status:** Phase 2 — Direct Engine API Working, Console Commands Broken

## Architecture

```
Claude Desktop <--stdio/MCP--> C# MCP Server <--named pipe--> SKSE Plugin (inside Skyrim)
```

Two components:
1. **SKSE Plugin** (`SkyrimMCPPlugin/`) — C++ DLL loaded by SKSE into Skyrim's process. Uses CommonLibSSE-NG for typed engine access. Exposes game state over named pipe `\\.\pipe\SkyrimMCP`. Statically linked (x64-windows-static).
2. **C# MCP Server** (`SkyrimMCP/`) — .NET 8 app relaying between Claude Desktop (stdio MCP) and the SKSE plugin (named pipe).

## Working Tools (Direct Engine API)

| Tool | Action | Method |
|------|--------|--------|
| CheckConnection | `ping` | Pipe connectivity check |
| GetPlayerInfo | `get_player_info` | `PlayerCharacter::GetSingleton()`, `AsActorValueOwner()` |
| GetInventory | `get_inventory` | `Player::GetInventory()` |
| GetGoldCount | `get_gold_count` | Inventory filter by gold FormID |
| AddItem | `add_item` | `AddObjectToContainer()` |
| RemoveItem | `remove_item` | `RemoveItem()` |
| SetHealth/Magicka/Stamina | `set_actor_value` | `AsActorValueOwner()->SetActorValue()` |
| ToggleGodMode | `toggle_god_mode` | Direct static bool flip via `RELOCATION_ID(517711, 404238)` |
| ToggleCollision | `toggle_collision` | `TESObjectREFR::SetCollision()` |
| GetQuestInfo | `get_quest_info` | `TESDataHandler::GetFormArray<TESQuest>()` |
| GetNearbyNPCs | `get_nearby_npcs` | `ProcessLists::highActorHandles` |
| SearchForms | `search_forms` | Direct form database search (replaces `help` command) |
| GetLoadOrder | `get_load_order` | `TESDataHandler::GetLoadedMods/LightMods()` |
| GetModFormIdPrefix | `get_mod_formid_prefix` | `GetLoadedModIndex()` / `GetLoadedLightModIndex()` |
| SaveGame | `save_game` | `BGSSaveLoadManager::Save()` |
| StartQuest | `start_quest` | `TESQuest::Start()` |
| StopQuest | `stop_quest` | `TESQuest::Stop()` |
| ListKnownItems | local | C# FormID database |
| GetActiveEffects | `get_active_effects` | `AsMagicTarget()->GetActiveEffectList()` |
| GetActorInfo | `get_actor_info` | Actor lookup by FormID |
| MoveActorTo | `move_actor_to` | `Actor::MoveTo()` |
| AddSpell | `add_spell` | `PlayerCharacter::AddSpell()` |
| AddPerk | `add_perk` | `PlayerCharacter::AddPerk()` |
| EquipItem | `equip_item` | `ActorEquipManager::EquipObject()` |
| UnequipItem | `unequip_item` | `ActorEquipManager::UnequipObject()` |
| GetGameTime | `get_game_time` | `Calendar::GetSingleton()` |
| SetGameTime | `set_game_time` | `Calendar::gameHour->value` direct write |
| IsInCombat | `is_in_combat` | `PlayerCharacter::IsInCombat()` |

## Broken Tools (Depend on CompileAndRun)

| Tool | Issue |
|------|-------|
| ExecuteConsoleCommand | `Script::CompileAndRun()` crashes with access violation on every call from SKSE task thread |
| Teleport | Uses `coc` console command internally |
| SetQuestStage | Uses `setstage` console command |
| CompleteQuest | Uses `completeallobjectives` console command |

**Root Cause:** `CompileAndRun` is fundamentally incompatible with being called from the SKSE task interface thread. Even the simplest command (`tgm`) crashes. SEH catches the access violation safely (no game crash), but the command doesn't execute.

**Potential Fixes:**
- Find the game's internal console command queue and post directly to it
- Hook the console input handler to inject commands
- Implement remaining actions as direct engine API calls (no `coc` equivalent found yet)

## Build Requirements

- Visual Studio 2022 with C++ Desktop workload
- CMake 3.21+
- vcpkg (`C:\vcpkg`, `VCPKG_ROOT` env var set)
- .NET 8 SDK
- SKSE64 + Address Library for SKSE in Skyrim

### Build Commands

```powershell
# Set vcpkg (VS Dev PowerShell uses its own copy otherwise)
$env:VCPKG_ROOT = "C:\vcpkg"

# C++ plugin (only need --preset default on first build or CMakeLists changes)
cmake --preset default                                    # first time only
cmake --build X:\_work\skyrim_mcp\SkyrimMCPPlugin\build --config Release

# C# MCP server
dotnet build X:\_work\skyrim_mcp\SkyrimMCP

# Deploy
copy X:\_work\skyrim_mcp\SkyrimMCPPlugin\output\SkyrimMCPPlugin.dll "C:\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\" -Force
```

### Build Notes
- Must use `x64-windows-static` vcpkg triplet — dynamic linking causes SKSE load failure (0000007E)
- CommonLibSSE-NG cloned locally to `extern/` with `--depth 1` (don't use FetchContent — it does full clone with no progress)
- CommonLibSSE-NG headers must be included BEFORE any Windows API headers
- `SKSEPluginInfo()` / `SKSEPluginLoad()` macros — don't use old `constinit SKSEPlugin_Version` pattern
- Actor values accessed via `AsActorValueOwner()`, not directly on Actor
- `GetBaseActorValue()` not `GetPermanentActorValue()`
- Quest objectives use `state` field not `IsCompleted()`

## Key Crash Fixes Applied

1. **Static linking** — `x64-windows-static` triplet eliminates external DLL dependencies
2. **SEH crash protection** — `__try/__except` around `CompileAndRun` catches access violations
3. **Script object management** — Fresh Script per call (old one leaked but corruption-free)
4. **Quest iteration safety** — try/catch around each quest in `GetQuestInfo` for modded game stability
5. **God mode / collision** — Direct engine API, no console commands needed

## Project Structure

```
skyrim_mcp/
├── SkyrimMCPPlugin/          # C++ SKSE plugin
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── vcpkg.json
│   ├── extern/CommonLibSSE-NG/  # git clone --depth 1 (not tracked)
│   └── src/
│       ├── Main.cpp          # SKSE entry point
│       ├── PipeServer.h/cpp  # Named pipe server
│       ├── Protocol.h/cpp    # JSON protocol (~30 actions)
│       ├── GameInterface.h/cpp # Engine API calls
│       └── TaskQueue.h/cpp   # Game thread dispatch
├── SkyrimMCP/                # C# MCP server
│   ├── Program.cs            # Host setup
│   ├── SkyrimTools.cs        # ~20 MCP tool definitions
│   ├── PipeClient.cs         # Named pipe client
│   ├── SkyrimOffsets.cs      # FormID database
│   └── SkyrimMCP.csproj
├── src/                      # Legacy Node.js prototype (deprecated)
├── CLAUDE.md
├── ROADMAP.md
├── STATUS.md
└── README.md
```

## Repository

- GitHub: `github.com:jarvann/SkryimMCM.git`
- Branch: `develop` (all active work)
- `main` branch: README + .gitignore only
