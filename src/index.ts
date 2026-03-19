#!/usr/bin/env node

/**
 * Skyrim MCP Server
 * Model Context Protocol server for interacting with Skyrim SE/AE
 */

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
  Tool,
} from '@modelcontextprotocol/sdk/types.js';

import { MemoryReader } from './memory-reader.js';
import { ConsoleInjector } from './console-injector.js';
import { FORM_IDS } from './skyrim-offsets.js';

// Initialize components
const memoryReader = new MemoryReader();
const consoleInjector = new ConsoleInjector();

// Track connection state
let isConnected = false;

/**
 * Define available MCP tools
 */
const TOOLS: Tool[] = [
  {
    name: 'check_skyrim_running',
    description:
      'Check if Skyrim is running and get process information. Call this first before using other tools.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
  },
  {
    name: 'execute_console_command',
    description:
      'Execute a console command in Skyrim. The game window will be activated and the command will be typed into the console automatically.',
    inputSchema: {
      type: 'object',
      properties: {
        command: {
          type: 'string',
          description: 'The console command to execute (e.g., "tgm", "player.additem 0000000f 1000")',
        },
      },
      required: ['command'],
    },
  },
  {
    name: 'add_item',
    description:
      'Add an item to player inventory. Wrapper around console command that handles FormID lookup.',
    inputSchema: {
      type: 'object',
      properties: {
        item: {
          type: 'string',
          description: 'Item name or FormID (e.g., "gold", "lockpick", "0000000F")',
        },
        count: {
          type: 'number',
          description: 'Number of items to add',
          default: 1,
        },
      },
      required: ['item'],
    },
  },
  {
    name: 'teleport',
    description: 'Teleport player to a location using COC (Center on Cell) command.',
    inputSchema: {
      type: 'object',
      properties: {
        location: {
          type: 'string',
          description: 'Location name or cell ID (e.g., "whiterun", "riverwood", "solitude")',
        },
      },
      required: ['location'],
    },
  },
  {
    name: 'toggle_god_mode',
    description: 'Toggle god mode (invincibility) for the player character.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
  },
  {
    name: 'list_known_items',
    description: 'List all known item FormIDs and locations that can be used with other commands.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
  },
];

/**
 * Handle tool execution
 */
async function handleToolCall(name: string, args: any): Promise<any> {
  switch (name) {
    case 'check_skyrim_running': {
      try {
        // Try to find window first (lighter operation)
        const windowFound = consoleInjector.findWindow();

        if (!windowFound) {
          return {
            running: false,
            message: 'Skyrim window not found. Make sure the game is running.',
          };
        }

        // Try to attach to process
        if (!memoryReader.isAttached()) {
          try {
            const processInfo = await memoryReader.attachToSkyrim();
            isConnected = true;
            return {
              running: true,
              connected: true,
              processInfo: {
                processId: processInfo.processId,
                version: processInfo.version.name,
                executable: processInfo.version.executable,
              },
              message: `Successfully connected to ${processInfo.version.name}`,
            };
          } catch (error) {
            return {
              running: true,
              connected: false,
              message: `Game is running but could not attach to process: ${
                error instanceof Error ? error.message : String(error)
              }`,
            };
          }
        }

        const processInfo = memoryReader.getProcessInfo();
        return {
          running: true,
          connected: true,
          processInfo: processInfo
            ? {
                processId: processInfo.processId,
                version: processInfo.version.name,
                executable: processInfo.version.executable,
              }
            : null,
          message: 'Already connected to Skyrim',
        };
      } catch (error) {
        return {
          running: false,
          connected: false,
          error: error instanceof Error ? error.message : String(error),
        };
      }
    }

    case 'execute_console_command': {
      const { command } = args;
      const result = await consoleInjector.executeCommand(command);

      if (result.success) {
        return {
          success: true,
          message: `Executed command: ${command}`,
          command: command,
        };
      } else {
        return {
          success: false,
          error: result.error,
          command: command,
        };
      }
    }

    case 'add_item': {
      const { item, count = 1 } = args;

      // Try to resolve item name to FormID
      let formId = item;
      const itemUpper = item.toUpperCase();

      if (FORM_IDS.ITEMS[itemUpper as keyof typeof FORM_IDS.ITEMS]) {
        formId = FORM_IDS.ITEMS[itemUpper as keyof typeof FORM_IDS.ITEMS];
      }

      const result = await consoleInjector.addItem(formId, count);

      if (result.success) {
        return {
          success: true,
          message: `Added ${count}x ${item} (FormID: ${formId})`,
        };
      } else {
        return {
          success: false,
          error: result.error,
        };
      }
    }

    case 'teleport': {
      const { location } = args;

      // Try to resolve location name
      let cellId = location.toLowerCase();
      const locationUpper = location.toUpperCase();

      if (FORM_IDS.LOCATIONS[locationUpper as keyof typeof FORM_IDS.LOCATIONS]) {
        cellId = FORM_IDS.LOCATIONS[locationUpper as keyof typeof FORM_IDS.LOCATIONS];
      }

      const result = await consoleInjector.teleportTo(cellId);

      if (result.success) {
        return {
          success: true,
          message: `Teleporting to ${location}`,
        };
      } else {
        return {
          success: false,
          error: result.error,
        };
      }
    }

    case 'toggle_god_mode': {
      const result = await consoleInjector.toggleGodMode();

      if (result.success) {
        return {
          success: true,
          message: 'Toggled god mode',
        };
      } else {
        return {
          success: false,
          error: result.error,
        };
      }
    }

    case 'list_known_items': {
      return {
        items: FORM_IDS.ITEMS,
        locations: FORM_IDS.LOCATIONS,
        message: 'Use these names with add_item and teleport commands, or provide FormIDs directly',
      };
    }

    default:
      throw new Error(`Unknown tool: ${name}`);
  }
}

/**
 * Main server setup
 */
async function main() {
  const server = new Server(
    {
      name: 'skyrim-mcp',
      version: '0.1.0',
    },
    {
      capabilities: {
        tools: {},
      },
    }
  );

  // Handle tool listing
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    return { tools: TOOLS };
  });

  // Handle tool execution
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;

    try {
      const result = await handleToolCall(name, args || {});

      return {
        content: [
          {
            type: 'text',
            text: JSON.stringify(result, null, 2),
          },
        ],
      };
    } catch (error) {
      return {
        content: [
          {
            type: 'text',
            text: JSON.stringify(
              {
                error: error instanceof Error ? error.message : String(error),
              },
              null,
              2
            ),
          },
        ],
        isError: true,
      };
    }
  });

  // Start server with stdio transport
  const transport = new StdioServerTransport();
  await server.connect(transport);

  // Log to stderr (stdout is used for MCP protocol)
  console.error('Skyrim MCP Server running on stdio');
  console.error('Available tools:', TOOLS.map((t) => t.name).join(', '));
}

// Run server
main().catch((error) => {
  console.error('Fatal error:', error);
  process.exit(1);
});
