/**
 * Memory reading functionality for Skyrim process
 * Uses Windows API via ffi-napi to read process memory
 */

import ffi from 'ffi-napi';
import ref from 'ref-napi';
import { ProcessInfo } from './types.js';

// Windows API constants
const PROCESS_VM_READ = 0x0010;
const PROCESS_VM_WRITE = 0x0020;
const PROCESS_VM_OPERATION = 0x0008;
const PROCESS_QUERY_INFORMATION = 0x0400;
const TH32CS_SNAPPROCESS = 0x00000002;

// Type definitions for Windows API
const DWORD = ref.types.uint32;
const HANDLE = ref.types.void;
const SIZE_T = ref.types.size_t;

// Windows API bindings
const kernel32 = ffi.Library('kernel32', {
  OpenProcess: [HANDLE, [DWORD, 'bool', DWORD]],
  ReadProcessMemory: ['bool', [HANDLE, 'uint64', 'pointer', SIZE_T, 'pointer']],
  WriteProcessMemory: ['bool', [HANDLE, 'uint64', 'pointer', SIZE_T, 'pointer']],
  CloseHandle: ['bool', [HANDLE]],
  CreateToolhelp32Snapshot: [HANDLE, [DWORD, DWORD]],
  Process32First: ['bool', [HANDLE, 'pointer']],
  Process32Next: ['bool', [HANDLE, 'pointer']],
});

export class MemoryReader {
  private processHandle: any = null;
  private processInfo: ProcessInfo | null = null;

  /**
   * Find and attach to Skyrim process
   */
  async attachToSkyrim(): Promise<ProcessInfo> {
    const processNames = ['SkyrimSE.exe', 'SkyrimAE.exe', 'Skyrim.exe'];

    for (const name of processNames) {
      const pid = this.findProcessByName(name);
      if (pid) {
        const handle = kernel32.OpenProcess(
          PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
          false,
          pid
        );

        if (handle.isNull()) {
          throw new Error(`Failed to open process ${name} (PID: ${pid}). Run as administrator?`);
        }

        this.processHandle = handle;

        // Detect version based on executable name
        const version = this.detectVersion(name);

        // Get base address (simplified - would need more complex logic in reality)
        const baseAddress = 0x140000000; // Typical base for 64-bit executables

        this.processInfo = {
          processId: pid,
          handle: handle,
          baseAddress: baseAddress,
          version: version,
        };

        return this.processInfo;
      }
    }

    throw new Error('Skyrim process not found. Make sure the game is running.');
  }

  /**
   * Find process ID by executable name
   */
  private findProcessByName(processName: string): number | null {
    // Note: This is a simplified version. A complete implementation would use
    // CreateToolhelp32Snapshot and Process32First/Next to enumerate processes.
    // For now, we'll return null and require manual PID input or use a Node package
    // like 'node-process-list' for cross-platform process enumeration.

    // TODO: Implement proper process enumeration
    console.warn('Process enumeration not fully implemented. Consider using node-process-list package.');
    return null;
  }

  /**
   * Detect Skyrim version from executable name
   */
  private detectVersion(executableName: string) {
    if (executableName.includes('SE')) {
      return { name: 'Skyrim SE', executable: executableName, version: '1.5.97' };
    } else if (executableName.includes('AE')) {
      return { name: 'Skyrim AE', executable: executableName, version: '1.6.x' };
    }
    return { name: 'Skyrim', executable: executableName, version: 'unknown' };
  }

  /**
   * Read memory at address
   */
  readMemory(address: number, size: number): Buffer {
    if (!this.processHandle) {
      throw new Error('Not attached to process');
    }

    const buffer = Buffer.alloc(size);
    const bytesRead = ref.alloc(SIZE_T);

    const success = kernel32.ReadProcessMemory(
      this.processHandle,
      address,
      buffer,
      size,
      bytesRead
    );

    if (!success) {
      throw new Error(`Failed to read memory at address 0x${address.toString(16)}`);
    }

    return buffer;
  }

  /**
   * Read a 32-bit integer
   */
  readInt32(address: number): number {
    const buffer = this.readMemory(address, 4);
    return buffer.readInt32LE(0);
  }

  /**
   * Read a 64-bit integer (as BigInt)
   */
  readInt64(address: number): bigint {
    const buffer = this.readMemory(address, 8);
    return buffer.readBigInt64LE(0);
  }

  /**
   * Read a float
   */
  readFloat(address: number): number {
    const buffer = this.readMemory(address, 4);
    return buffer.readFloatLE(0);
  }

  /**
   * Read a pointer (64-bit address)
   */
  readPointer(address: number): number {
    const buffer = this.readMemory(address, 8);
    return Number(buffer.readBigUInt64LE(0));
  }

  /**
   * Follow a pointer chain and return final address
   */
  resolvePointerChain(offsets: number[]): number {
    if (!this.processInfo) {
      throw new Error('Not attached to process');
    }

    let address = this.processInfo.baseAddress + offsets[0];

    // Follow each pointer in the chain
    for (let i = 1; i < offsets.length; i++) {
      address = this.readPointer(address);
      if (address === 0) {
        throw new Error(`Null pointer encountered at offset index ${i}`);
      }
      address += offsets[i];
    }

    return address;
  }

  /**
   * Read a null-terminated string
   */
  readString(address: number, maxLength: number = 256): string {
    const buffer = this.readMemory(address, maxLength);
    const nullIndex = buffer.indexOf(0);
    return buffer.toString('utf8', 0, nullIndex === -1 ? maxLength : nullIndex);
  }

  /**
   * Write memory (for potential future use)
   */
  writeMemory(address: number, buffer: Buffer): void {
    if (!this.processHandle) {
      throw new Error('Not attached to process');
    }

    const bytesWritten = ref.alloc(SIZE_T);

    const success = kernel32.WriteProcessMemory(
      this.processHandle,
      address,
      buffer,
      buffer.length,
      bytesWritten
    );

    if (!success) {
      throw new Error(`Failed to write memory at address 0x${address.toString(16)}`);
    }
  }

  /**
   * Check if still attached to process
   */
  isAttached(): boolean {
    return this.processHandle !== null;
  }

  /**
   * Get current process info
   */
  getProcessInfo(): ProcessInfo | null {
    return this.processInfo;
  }

  /**
   * Detach from process
   */
  detach(): void {
    if (this.processHandle) {
      kernel32.CloseHandle(this.processHandle);
      this.processHandle = null;
      this.processInfo = null;
    }
  }
}
