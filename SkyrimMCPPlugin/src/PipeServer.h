#pragma once

#include <atomic>
#include <thread>
#include <cstdint>

// Forward declare HANDLE as void* to avoid including windows.h in header
// (CommonLibSSE-NG requires its headers before any Windows API includes)
using PipeHandle = void*;

namespace SkyrimMCP {

    class PipeServer {
    public:
        PipeServer();
        ~PipeServer();

        void Start();
        void Stop();

    private:
        void ServerLoop();
        void HandleClient(PipeHandle pipe);

        std::thread _thread;
        std::atomic<bool> _running{false};
        PipeHandle _stopEvent;

        static constexpr const char* PIPE_NAME = "\\\\.\\pipe\\SkyrimMCP";
        static constexpr unsigned long BUFFER_SIZE = 65536;
    };

}
