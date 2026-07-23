using System.ComponentModel;
using System.Diagnostics;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using ArcVibrance.Models;

namespace ArcVibrance.Services;

public enum AgentCommand : uint
{
    GetStatus = 1,
    ReloadProfiles = 2,
    ReapplyActiveProfile = 3,
    RestoreDesktop = 4,
    Shutdown = 5
}

internal enum AgentError : uint
{
    None = 0,
    InvalidRequest = 1,
    UnsupportedProtocol = 2,
    RuntimeFailure = 3,
    ProfileLoadFailure = 4
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
internal struct AgentRequest
{
    public uint Size;
    public uint ProtocolVersion;
    public AgentCommand Command;
    public uint RequestId;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 4)]
internal struct NativeAgentStatus
{
    public uint Size;
    public uint ProtocolVersion;
    [MarshalAs(UnmanagedType.Bool)] public bool AgentRunning;
    [MarshalAs(UnmanagedType.Bool)] public bool RuntimeInitialized;
    [MarshalAs(UnmanagedType.Bool)] public bool GameActive;
    public int ActiveProfileIndex;
    public int AppliedSaturationPercent;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string ActiveExecutableName;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 4)]
internal struct AgentResponse
{
    public uint Size;
    public uint ProtocolVersion;
    public uint RequestId;
    [MarshalAs(UnmanagedType.Bool)] public bool Success;
    public AgentError Error;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 160)] public string ErrorMessage;
    public NativeAgentStatus Status;
}

public sealed class AgentClient
{
    private const string PipeName = "ArcVibrance.Agent.v1";
    private const uint ProtocolVersion = 1;
    private static readonly TimeSpan ProbeTimeout = TimeSpan.FromMilliseconds(450);
    private static readonly TimeSpan CommandTimeout = TimeSpan.FromSeconds(2);
    private readonly SemaphoreSlim _launchGate = new(1, 1);
    private int _nextRequestId;

