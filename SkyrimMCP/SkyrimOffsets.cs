using System.Collections.Generic;

namespace SkyrimMCP;

/// <summary>
/// Memory offsets and FormIDs for Skyrim
/// </summary>
public static class SkyrimOffsets
{
    /// <summary>
    /// Memory offsets for different game versions
    /// NOTE: These are placeholder values and need to be researched/updated
    /// </summary>
    public static class MemoryOffsets
    {
        // Skyrim SE 1.5.97 offsets
        public static class SE_1_5_97
        {
            public static readonly long[] PlayerBase = { 0x2F26A78 };
            public static readonly long[] PlayerLevel = { 0x2F26A78, 0x78, 0xB4 };
            public static readonly long[] PlayerHealth = { 0x2F26A78, 0x78, 0x260 };
            public static readonly long[] PlayerMagicka = { 0x2F26A78, 0x78, 0x268 };
            public static readonly long[] PlayerStamina = { 0x2F26A78, 0x78, 0x270 };
        }

        // Skyrim AE 1.6.x offsets
        public static class AE_1_6_X
        {
            public static readonly long[] PlayerBase = { 0x2F6B948 };
            public static readonly long[] PlayerLevel = { 0x2F6B948, 0x78, 0xB4 };
            public static readonly long[] PlayerHealth = { 0x2F6B948, 0x78, 0x260 };
            public static readonly long[] PlayerMagicka = { 0x2F6B948, 0x78, 0x268 };
            public static readonly long[] PlayerStamina = { 0x2F6B948, 0x78, 0x270 };
        }
    }

    /// <summary>
    /// Known FormIDs for common items.
    /// These are base-game (Skyrim.esm) FormIDs, invariant across game versions.
    /// Every entry machine-verified against the live load order via houseCARL
    /// (2026-06-13): 11 IDs were corrected from the original table — the soul-gem
    /// block was shifted (filled-variant IDs, only black was right), petty/BMF
    /// collided on a Greater Soul Gem, steel_dagger pointed at a war axe,
    /// iron_gauntlets at an ArmorAddon, wheat at a nonexistent FormID, and the
    /// health/stamina potions were mis-tiered. Some names below read differently
    /// in-game (Apostasy renames, e.g. Blue Mountain Flower → "Kyne's Kiss",
    /// iron armor → "Northern Iron") but the FormIDs are the correct base records.
    /// </summary>
    public static readonly Dictionary<string, string> Items = new()
    {
        // Currency and basics
        { "gold", "0000000F" },          // Gold001
        { "lockpick", "0000000A" },      // Lockpick

        // Weapons
        { "iron_sword", "00012EB7" },    // IronSword
        { "iron_dagger", "0001397E" },   // IronDagger
        { "steel_sword", "00013989" },   // SteelSword
        { "steel_dagger", "00013986" },  // SteelDagger (was 00013983 = SteelWarAxe)

        // Armor (Apostasy "Northern Iron" set)
        { "iron_armor", "00012E49" },    // ArmorIronCuirass
        { "iron_helmet", "00012E4D" },   // ArmorIronHelmet
        { "iron_boots", "00012E4B" },    // ArmorIronBoots
        { "iron_gauntlets", "00012E46" },// ArmorIronGauntlets (was 00012E4C = IronHelmetAA)

        // Potions (Restore* tier: 01 = Minor, 02 = regular)
        { "health_potion", "0003EADE" },       // RestoreHealth02 — Potion of Healing
        { "health_potion_minor", "0003EADD" }, // RestoreHealth01 (was 0003EADE, dup)
        { "magicka_potion", "0003EAE1" },      // RestoreMagicka02 — Potion of Magicka
        { "stamina_potion", "00039BE8" },      // RestoreStamina02 (was 0003EAE0 = Minor Magicka)

        // Ingredients (commonly used)
        { "blue_mountain_flower", "00077E1C" }, // MountainFlower01Blue (was 0002E4F4 = Greater Soul Gem)
        { "wheat", "0004B0BA" },                // Wheat (was 0006C5F4 = nonexistent)
        { "salt_pile", "00034CDF" },            // SaltPile

        // Soul gems (EMPTY variants; the block was shifted to filled IDs)
        { "petty_soul_gem", "0002E4E2" },   // SoulGemPetty   (was 0002E4F4 = Greater)
        { "lesser_soul_gem", "0002E4E4" },  // SoulGemLesser  (was 0002E4FB = GreaterFilled)
        { "common_soul_gem", "0002E4E6" },  // SoulGemCommon  (was 0002E4FC = Grand)
        { "greater_soul_gem", "0002E4F4" }, // SoulGemGreater (was 0002E4FD)
        { "grand_soul_gem", "0002E4FC" },   // SoulGemGrand   (was 0002E4FF = GrandFilled)
        { "black_soul_gem", "0002E500" },   // SoulGemBlack   (already correct)

        // Crafting materials
        { "leather", "000DB5D2" },       // Leather01
        { "leather_strips", "000800E4" },// LeatherStrips
        { "iron_ingot", "0005ACE4" },    // IngotIron
        { "steel_ingot", "0005ACE5" },   // IngotSteel
        { "gold_ingot", "0005AD9E" },    // IngotGold
    };

