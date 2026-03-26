# Skyrim MCP — Release Build Script
# Run from Developer PowerShell for VS 2022
# Produces a ready-to-zip release folder

param(
    [string]$Version = "1.0.0",
    [string]$OutputDir = "release",
    [string]$VcpkgRoot = "C:\vcpkg"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Building Skyrim MCP v$Version ===" -ForegroundColor Cyan

# Set vcpkg
$env:VCPKG_ROOT = $VcpkgRoot

# Clean output
if (Test-Path $OutputDir) { Remove-Item -Recurse -Force $OutputDir }
New-Item -ItemType Directory -Path "$OutputDir\Data\SKSE\Plugins\SkyrimMCP_Server" -Force | Out-Null

# Build C++ plugin
Write-Host "`n--- Building SKSE Plugin (C++) ---" -ForegroundColor Yellow
Push-Location SkyrimMCPPlugin
if (-not (Test-Path "build")) {
    cmake --preset default
}
cmake --build build --config Release
Pop-Location

# Build C# MCP Server
Write-Host "`n--- Building MCP Server (C#) ---" -ForegroundColor Yellow
dotnet publish SkyrimMCP -c Release -o "$OutputDir\Data\SKSE\Plugins\SkyrimMCP_Server" --no-self-contained

# Copy SKSE plugin DLL
Write-Host "`n--- Packaging ---" -ForegroundColor Yellow
Copy-Item "SkyrimMCPPlugin\output\SkyrimMCPPlugin.dll" "$OutputDir\Data\SKSE\Plugins\" -Force

# Create example config
$config = @{
    mcpServers = @{
        skyrim = @{
            command = "dotnet"
            args = @("<SKYRIM_PATH>\Data\SKSE\Plugins\SkyrimMCP_Server\SkyrimMCP.dll")
        }
    }
}
$config | ConvertTo-Json -Depth 4 | Set-Content "$OutputDir\claude_desktop_config_example.json"

# Create README
@"
# Skyrim MCP Server v$Version

An MCP server that lets AI assistants (Claude, etc.) interact directly
with Skyrim SE/AE's runtime engine via an SKSE plugin.

## Requirements
- Skyrim SE or AE
- SKSE64 (https://skse.silverlock.org/)
- Address Library for SKSE (https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- .NET 10 Runtime (https://dotnet.microsoft.com/download/dotnet/10.0)
- Claude Desktop (https://claude.ai/desktop)

## Installation
1. Extract the Data folder into your Skyrim directory
   (or install via Mod Organizer 2 / Vortex)
2. Install .NET 10 Runtime if not already installed
3. Configure Claude Desktop (see below)

## Claude Desktop Configuration
Edit %APPDATA%\Claude\claude_desktop_config.json:

{
  "mcpServers": {
    "skyrim": {
      "command": "dotnet",
      "args": ["<SKYRIM_PATH>\\Data\\SKSE\\Plugins\\SkyrimMCP_Server\\SkyrimMCP.dll"]
    }
  }
}

Replace <SKYRIM_PATH> with your Skyrim installation path.

## Usage
1. Launch Skyrim via SKSE
2. Open Claude Desktop
3. Ask Claude to interact with your game!

## Features
- 74 MCP tools for reading and modifying game state
- Player stats, inventory, spells, perks, shouts, appearance
- Quest management, NPC interaction, follower management
- Weather control, world objects, containers, locks
- Combat analysis, threat assessment, damage stats
- Event system (combat, death, quest updates, location changes)
- Character blueprint export/import
- Papyrus VM bridge (call any mod's Papyrus functions)
- Form search engine (replaces console 'help' command)

## Credits
- ConsoleUtilSSE approach by VersuchDrei (MIT License)
- CommonLibSSE-NG by CharmedBaryon
- SKSE by Ian Patterson, Stephen Abel, and Paul Shortman
"@ | Set-Content "$OutputDir\README.txt"

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "Release folder: $OutputDir\"
Write-Host "  SKSE Plugin: $OutputDir\Data\SKSE\Plugins\SkyrimMCPPlugin.dll"
Write-Host "  MCP Server:  $OutputDir\Data\SKSE\Plugins\SkyrimMCP_Server\"
Write-Host "`nZip the '$OutputDir' folder for Nexus upload."
