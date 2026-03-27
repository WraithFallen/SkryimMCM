# SkyLink AI — Usage Guide

## Getting Started

1. Launch Skyrim via SKSE loader
2. Wait for the game to fully load (main menu or in-game)
3. Open Claude Desktop
4. Verify connection: *"Is Skyrim connected?"*

The MCP server automatically connects when both Skyrim and Claude Desktop are running.

## Basic Commands

### Player Info
```
"What's my health and level?"
"What are my skill levels?"
"What perks do I have?" (page 1)
"How many spells do I know?" (summary only, no data loaded)
"What am I wearing?"
"What do I look like?" (face morphs, head parts, race)
"What factions am I in?"
"What are my magic resistances?"
"Am I a vampire?"
"What powers do I have?"
```

### Inventory
```
"What's in my inventory?" (paged — page 1, 50 items)
"Show me inventory page 3"
"How many items do I have?" (summary only)
"Give me 5000 gold"
"Give me 10 iron swords"
"Remove 5 lockpicks"
"What enchantments are on Potema's Crown?" (pass FormID)
```

### Equipment
```
"What am I wearing?" (all 32 biped slots)
"Equip the iron sword"
"Unequip my helmet"
```

### Quests
```
"What quests am I on?"
"Show me the stages of quest 00074793"
"Who fills the aliases in the main quest?"
"What quest items do I have?"
"Set quest stage to 200"
"Start quest / stop quest"
```

### NPCs & Followers
```
"Who's nearby?"
"Who are my followers?"
"Tell me about Inigo" (detailed NPC info)
"What is Inigo carrying?"
"Can that guard see me?" (detection level)
"What am I looking at?" (crosshair target)
"Kill that NPC" (by refId)
"Move Lydia to me"
```

### World
```
"Teleport me to Whiterun"
"What's the weather?"
"Make it snow"
"List all weather types"
"Where exactly am I?" (cell info + coordinates)
"What objects are nearby?" (containers, doors, furniture)
"What's in that chest?" (container inventory)
"Unlock that door"
"Discover all map markers"
"What time is it in-game?"
"Set time to noon"
```

### Combat
```
"Am I in combat?" (targets + allies)
"What's my damage and armor rating?"
"Any threats nearby?" (hostile actors with weapons)
"Toggle god mode"
"Toggle immortal mode" (takes damage, can't die)
"Toggle collision" (noclip)
```

### Economy
```
"Any merchants nearby?"
"What does the merchant sell?"
"Do I have any bounties?"
"Clear my bounty with The Pale"
```

### Magic
```
"Tell me about the spell Fireball" (search first, then details)
"What enchantments are on my ring?"
"What are my resistances?"
"Am I diseased?"
"What daily powers do I have?"
```

### Save & Load
```
"Save my game" (auto-named with character/location/timestamp)
"Take a snapshot" (blueprint + save with matching names)
"Load my most recent save"
"Load save named MyTestSave"
```

### Events
```
"What happened recently?" (poll events)
"Show me only combat and death events"
"Poll events excluding lock changes"
```

### Utility
```
"Search for iron sword" (form database search)
"What's the FormID prefix for HLIORemi.esp?"
"Show me the load order"
"What SKSE plugins do I have?"
"Is the game in a safe state?"
"What menus are open?"
```

### Papyrus Bridge
```
"What Papyrus functions are available?" (scans 7400+ source files)
"What can ConsoleUtil do?" (function signatures)
"What can the Experience mod do?"
```

### Loot & Analysis
```
"Scan for valuable loot nearby"
"Export my character blueprint to Downloads"
"What's my damage and armor compared to nearby threats?"
```

## Advanced Usage

### Paging Large Results

Tools that return many items support `page` and `pageSize` parameters:
- `page=1, pageSize=50` — default, returns first 50 items
- `page=0` — summary only (just the total count, no data)
- `page=3, pageSize=25` — third page of 25 items

This saves tokens and context window space. Ask *"How many spells do I have?"* instead of *"Show me all my spells"*.

### Character Blueprint

The blueprint is a complete character snapshot exported to a JSON file:
- Player info, skills, perks (700+), spells (1400+), shouts
- Equipment (32 slots), full inventory with enchantments
- Appearance (19 face morphs, head parts, race, height)
- Factions, resistances, powers, diseases, damage stats

Always use `outputPath` — the data is too large for the MCP response.

### Event Filtering

Poll events supports filters to reduce noise:
- `eventTypes` — only return specific types: `"combat,death,quest_stage"`
- `excludeTypes` — exclude noisy types: `"lock_changed,quest_start_stop"`

Framework quest noise (NFF cycling, etc.) is auto-filtered.

### Console Command Fallback

`ExecuteConsoleCommand` is a last resort. Always prefer dedicated tools:
- Instead of `player.getav health` → use `GetPlayerInfo`
- Instead of `sqv questId` → use `GetQuestStages` + `GetQuestAliases`
- Instead of `help "iron" 0` → use `SearchForms`

When using console commands, target NPCs with dot-notation: `FE124B0E.kill` (not `prid`).

### Game Safety

Check `GetGameSafety` before risky operations (spawning, teleporting during combat). The tool reports:
- `isLoading` — game is loading, commands will crash
- `isInKillMove` — animation in progress, commands may crash
- `isSaving` — save in progress, commands may corrupt

Some tools (like ToggleImmortalMode) automatically check safety and use fallback strategies.

## Troubleshooting

### "Cannot connect to Skyrim"
- Ensure Skyrim is running via SKSE (not the vanilla launcher)
- Check that `SkyrimMCPPlugin.dll` is in `Data/SKSE/Plugins/`
- Verify Address Library for SKSE is installed

### Console commands not executing
- Check the SKSE plugin log: `Documents/My Games/Skyrim Special Edition/SKSE/SkyrimMCPPlugin.log`
- Use dedicated tools instead of console commands when possible

### Game crashes on command
- Some console commands are unsafe during certain game states
- Use `GetGameSafety` before risky operations
- Commands during loading screens, kill moves, or active saves may crash

### MCP server timeout
- Heavy operations (save game, character blueprint) may take 10+ seconds
- Save game is async — returns immediately, completes in background
- Blueprint export uses `outputPath` to write directly to disk

### Events too noisy
- Use `excludeTypes` to filter out lock_changed and quest_start_stop
- Framework quest noise is auto-filtered but some may still appear

### Tool not found
- Close and reopen Claude Desktop to reload the tool list
- C# changes don't need a game restart, just a Claude Desktop restart
- C++ changes require a full game restart