    /// <summary>
    /// Known cell/location IDs for teleportation
    /// </summary>
    public static readonly Dictionary<string, string> Locations = new()
    {
        // Major cities
        { "whiterun", "whiterun" },
        { "solitude", "solitude" },
        { "windhelm", "windhelm" },
        { "riften", "riften" },
        { "markarth", "markarth" },
        { "winterhold", "winterhold" },
        { "dawnstar", "dawnstar" },
        { "morthal", "morthal" },
        { "falkreath", "falkreath" },

        // Towns and settlements
        { "riverwood", "riverwood" },
        { "rorikstead", "rorikstead" },
        { "ivarstead", "ivarstead" },
        { "dragonbridge", "dragonbridge" },
        { "karthwasten", "karthwasten" },

        // Dungeons and locations (values are `coc` cell EditorIDs; houseCARL-verified 2026-06-13)
        { "bleak_falls_barrow", "BleakFallsBarrow01" },
        { "helgen", "helgen" },
        { "high_hrothgar", "HighHrothgar" },          // was "highhrotgar" (misspelled, no such cell)
        { "skyhaven_temple", "SkyHavenTemple" },      // was "skyhaventemple01" (no such cell)
        { "throat_of_the_world", "ThroatoftheWorld01" }, // was "throatoftheworldpartb" (no such cell)

        // Player homes
        { "breezehome", "WhiterunBreezehome" },
        { "honeyside", "RiftenHoneyside" },
        { "proudspire_manor", "SolitudeProudspireManor" },
        { "vlindrel_hall", "MarkarthVlindrelHall" },
        { "hjerim", "WindhelmHjerim" },               // was "windhelm" (city exterior, not the house)
    };

    /// <summary>
    /// Try to resolve item name to FormID
    /// </summary>
    public static string? GetItemFormId(string itemName)
    {
        var key = itemName.ToLower().Replace(" ", "_");
        return Items.TryGetValue(key, out var formId) ? formId : null;
    }

    /// <summary>
    /// Try to resolve location name to cell ID
    /// </summary>
    public static string? GetLocationCellId(string locationName)
    {
        var key = locationName.ToLower().Replace(" ", "_");
        return Locations.TryGetValue(key, out var cellId) ? cellId : null;
    }

    /// <summary>
    /// Get all known item names
    /// </summary>
    public static IEnumerable<string> GetAllItems() => Items.Keys;

    /// <summary>
    /// Get all known location names
    /// </summary>
    public static IEnumerable<string> GetAllLocations() => Locations.Keys;
}
