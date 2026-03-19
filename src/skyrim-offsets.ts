/**
 * Memory offsets for different Skyrim versions
 *
 * These offsets are memory addresses that point to important game data.
 * They differ between Skyrim SE (1.5.97) and AE (1.6.x) versions.
 *
 * Format: Arrays represent pointer chains to follow
 * Example: [0x12345, 0x10, 0x20] means: read pointer at baseAddress + 0x12345,
 *          then read pointer at that address + 0x10, then read value at that + 0x20
 *
 * NOTE: These are placeholder values and need to be found using tools like
 * Cheat Engine, ReClass.NET, or by referencing SKSE source code.
 */

import { GameVersionOffsets } from './types.js';

export const OFFSETS: GameVersionOffsets = {
  SE_1_5_97: {
    // Skyrim SE 1.5.97 offsets (most stable modding version)
    // TODO: These need to be researched and filled in with actual values
    playerBase: [0x2F26A78], // Example placeholder - needs real offset
    playerLevel: [0x2F26A78, 0x78, 0xB4],
    playerHealth: [0x2F26A78, 0x78, 0x260],
    playerMagicka: [0x2F26A78, 0x78, 0x268],
    playerStamina: [0x2F26A78, 0x78, 0x270],
    playerPosition: [0x2F26A78, 0x78, 0x54],
  },
  AE_1_6_X: {
    // Skyrim AE 1.6.x offsets (Anniversary Edition)
    // TODO: These need to be researched and filled in with actual values
    playerBase: [0x2F6B948], // Example placeholder - needs real offset
    playerLevel: [0x2F6B948, 0x78, 0xB4],
    playerHealth: [0x2F6B948, 0x78, 0x260],
    playerMagicka: [0x2F6B948, 0x78, 0x268],
    playerStamina: [0x2F6B948, 0x78, 0x270],
    playerPosition: [0x2F6B948, 0x78, 0x54],
  },
};

/**
 * Known FormIDs for common items, NPCs, and locations
 * These are consistent across all game versions
 */
export const FORM_IDS = {
  ITEMS: {
    GOLD: '0000000F',
    LOCKPICK: '0000000A',
    IRON_SWORD: '00012EB7',
    HEALTH_POTION: '0003EADE',
  },
  LOCATIONS: {
    WHITERUN: 'whiterun',
    RIVERWOOD: 'riverwood',
    SOLITUDE: 'solitude',
    WINDHELM: 'windhelm',
  },
  // Add more as needed
};

/**
 * Helper to get offsets for detected game version
 */
export function getOffsetsForVersion(version: 'SE_1_5_97' | 'AE_1_6_X') {
  return OFFSETS[version];
}
