# Skyrim MCP — Feature Roadmap

Based on a comprehensive analysis of CommonLibSSE-NG's API surface (~200+ classes, 50+ singletons).

## Current State (v0.2)

### Working (Direct Engine API)
- Player info (name, race, level, stats, position, cell)
- Full inventory read (items, types, weights, values)
- Gold count
- Add/remove items
- Set actor values (health, magicka, stamina, all 18 skills)
- Equip/unequip items
- Add/remove spells and perks
- Active quest list with objectives
- Nearby NPC detection (name, race, level, distance, hostility)
- Actor info query by FormID
- Move actor to player/reference
- Set relationship rank
- Active magical effects
- Game time read
- Combat status
- Load order query
- Mod FormID prefix lookup
- Form search by name/editor ID (replacement for `help` command)
- Save game (via BGSSaveLoadManager)
- Start/Stop quests (direct API)

### Broken (Depends on CompileAndRun)
- Console command execution
- Teleportation (coc)
- Set quest stage (setstage)
- Complete quest objectives
- Set game time
- Toggle god mode / collision

---

## Phase 1 — Fix Console Commands & Core Stability

**Priority: Critical**

- [ ] Fix `CompileAndRun` crash — investigate thread safety, try alternative console execution methods
- [ ] Alternative: implement key actions as direct API calls to eliminate console dependency:
  - Teleport via cell lookup + `MoveTo` or `MoveToCell`
  - God mode via direct flag manipulation on Actor
  - Set quest stage via quest API (may need SKSE Papyrus bridge)
  - Time manipulation via `RE::Calendar` / `BSTimer::SetGlobalTimeMultiplier()`
- [ ] Validate SEH doesn't leave game in corrupted state after catching exceptions
- [ ] Add connection health monitoring (periodic ping, auto-reconnect)

## Phase 2 — Deep Player Management

**Priority: High** | **API: RE::PlayerCharacter, RE::TESNPC, RE::ActorValueOwner**

- [ ] **Skill Management** — Read/write all 18 skills, advance skill XP
- [ ] **Perk Tree Browser** — List available perks per skill tree, show requirements, add/remove perks by name
- [ ] **Spell Book** — List all known spells, filter by school/type, favorites management via `MagicFavorites`
- [ ] **Shout Management** — List unlocked shouts/words of power, unlock new words
- [ ] **Level Management** — Set level, add XP, show XP to next level
- [ ] **Appearance Query** — Read race, sex, head parts, face morphs (19 facial values via `TESNPC::FaceData`)
- [ ] **Carry Weight** — Read/set carry weight, show encumbrance status

## Phase 3 — World Interaction

**Priority: High** | **API: RE::TESObjectCELL, RE::TESWorldSpace, RE::Calendar, RE::TESWeather**

- [ ] **Weather Control** — Get current weather, set weather (clear, rain, snow, storm), wind speed/direction
- [ ] **Time Control** — Get/set game hour, day, month, year, day of week, time scale multiplier
- [ ] **Cell Info** — Current cell details (interior/exterior, water, lighting, encounter zone)
- [ ] **World Position** — Exact coordinates, worldspace name, region
- [ ] **Object Interaction** — Lock/unlock doors, activate objects, read container contents
- [ ] **Map Markers** — Discover/undiscover map markers, list discovered locations
- [ ] **Placed Object Query** — Find specific objects in current cell (chests, crafting stations, NPCs)

## Phase 4 — Quest Intelligence

**Priority: High** | **API: RE::TESQuest, RE::BGSQuestObjective, RE::BGSStoryEvent**

- [ ] **Quest Stage Browser** — List all stages for a quest, show which are completed
- [ ] **Quest Variable Read** — Read quest script variables (safe alternative to `sqv`)
- [ ] **Objective Management** — Display/complete/fail individual objectives
- [ ] **Quest Item Tracking** — List items flagged as quest objects, which quest they belong to
- [ ] **Story Event Tracking** — Monitor major story milestones
- [ ] **Quest Alias Resolution** — Resolve quest aliases to actual NPCs/objects
- [ ] **Quest Dependency Mapping** — Show quest prerequisites and follow-up quests

## Phase 5 — NPC & Faction Deep Dive

**Priority: Medium** | **API: RE::Actor, RE::TESFaction, RE::BGSRelationship, RE::CombatController**

- [ ] **Faction Membership** — List player's factions with ranks, join/leave factions, set rank
- [ ] **NPC Detailed Info** — Class, combat style, voice type, outfit, home location
- [ ] **NPC Disposition** — Read/set relationship rank, faction reactions
- [ ] **NPC Inventory** — Read any NPC's inventory (not just player)
- [ ] **NPC State** — Is essential, is protected, is ghost, is child, is follower
- [ ] **Follower Management** — List current followers, dismiss, recruit (via faction/relationship)
- [ ] **Combat AI** — Read aggression level, confidence, combat style, current target
- [ ] **Detection** — Player detection state per NPC (unaware, suspicious, detected)

## Phase 6 — Magic & Effects Deep Dive

