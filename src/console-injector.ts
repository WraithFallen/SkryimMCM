/**
 * Console command injection for Skyrim
 * Simulates keyboard input to execute console commands
 */

import ffi from 'ffi-napi';
import ref from 'ref-napi';
import { ConsoleCommandResult } from './types.js';

// Windows API constants for keyboard input
const INPUT_KEYBOARD = 1;
const KEYEVENTF_KEYUP = 0x0002;
const VK_TILDE = 0xC0; // ~ key for console

// Windows API bindings
const user32 = ffi.Library('user32', {
  FindWindowA: ['int32', ['string', 'string']],
  SetForegroundWindow: ['bool', ['int32']],
  SendInput: ['uint32', ['uint32', 'pointer', 'int32']],
});

/**
 * Sleep for specified milliseconds
 */
function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export class ConsoleInjector {
  private windowHandle: number | null = null;

  /**
   * Find Skyrim window
   */
  findWindow(): boolean {
    // Try different window class names for different Skyrim versions
    const windowClasses = ['Skyrim Special Edition', 'Skyrim', 'SkyrimSE'];

    for (const className of windowClasses) {
      const handle = user32.FindWindowA(className, null);
      if (handle !== 0) {
        this.windowHandle = handle;
        return true;
      }
    }

    return false;
  }

  /**
   * Bring Skyrim window to foreground
   */
  private async activateWindow(): Promise<void> {
    if (!this.windowHandle) {
      throw new Error('Window not found. Call findWindow() first.');
    }

    user32.SetForegroundWindow(this.windowHandle);
    await sleep(100); // Give window time to activate
  }

  /**
   * Send a single key press
   */
  private async sendKey(virtualKeyCode: number): Promise<void> {
    // Create INPUT structure for key down
    const inputSize = 28; // Size of INPUT structure
    const inputDown = Buffer.alloc(inputSize);
    inputDown.writeUInt32LE(INPUT_KEYBOARD, 0); // type
    inputDown.writeUInt16LE(virtualKeyCode, 4); // wVk (virtual key code)
    inputDown.writeUInt16LE(0, 6); // wScan
    inputDown.writeUInt32LE(0, 8); // dwFlags (0 = key down)

    // Create INPUT structure for key up
    const inputUp = Buffer.alloc(inputSize);
    inputUp.writeUInt32LE(INPUT_KEYBOARD, 0); // type
    inputUp.writeUInt16LE(virtualKeyCode, 4); // wVk
    inputUp.writeUInt16LE(0, 6); // wScan
    inputUp.writeUInt32LE(KEYEVENTF_KEYUP, 8); // dwFlags

    // Send key down
    user32.SendInput(1, inputDown, inputSize);
    await sleep(20);

    // Send key up
    user32.SendInput(1, inputUp, inputSize);
    await sleep(20);
  }

  /**
   * Send text as keyboard input
   */
  private async sendText(text: string): Promise<void> {
    for (const char of text) {
      const keyCode = char.toUpperCase().charCodeAt(0);
      await this.sendKey(keyCode);
    }
  }

  /**
   * Open console in Skyrim
   */
  private async openConsole(): Promise<void> {
    await this.sendKey(VK_TILDE);
    await sleep(200); // Give console time to open
  }

  /**
   * Press Enter key
   */
  private async pressEnter(): Promise<void> {
    await this.sendKey(0x0D); // VK_RETURN
    await sleep(100);
  }

  /**
   * Execute a console command in Skyrim
   */
  async executeCommand(command: string): Promise<ConsoleCommandResult> {
    try {
      // Find window if not already found
      if (!this.windowHandle && !this.findWindow()) {
        return {
          success: false,
          command: command,
          error: 'Skyrim window not found. Is the game running?',
        };
      }

      // Activate window
      await this.activateWindow();

      // Open console
      await this.openConsole();

      // Type command
      await this.sendText(command);
      await sleep(100);

      // Press Enter to execute
      await this.pressEnter();

      // Close console (press ~ again)
      await this.openConsole();

      return {
        success: true,
        command: command,
      };
    } catch (error) {
      return {
        success: false,
        command: command,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Execute multiple commands in sequence
   */
  async executeCommands(commands: string[]): Promise<ConsoleCommandResult[]> {
    const results: ConsoleCommandResult[] = [];

    for (const command of commands) {
      const result = await this.executeCommand(command);
      results.push(result);

      // Add delay between commands
      await sleep(500);
    }

    return results;
  }

  /**
   * Common helper commands
   */
  async toggleGodMode(): Promise<ConsoleCommandResult> {
    return this.executeCommand('tgm');
  }

  async addGold(amount: number): Promise<ConsoleCommandResult> {
    return this.executeCommand(`player.additem 0000000f ${amount}`);
  }

  async teleportTo(location: string): Promise<ConsoleCommandResult> {
    return this.executeCommand(`coc ${location}`);
  }

  async setLevel(level: number): Promise<ConsoleCommandResult> {
    return this.executeCommand(`player.setlevel ${level}`);
  }

  async addItem(formId: string, count: number = 1): Promise<ConsoleCommandResult> {
    return this.executeCommand(`player.additem ${formId} ${count}`);
  }
}
