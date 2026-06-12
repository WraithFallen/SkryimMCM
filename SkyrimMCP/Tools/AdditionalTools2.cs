using System.ComponentModel;
using System.Text.Json;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools added in Fix 4 — exact editor-ID lookup and paged active-quest listing.
/// Recovered 2026-06-12 (decompile + validation against the rediscovered originals — see
/// AdditionalTools.cs for the recovery story). The deployed build also carried GetScriptsOnRef
/// in this class; that copy was DELIBERATELY DROPPED here because PapyrusTools.GetScriptsOnRef
/// (commit e22a26d, Fix 5) supersedes it — same pipe action, optional refId defaulting to player,
/// and no longer needs the deployed copy's "plugin doesn't support this yet" fallback since the
/// C++ get_scripts_on_ref handler shipped with Fix 5.
/// </summary>
[McpServerToolType]
public class AdditionalTools2 : ToolBase
{
    public AdditionalTools2(IPipeClient pipe) : base(pipe) { }

    [McpServerTool(Name = "get_form_by_editor_id")]
    [Description("Look up a form by exact editor ID — equivalent to 'help X 4' in the console. Returns formId, " +
        "editorId, name, type, and source plugin. If no exact match exists, returns the closest prefix match " +
        "with matchType='prefix'. Use SearchForms for fuzzy name searches across all form types.")]
    public async Task<object> GetFormByEditorId(string editorId)
    {
        var data = await _pipe.SendRequestAsync("search_forms", new JsonObject
        {
            ["query"] = editorId,
            ["type"] = "all",
            ["maxResults"] = 10
        });

        var forms = data?["forms"]?.AsArray();
        if (forms != null)
        {
            foreach (var form in forms)
            {
                var candidate = form?["editorId"]?.GetValue<string>();
                if (candidate != null && string.Equals(candidate, editorId, StringComparison.OrdinalIgnoreCase))
                {
                    return DeserializeResponse(form);
                }
            }

            if (forms.Count > 0)
            {
                return new
                {
                    matchType = "prefix",
                    note = $"No exact match for '{editorId}'; returning closest result",
                    form = JsonSerializer.Deserialize<JsonElement>(forms[0]?.ToJsonString() ?? "{}")
                };
            }
        }

        throw new InvalidOperationException("No form found for editor ID: " + editorId);
    }

    [McpServerTool(Name = "get_all_active_quests")]
    [Description("List all active quests with paging. Returns formId, name, current stage, running/completed " +
        "status per quest. page=0 returns count summary only. Prefer this over GetQuestInfo in heavily modded " +
        "installs — GetQuestInfo returns everything unpaged.")]
    public async Task<object> GetAllActiveQuests(int page = 1, int pageSize = 50)
    {
        return PageResponse(await _pipe.SendRequestAsync("get_quest_info"), "quests", page, pageSize);
    }
}