**Priority: Medium** | **API: RE::MagicTarget, RE::ActiveEffect, RE::EffectSetting, RE::EnchantmentItem**

- [ ] **Active Effects Browser** — Detailed view of all active effects with remaining duration
- [ ] **Disease/Cure Management** — List diseases, cure specific diseases, check vampirism/lycanthropy
- [ ] **Enchantment Info** — Read enchantments on equipped items, charge levels
- [ ] **Magic Resistance** — Read all resistance values (fire, frost, shock, poison, magic, disease)
- [ ] **Spell Details** — Cost, casting type, delivery, school, effect list per spell
- [ ] **Power Management** — Daily powers usage tracking, lesser powers

## Phase 7 — Economy & Trade

**Priority: Medium** | **API: RE::InventoryChanges, RE::TESFaction**

- [ ] **Merchant Detection** — Find nearby merchants, their specializations
- [ ] **Merchant Inventory** — Read what a merchant has for sale
- [ ] **Price Calculation** — Calculate buy/sell prices accounting for Speech skill and perks
- [ ] **Bounty Management** — Read/clear bounties per hold
- [ ] **Property Ownership** — List owned houses, available properties

## Phase 8 — Combat Analysis

**Priority: Medium** | **API: RE::CombatController, RE::CombatGroup, RE::CombatInventory**

- [ ] **Combat Status** — Detailed combat state (target, group members, fleeing status)
- [ ] **Damage Analysis** — Current weapon damage, armor rating, resistances
- [ ] **Combat Log** — Track recent hits, kills, spell casts during combat
- [ ] **Threat Assessment** — Nearby hostile actors with levels and equipment
- [ ] **Kill Stats** — Total kills by type

## Phase 9 — UI & Notifications

**Priority: Low** | **API: RE::UI, RE::BGSMessage**

- [ ] **Menu State Query** — Which menus are open, is game paused
- [ ] **Notification System** — Send custom notifications to the player's HUD
- [ ] **Map Marker Management** — Add/remove custom map markers
- [ ] **Journal Integration** — Read journal entries, add custom notes

## Phase 10 — Advanced Automation

**Priority: Low** | **Composite features built on earlier phases**

- [ ] **Quest Completion Scripts** — Multi-step quest resolution (set stages, add items, move NPCs, update factions)
- [ ] **Character Build Planner** — Set all skills, perks, spells to match a build template
- [ ] **Inventory Optimizer** — Suggest items to sell/keep based on weight/value ratio
- [ ] **Loot Scanner** — Scan nearby containers and report valuable items
- [ ] **Mod Conflict Detector** — Check for FormID conflicts across loaded mods
- [ ] **Game State Snapshot** — Full dump of player state for backup/analysis
- [ ] **Scripted Scenes** — Move NPCs, set relationships, trigger events for custom scenarios

## Phase 11 — Audio & Animation (Experimental)

**Priority: Low** | **API: RE::BSAudioManager, RE::BSMusicManager, RE::BipedAnim**

- [ ] **Music Control** — Change music track, battle music state
- [ ] **Volume Control** — Master/category volume adjustment
- [ ] **Play Idle Animations** — Trigger specific idles on player or NPCs
- [ ] **Screenshot Trigger** — Take screenshots programmatically

---

## Technical Debt & Infrastructure

- [ ] **VPrint Hook** — Full console output capture for multi-line command results
- [ ] **Config File** — `.ini` or `.toml` for pipe name, log level, feature toggles
- [ ] **Error Recovery** — Automatic Script object cleanup after SEH exceptions
- [ ] **Performance** — Cache frequently queried data (inventory, quest list) to reduce game thread blocking
- [ ] **Pipe Protocol v2** — Batch requests, subscriptions (push notifications for events like combat start, quest update)
- [ ] **Event System** — Subscribe to game events (level up, quest complete, combat start/end, cell change) via SKSE event sinks
- [ ] **Multi-client** — Support multiple MCP clients simultaneously (Claude + other tools)

---

## Key Singletons Reference

| Singleton | Access | Purpose |
|-----------|--------|---------|
| `RE::PlayerCharacter::GetSingleton()` | Player actor | Stats, inventory, spells, position |
| `RE::TESDataHandler::GetSingleton()` | Form database | All forms, mods, load order |
| `RE::UI::GetSingleton()` | UI state | Menu queries, pause state |
| `RE::Calendar::GetSingleton()` | Time | Game hour, date, day of week |
| `RE::BGSSaveLoadManager::GetSingleton()` | Save system | Save/load game files |
| `RE::ProcessLists::GetSingleton()` | Actor lists | Nearby loaded actors |
| `RE::BSAudioManager::GetSingleton()` | Audio | Volume, music control |
| `RE::BSInputDeviceManager::GetSingleton()` | Input | Key/mouse/gamepad state |
| `RE::PlayerControls::GetSingleton()` | Movement | Player input state |
| `RE::ActorEquipManager::GetSingleton()` | Equipment | Equip/unequip items |
| `RE::ConsoleLog::GetSingleton()` | Console | Output capture |
| `RE::MagicFavorites::GetSingleton()` | Favorites | Spell/item favorites |
