using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools added in Fix 3 — batch console execution, quest stage setting, crash log discovery, save listing.
/// Recovered 2026-06-12 by decompiling the deployed SkyrimMCP.dll (built 2026-05-22), then validated
/// line-for-line against the rediscovered originals (the May-22 build came from a decompiled-monolith
/// side project that pre-dated adopting this fork repo; this commit integrates the tools properly).
/// Tool names are pinned with explicit Name attributes so the MCP SDK's PascalCase splitter can
/// never change them (the get_nearby_np_cs artifact class).
/// </summary>
[McpServerToolType]
public class AdditionalTools : ToolBase
{
    public AdditionalTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool(Name = "batch_console_commands")]
    [Description("Execute a list of console commands in sequence. Returns per-command results including " +
        "attempted/succeeded/failed counts. Use for bulk operations like applying multiple setstage calls, " +
        "toggling multiple settings, or any sequence of console commands that must run together.")]
    public async Task<object> BatchConsoleCommands(string[] commands)
    {
        if (commands == null || commands.Length == 0)
        {
            return new { attempted = 0, succeeded = 0, failed = 0, results = Array.Empty<object>() };
        }

        var results = new List<object>();
        int succeeded = 0, failed = 0;
        foreach (var cmd in commands)
        {
            try
            {
                var data = await _pipe.SendRequestAsync("execute_command", new JsonObject { ["command"] = cmd });
                results.Add(new { command = cmd, result = DeserializeResponse(data) });
                succeeded++;
            }
            catch (Exception ex)
            {
                results.Add(new { command = cmd, error = ex.Message });
                failed++;
            }
        }

        return new { attempted = commands.Length, succeeded, failed, results };
    }

    [McpServerTool(Name = "set_quest_stage")]
    [Description("Set a quest to a specific stage using the setstage console command. questFormId is the hex " +
        "FormID (e.g. '0004B2D9') or plugin-relative ID. Use GetQuestStages first to see valid stage numbers. " +
        "CAUTION: advancing quest stages bypasses quest logic — use only to fix stuck quests.")]
    public async Task<object> SetQuestStage(string questFormId, int stage)
    {
        var command = $"setstage {questFormId} {stage}";
        var data = await _pipe.SendRequestAsync("execute_command", new JsonObject { ["command"] = command });
        await NotifyInGame($"Quest stage set: {questFormId} -> {stage}");
        return new { success = true, command, result = DeserializeResponse(data) };
    }

    // MO2 instance root derived from SKYRIM_PATH (the game root the file tools already use —
    // e.g. "C:\Games\Authoria 1.5\Stock Game" -> "C:\Games\Authoria 1.5"). Audit 43: the old
    // hardcoded Apostasy paths made list_saves/check_crash_logs silently miss the ACTIVE
    // modlist after every migration (list_saves was returning year-old Documents saves).
    private static string? Mo2Root()
    {
        var sp = Environment.GetEnvironmentVariable("SKYRIM_PATH");
        if (string.IsNullOrWhiteSpace(sp)) return null;
        // TrimEnd: a trailing slash makes GetDirectoryName return the game root ITSELF (not its
        // parent), which would silently skip the profiles scan. Non-MO2 setups (Steam root) are
        // fine either way — profiles\ won't exist and the caller's Directory.Exists guard skips.
        try { return Path.GetDirectoryName(Path.GetFullPath(sp.TrimEnd('\\', '/'))); } catch { return null; }
    }

    [McpServerTool(Name = "check_crash_logs")]
    [Description("Scan crash log directories and return the most recent crash log files. Checks both the " +
        "CrashLoggerSSE output (crash-*.log) and the Tullius CTD Logger output (under the MO2 instance's " +
        "overwrite, derived from SKYRIM_PATH). Returns file names, paths, sizes, and last-modified " +
        "timestamps sorted newest-first. Use this to quickly see if a recent crash occurred and locate " +
        "the log files to read.")]
    public Task<object> CheckCrashLogs(int count = 5)
    {
        count = Math.Clamp(count, 1, 100);
        var skseDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal),
            "My Games", "Skyrim Special Edition", "SKSE");
        var tulliusDir = Mo2Root() is string mo2Root
            ? Path.Combine(mo2Root, "overwrite", "SKSE", "Plugins", "Tullius Ctd Logs")
            : @"C:\Games\Apostasy\overwrite\SKSE\Plugins\Tullius Ctd Logs";

        var logs = new List<object>();
        var scanErrors = new List<string>();

        CollectFiles(skseDir, "crash-*.log", "SKSE dir", logs, scanErrors);
        CollectFiles(tulliusDir, "*.log", "Tullius dir", logs, scanErrors);

        if (logs.Count == 0 && scanErrors.Count == 0)
        {
            return Task.FromResult((object)new { error = "No crash log files found" });
        }

        var newest = logs.OrderByDescending(f => ((dynamic)f).lastModified).Take(count).ToArray();
        var response = new Dictionary<string, object>
        {
            ["count"] = newest.Length,
            ["logs"] = newest
        };
        if (scanErrors.Count > 0) response["scanErrors"] = scanErrors.ToArray();
        return Task.FromResult((object)response);
    }

    [McpServerTool(Name = "list_saves")]
    [Description("List Skyrim save files sorted newest-first. Returns save name, full path, size, and " +
        "last-modified time. Skyrim SE save filenames encode the character name and location " +
        "(e.g. Save5_Dragonborn_Whiterun_...). Checks the standard Documents save directory plus every " +
        "MO2 profile's local saves under the active instance (derived from SKYRIM_PATH). Use this to " +
        "find the most recent save, verify autosaves are being created, or locate a save to load.")]
    public Task<object> ListSaves(int count = 20)
    {
        count = Math.Clamp(count, 1, 200);
        var saveDirs = new List<string>
        {
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal),
                "My Games", "Skyrim Special Edition", "Saves"),
        };
        // MO2 profile-local saves: every profiles\*\saves under the instance root (audit 43 —
        // the old hardcoded Apostasy profile path missed the active playthrough entirely).
        if (Mo2Root() is string mo2)
        {
            var profiles = Path.Combine(mo2, "profiles");
            if (Directory.Exists(profiles))
            {
                foreach (var p in Directory.GetDirectories(profiles))
                {
                    var s = Path.Combine(p, "saves");
                    if (Directory.Exists(s)) saveDirs.Add(s);
                }
            }
        }

        var saves = new List<object>();
        var scanErrors = new List<string>();
        foreach (var dir in saveDirs)
        {
            CollectFiles(dir, "*.ess", dir, saves, scanErrors);
        }

        if (saves.Count == 0 && scanErrors.Count == 0)
        {
            return Task.FromResult((object)new { error = "No save files found in: " + string.Join(", ", saveDirs) });
        }

        var newest = saves.OrderByDescending(f => ((dynamic)f).lastModified).Take(count).ToArray();
        var response = new Dictionary<string, object>
        {
            ["count"] = newest.Length,
            ["saves"] = newest
        };
        if (scanErrors.Count > 0) response["scanErrors"] = scanErrors.ToArray();
        return Task.FromResult((object)response);
    }

    /// <summary>
    /// Enumerate files matching a pattern in a directory into an anonymous-object list.
    /// Per-file stat failures are skipped silently; directory-level failures are recorded in scanErrors.
    /// </summary>
    private static void CollectFiles(string dir, string pattern, string errorLabel,
        List<object> results, List<string> scanErrors)
    {
        if (!Directory.Exists(dir)) return;
        try
        {
            foreach (var fi in new DirectoryInfo(dir).GetFiles(pattern))
            {
                try
                {
                    results.Add(new
                    {
                        name = fi.Name,
                        path = fi.FullName,
                        sizeBytes = fi.Length,
                        lastModified = fi.LastWriteTime.ToString("o")
                    });
                }
                catch { /* file vanished mid-scan — skip */ }
            }
        }
        catch (Exception ex)
        {
            scanErrors.Add($"{errorLabel}: {ex.Message}");
        }
    }
}
