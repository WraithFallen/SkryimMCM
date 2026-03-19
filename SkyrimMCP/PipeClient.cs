using System.IO.Pipes;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SkyrimMCP;

/// <summary>
/// Named pipe client that communicates with the SKSE plugin running inside Skyrim
/// </summary>
public class PipeClient : IDisposable
{
    private const string PipeName = "SkyrimMCP";
    private const int ConnectTimeoutMs = 3000;
    private const int ResponseTimeoutMs = 10000;

    private NamedPipeClientStream? _pipe;
    private StreamReader? _reader;
    private StreamWriter? _writer;
    private readonly SemaphoreSlim _sendLock = new(1, 1);

    public bool IsConnected => _pipe?.IsConnected ?? false;

    /// <summary>
    /// Try to connect to the SKSE plugin's named pipe
    /// </summary>
    public async Task<bool> TryConnectAsync()
    {
        try
        {
            Disconnect();

            _pipe = new NamedPipeClientStream(".", PipeName, PipeDirection.InOut);
            await _pipe.ConnectAsync(ConnectTimeoutMs);

            _reader = new StreamReader(_pipe);
            _writer = new StreamWriter(_pipe) { AutoFlush = true };

            return true;
        }
        catch (TimeoutException)
        {
            Disconnect();
            return false;
        }
        catch (Exception)
        {
            Disconnect();
            return false;
        }
    }

    /// <summary>
    /// Send a request to the SKSE plugin and wait for a response
    /// </summary>
    public async Task<JsonNode?> SendRequestAsync(string action, JsonObject? parameters = null)
    {
        await _sendLock.WaitAsync();
        try
        {
            if (!IsConnected)
            {
                bool connected = await TryConnectAsync();
                if (!connected)
                {
                    throw new InvalidOperationException(
                        "Cannot connect to Skyrim. Make sure the game is running with SKSE and the SkyrimMCPPlugin is loaded.");
                }
            }

            var request = new JsonObject
            {
                ["id"] = Guid.NewGuid().ToString(),
                ["action"] = action,
                ["params"] = parameters ?? new JsonObject()
            };

            var requestLine = request.ToJsonString();
            await _writer!.WriteLineAsync(requestLine);
            await _writer.FlushAsync();

            // Read response with timeout
            using var cts = new CancellationTokenSource(ResponseTimeoutMs);
            var responseLine = await _reader!.ReadLineAsync(cts.Token);

            if (string.IsNullOrEmpty(responseLine))
            {
                Disconnect();
                throw new InvalidOperationException("Empty response from SKSE plugin - connection may have been lost");
            }

            var response = JsonNode.Parse(responseLine);
            if (response == null)
            {
                throw new InvalidOperationException("Failed to parse response from SKSE plugin");
            }

            var success = response["success"]?.GetValue<bool>() ?? false;
            if (!success)
            {
                var error = response["error"]?.GetValue<string>() ?? "Unknown error from SKSE plugin";
                throw new InvalidOperationException(error);
            }

            return response["data"];
        }
        catch (OperationCanceledException)
        {
            Disconnect();
            throw new TimeoutException("Timed out waiting for response from SKSE plugin - game may be loading or paused");
        }
        catch (IOException)
        {
            Disconnect();
            throw new InvalidOperationException("Lost connection to Skyrim - the game may have been closed");
        }
        finally
        {
            _sendLock.Release();
        }
    }

    /// <summary>
    /// Check if the SKSE plugin is reachable
    /// </summary>
    public async Task<bool> PingAsync()
    {
        try
        {
            await SendRequestAsync("ping");
            return true;
        }
        catch
        {
            return false;
        }
    }

    private void Disconnect()
    {
        _writer?.Dispose();
        _reader?.Dispose();
        _pipe?.Dispose();
        _writer = null;
        _reader = null;
        _pipe = null;
    }

    public void Dispose()
    {
        Disconnect();
        _sendLock.Dispose();
    }
}
