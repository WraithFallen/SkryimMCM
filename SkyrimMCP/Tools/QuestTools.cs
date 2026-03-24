using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for quest operations — quest info, stages, aliases, quest items
/// </summary>
[McpServerToolType]
public class QuestTools : ToolBase
{
    public QuestTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("Get information about active quests including objectives and completion status. " +
        "Note: In heavily modded games this may return a large number of quests. " +
        "For a specific quest, use ExecuteConsoleCommand with 'getqueststage <questID>' instead.")]
    public async Task<object> GetQuestInfo()
    {
        var data = await _pipe.SendRequestAsync("get_quest_info");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("List all stages of a specific quest — executed stages, waiting stages, and objectives with their state. " +
        "Use this to see quest progression history and what stages have been completed. " +
        "Get the quest FormID from SearchForms or GetQuestInfo first.")]
    public async Task<object> GetQuestStages(string formId)
    {
        var data = await _pipe.SendRequestAsync("get_quest_stages", new JsonObject { ["formId"] = formId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all aliases for a quest — resolves to actual NPCs/objects with their reference IDs. " +
        "Shows if each alias is essential, protected, quest object, alive/dead. " +
        "Use this to find which NPCs are involved in a quest and their current state.")]
    public async Task<object> GetQuestAliases(string formId)
    {
        var data = await _pipe.SendRequestAsync("get_quest_aliases", new JsonObject { ["formId"] = formId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("List all quest items in the player's inventory — items flagged as quest objects that cannot normally be dropped. " +
        "Shows which items are tied to active quests.")]
    public async Task<object> GetQuestItems()
    {
        var data = await _pipe.SendRequestAsync("get_quest_items");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Read quest variables and running status via console commands. " +
        "Pass a quest FormID to check if it's running and get its current stage. " +
        "Uses getqueststage and isrunning console commands with output capture.")]
    public async Task<object> ReadQuestVariables(string questFormId)
    {
        var stage = await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = $"getqueststage {questFormId}"
        });

        var running = await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = $"isrunning {questFormId}"
        });

        return new
        {
            questFormId,
            stageOutput = stage?["output"]?.GetValue<string>() ?? "no output",
            runningOutput = running?["output"]?.GetValue<string>() ?? "no output"
        };
    }
}
