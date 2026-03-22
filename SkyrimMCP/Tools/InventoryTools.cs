using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for inventory and item operations — get inventory, add/remove items, equip, bulk operations
/// </summary>
[McpServerToolType]
public class InventoryTools : ToolBase
{
    public InventoryTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("Get the player's inventory. Supports paging: page (default 1), pageSize (default 50). " +
        "Set page=0 for summary only. Includes item names, counts, types, weights, values, display names, enchantments.")]
    public async Task<object> GetInventory(int page = 1, int pageSize = 50)
    {
        var data = await _pipe.SendRequestAsync("get_inventory");
        return PageResponse(data, "items", page, pageSize);
    }

    [McpServerTool]
    [Description("Get all currently equipped items including armor, weapons, jewelry, and shield. " +
        "Shows which slot each item occupies (head, body, hands, feet, leftHand, rightHand, etc.).")]
    public async Task<object> GetEquippedItems()
    {
        var data = await _pipe.SendRequestAsync("get_equipped_items");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Add an item to player inventory by name or FormID. " +
        "CAUTION: Invalid FormIDs that don't exist in the current load order will crash the game. " +
        "Use ListKnownItems for safe FormIDs, or verify the FormID exists before adding.")]
    public async Task<object> AddItem(string item, int count = 1)
    {
        // Resolve friendly name to FormID
        var formId = SkyrimOffsets.GetItemFormId(item) ?? item;

        var data = await _pipe.SendRequestAsync("add_item", new JsonObject
        {
            ["formId"] = formId,
            ["count"] = count
        });

        var name = data?["name"]?.GetValue<string>() ?? item;
        await NotifyInGame($"Added {count}x {name}");
        return new { success = true, message = $"Added {count}x {name} (FormID: {formId})" };
    }

    [McpServerTool]
    [Description("Remove an item from player inventory by name or FormID. " +
        "CAUTION: Removing quest items can break quest progression and may be irreversible. " +
        "Confirm with the user before removing items you're not sure about.")]
    public async Task<object> RemoveItem(string item, int count = 1)
    {
        var formId = SkyrimOffsets.GetItemFormId(item) ?? item;

        var data = await _pipe.SendRequestAsync("remove_item", new JsonObject
        {
            ["formId"] = formId,
            ["count"] = count
        });

        var name = data?["name"]?.GetValue<string>() ?? item;
        await NotifyInGame($"Removed {count}x {name}");
        return new { success = true, message = $"Removed {count}x {name} (FormID: {formId})" };
    }

    [McpServerTool]
    [Description("Add multiple items to player inventory in a single call. " +
        "Pass a JSON array of items, each with 'formId' (or 'name') and optional 'count' (default 1). " +
        "Example: [{\"formId\":\"0000000F\",\"count\":1000},{\"name\":\"iron_sword\",\"count\":5}] " +
        "Items that fail are skipped and reported in the errors list. " +
        "Use this for bulk operations like applying a character blueprint's inventory.")]
    public async Task<object> BulkAddItems(string items)
    {
        List<object> results = new();
        List<object> errors = new();
        int successCount = 0;

        try
        {
            var itemArray = JsonNode.Parse(items)?.AsArray();
            if (itemArray == null) return new { error = "Invalid JSON array" };

            foreach (var item in itemArray)
            {
                try
                {
                    var formId = item?["formId"]?.GetValue<string>();
                    var name = item?["name"]?.GetValue<string>();
                    var count = item?["count"]?.GetValue<int>() ?? 1;

                    var resolvedId = formId ?? SkyrimOffsets.GetItemFormId(name ?? "") ?? name;
                    if (string.IsNullOrEmpty(resolvedId))
                    {
                        errors.Add(new { item = name ?? formId, error = "Could not resolve FormID" });
                        continue;
                    }

                    await _pipe.SendRequestAsync("add_item", new JsonObject
                    {
                        ["formId"] = resolvedId,
                        ["count"] = count
                    });
                    successCount++;
                }
                catch (Exception ex)
                {
                    errors.Add(new { item = item?.ToJsonString(), error = ex.Message });
                }
            }
        }
        catch (Exception ex)
        {
            return new { error = $"Failed to parse items: {ex.Message}" };
        }

        await NotifyInGame($"Added {successCount} items");
        return new { success = true, added = successCount, failed = errors.Count, errors };
    }

    [McpServerTool]
    [Description("List all known item FormIDs and location names that can be used with other commands")]
    public object ListKnownItems()
    {
        return new
        {
            items = SkyrimOffsets.Items,
            locations = SkyrimOffsets.Locations,
            message = "Use these names with add_item, remove_item, and teleport commands, or provide FormIDs directly"
        };
    }
}
