using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for Papyrus VM interaction — discover and call any Papyrus function from any installed mod
/// </summary>
[McpServerToolType]
public class PapyrusTools : ToolBase
{
    public PapyrusTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("Get the full Papyrus function catalog — all native functions from all installed mods. " +
        "Scans .psc source files on first call (takes 2-3 seconds), caches for subsequent calls. " +
        "Returns scripts organized with function names, parameter types, return types, and global flags. " +
        "Use this to discover what functions are available from installed mods.")]
    public async Task<object> GetPapyrusCatalog()
    {
        var data = await _pipe.SendRequestAsync("get_papyrus_catalog");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all Papyrus scripts currently attached to a reference at runtime. " +
        "Walks the Papyrus VM's live object table — shows what script classes are actually bound and running on the ref. " +
        "Pass refId as a hex FormID (e.g. '14' for player) or 'player'. " +
        "Omit refId to query the player. Returns an array of script class names.")]
    public async Task<object> GetScriptsOnRef(string? refId = null)
    {
        var effectiveRefId = string.IsNullOrEmpty(refId) ? "player" : refId;
        var data = await _pipe.SendRequestAsync("get_scripts_on_ref", new JsonObject { ["refId"] = effectiveRefId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Force rescan of Papyrus source files. Use after installing new mods to update the catalog.")]
    public async Task<object> ScanPapyrusSources()
    {
        var data = await _pipe.SendRequestAsync("scan_papyrus_sources");
        await NotifyInGame("Papyrus catalog updated");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all available functions for a specific Papyrus script class (e.g., 'ConsoleUtil', 'PapyrusUtil', 'JArray'). " +
        "Shows function names, parameter types, return types. Use GetPapyrusCatalog first to see available script classes.")]
    public async Task<object> GetScriptFunctions(string className)
    {
        var data = await _pipe.SendRequestAsync("get_script_functions", new JsonObject { ["className"] = className });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Call any Papyrus function from any installed mod via the Papyrus VM. " +
        "Pass className (script class), functionName, and args (JSON array of values). " +
        "Supports string, int, float, bool arguments. Returns the function's return value. " +
        "WORKFLOW: Use GetScriptFunctions(className) first to see available functions and parameter types. " +
        "KEY PLUGINS AVAILABLE: " +
        "ConsoleUtil (6 funcs): ExecuteCommand, ReadMessage, GetVersion, SetSelectedReference. " +
        "PapyrusUtil (21 funcs): MorphManager for face/body morphs, array utilities, file I/O. " +
        "StorageUtil (171 funcs): Persistent key-value storage per form — SetIntValue, GetStringValue, etc. " +
        "JsonUtil (123 funcs): Read/write JSON files — Load, Save, SetStringValue, GetIntValue. " +
        "JContainers (431 funcs): Advanced data structures — JArray, JMap, JValue, JDB for persistent databases. " +
        "Experience (13 funcs): AddExperience, GetSkillCap, ShowNotification for XP-based leveling. " +
        "OActor (40 funcs): PlayExpression, Undress/Redress, EquipObject for character interaction. " +
        "IED (112 funcs): Equipment display management — show weapons/shields on character model. " +
        "DbSkseFunctions (159 funcs): Extended utilities — clipboard, save management, form queries. " +
        "Input (8 funcs): IsKeyPressed, TapKey, HoldKey for input simulation. " +
        "Camera (6 funcs): GetCameraState, SetWorldFieldOfView for camera control. " +
        "EXAMPLES: " +
        "Get ConsoleUtil version: className='ConsoleUtil', functionName='GetVersion', args=[] " +
        "Execute console command: className='ConsoleUtil', functionName='ExecuteCommand', args=['tgm'] " +
        "Add experience: className='Experience', functionName='AddExperience', args=[100, true] " +
        "CAUTION: Functions requiring Actor/Form type arguments need FormID resolution (not yet supported for all types).")]
    public async Task<object> CallPapyrusFunction(string className, string functionName, string? args = null)
    {
        var parms = new JsonObject
        {
            ["className"] = className,
            ["functionName"] = functionName
        };

        if (!string.IsNullOrEmpty(args))
        {
            try
            {
                parms["args"] = JsonNode.Parse(args);
            }
            catch
            {
                // Treat as single string argument
                var arr = new JsonArray();
                arr.Add(args);
                parms["args"] = arr;
            }
        }
        else
        {
            parms["args"] = new JsonArray();
        }

        var data = await _pipe.SendRequestAsync("call_papyrus_function", parms);
        return DeserializeResponse(data);
    }
}