    public async Task<bool> EnsureRunningAsync(CancellationToken cancellationToken = default)
    {
        if (await IsResponsiveAsync(cancellationToken))
        {
            return true;
        }

        await _launchGate.WaitAsync(cancellationToken);
        try
        {
            if (await IsResponsiveAsync(cancellationToken))
            {
                return true;
            }

            string agentPath = Path.Combine(AppContext.BaseDirectory, "ArcVibrance.Agent.exe");
            if (!File.Exists(agentPath))
            {
                throw new FileNotFoundException(
                    $"ArcVibrance.Agent.exe was not found in '{AppContext.BaseDirectory}'. " +
                    "Run build.ps1 and launch dist\\x64\\ArcVibrance.exe.",
                    agentPath);
            }

            Process? process;
            try
            {
                process = Process.Start(new ProcessStartInfo
                {
                    FileName = agentPath,
                    WorkingDirectory = AppContext.BaseDirectory,
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Hidden
                });
            }
            catch (Exception exception) when (exception is Win32Exception or InvalidOperationException)
            {
                throw new InvalidOperationException(
                    $"Windows could not start the ArcVibrance tray agent: {exception.Message}", exception);
            }

            if (process is null)
            {
                throw new InvalidOperationException("Windows did not create the ArcVibrance tray-agent process.");
            }

            for (int attempt = 0; attempt < 60; attempt++)
            {
                await Task.Delay(250, cancellationToken);
                if (await IsResponsiveAsync(cancellationToken))
                {
                    return true;
                }

                if (process.HasExited && process.ExitCode != 0)
                {
                    string logPath = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                        "ArcVibrance", "Logs", "ArcVibrance.Agent.log");
                    throw new InvalidOperationException(
                        $"ArcVibrance.Agent.exe exited during startup (code {process.ExitCode}). " +
                        $"Check the agent log at {logPath}.");
                }
                // Exit code 0 can mean that another agent instance already owns
                // the mutex and is still completing startup, so keep probing.
            }

            throw new TimeoutException("The ArcVibrance tray agent started but its IPC service did not become ready.");
        }
        finally
        {
            _launchGate.Release();
        }
    }

    public async Task<AgentStatus> GetStatusAsync(CancellationToken cancellationToken = default)
    {
        AgentResponse? response = await SendAsync(AgentCommand.GetStatus, CommandTimeout, cancellationToken);
        return response is { Success: true } value ? Convert(value.Status) : AgentStatus.Disconnected;
    }

    public async Task<AgentStatus> ReloadProfilesAsync(CancellationToken cancellationToken = default) =>
        await SendStatusCommandAsync(AgentCommand.ReloadProfiles, cancellationToken);

    public async Task<AgentStatus> ReapplyActiveProfileAsync(CancellationToken cancellationToken = default) =>
        await SendStatusCommandAsync(AgentCommand.ReapplyActiveProfile, cancellationToken);

    public async Task<AgentStatus> RestoreDesktopAsync(CancellationToken cancellationToken = default) =>
        await SendStatusCommandAsync(AgentCommand.RestoreDesktop, cancellationToken);

    private async Task<bool> IsResponsiveAsync(CancellationToken cancellationToken) =>
        (await SendAsync(AgentCommand.GetStatus, ProbeTimeout, cancellationToken)) is { Success: true };

    private async Task<AgentStatus> SendStatusCommandAsync(AgentCommand command, CancellationToken cancellationToken)
    {
        AgentResponse? response = await SendAsync(command, CommandTimeout, cancellationToken);
        if (response is not { Success: true } value)
        {
            throw new IOException(response?.ErrorMessage ?? "ArcVibrance Agent is not available.");
        }

        return Convert(value.Status);
    }

    private async Task<AgentResponse?> SendAsync(
        AgentCommand command,
        TimeSpan operationTimeout,
        CancellationToken cancellationToken)
    {
        uint requestId = unchecked((uint)Interlocked.Increment(ref _nextRequestId));
        var request = new AgentRequest
        {
            Size = (uint)Marshal.SizeOf<AgentRequest>(),
            ProtocolVersion = ProtocolVersion,
            Command = command,
            RequestId = requestId
        };

        try
        {
            using var pipe = new NamedPipeClientStream(
                ".",
                PipeName,
                PipeDirection.InOut,
                PipeOptions.Asynchronous);

            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeout.CancelAfter(operationTimeout);
            await pipe.ConnectAsync(timeout.Token);

            byte[] requestBytes = StructureToBytes(request);
            await pipe.WriteAsync(requestBytes, timeout.Token);
            await pipe.FlushAsync(timeout.Token);

            byte[] responseBytes = new byte[Marshal.SizeOf<AgentResponse>()];
            int total = 0;
            while (total < responseBytes.Length)
            {
                int read = await pipe.ReadAsync(responseBytes.AsMemory(total), timeout.Token);
                if (read == 0)
                {
                    return null;
                }
                total += read;
            }

            AgentResponse response = BytesToStructure<AgentResponse>(responseBytes);
            int expectedResponseSize = Marshal.SizeOf<AgentResponse>();
            int expectedStatusSize = Marshal.SizeOf<NativeAgentStatus>();
            return response.Size == (uint)expectedResponseSize &&
                   response.ProtocolVersion == ProtocolVersion &&
                   response.RequestId == requestId &&
                   response.Status.Size == (uint)expectedStatusSize &&
                   response.Status.ProtocolVersion == ProtocolVersion
                ? response
                : null;
        }
        catch (Exception exception) when (
            exception is IOException or TimeoutException or OperationCanceledException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    private static AgentStatus Convert(NativeAgentStatus status) => new(
        status.AgentRunning,
        status.RuntimeInitialized,
        status.GameActive,
        status.ActiveProfileIndex,
        status.AppliedSaturationPercent,
        status.ActiveExecutableName?.TrimEnd('\0') ?? string.Empty);

    private static byte[] StructureToBytes<T>(T value) where T : struct
    {
        int size = Marshal.SizeOf<T>();
        byte[] bytes = new byte[size];
        IntPtr pointer = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(value, pointer, false);
            Marshal.Copy(pointer, bytes, 0, size);
            return bytes;
        }
        finally
        {
            Marshal.FreeHGlobal(pointer);
        }
    }

    private static T BytesToStructure<T>(byte[] bytes) where T : struct
    {
        IntPtr pointer = Marshal.AllocHGlobal(bytes.Length);
        try
        {
            Marshal.Copy(bytes, 0, pointer, bytes.Length);
            return Marshal.PtrToStructure<T>(pointer);
        }
        finally
        {
            Marshal.FreeHGlobal(pointer);
        }
    }
}
