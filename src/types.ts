/**
 * Type definitions for Skyrim MCP Server
 */

export interface SkyrimVersion {
  name: string;
  executable: string;
  version: string;
}

export interface PlayerStatus {
  level: number;
  health: { current: number; max: number };
  magicka: { current: number; max: number };
  stamina: { current: number; max: number };
  carryWeight: { current: number; max: number };
  gold: number;
}

export interface PlayerLocation {
  cellName: string;
  worldSpace?: string;
  coordinates: {
    x: number;
    y: number;
    z: number;
  };
}

export interface MemoryOffsets {
  // Base offsets for different game versions
  playerBase: number[];
  playerLevel?: number[];
  playerHealth?: number[];
  playerMagicka?: number[];
  playerStamina?: number[];
  playerPosition?: number[];
  // Add more as we discover them
}

export interface GameVersionOffsets {
  SE_1_5_97: MemoryOffsets;
  AE_1_6_X: MemoryOffsets;
}

export interface ProcessInfo {
  processId: number;
  handle: number;
  baseAddress: number;
  version: SkyrimVersion;
}

export interface ConsoleCommandResult {
  success: boolean;
  command: string;
  error?: string;
}
