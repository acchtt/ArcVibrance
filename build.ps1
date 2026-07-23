param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64','ARM64')]
    [string]$Platform = 'x64',
    [switch]$SelfContained
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildRoot = Join-Path $Root 'build'
$NativeBuild = Join-Path $BuildRoot "native-$Platform"
$Dist = Join-Path $Root "dist\$Platform"

function Stop-ArcVibranceProcesses {
    $processNames = @('ArcVibrance.exe', 'ArcVibrance.Agent.exe')
    $runningProcesses = @(
        Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
            Where-Object { $processNames -contains $_.Name }
    )

    foreach ($process in $runningProcesses) {
        $processPath = [string]$process.ExecutablePath
        $isProjectProcess = ([string]::IsNullOrWhiteSpace($processPath) -or $processPath.StartsWith($Root, [System.StringComparison]::OrdinalIgnoreCase))

        if (-not $isProjectProcess) {
            Write-Warning "Leaving $($process.Name) running because it is outside this project: $processPath"
            continue
        }

        Write-Host "Stopping running $($process.Name) (PID $($process.ProcessId))..." -ForegroundColor Yellow
        Stop-Process -Id ([int]$process.ProcessId) -Force -ErrorAction Stop

        try {
            Wait-Process -Id ([int]$process.ProcessId) -Timeout 10 -ErrorAction Stop
        }
        catch {
            if (Get-Process -Id ([int]$process.ProcessId) -ErrorAction SilentlyContinue) {
                throw "Could not stop $($process.Name) (PID $($process.ProcessId)). Close it manually and run the build again."
            }
        }
    }

    # Give Windows a moment to release executable and DLL file handles.
    Start-Sleep -Milliseconds 350
}

# A hidden/minimized WinUI instance still locks apphost.exe. Stop local app and
# agent instances before compiling so dotnet publish can replace the binaries.
Stop-ArcVibranceProcesses

if ($Platform -ne 'x64') {
    throw 'The Intel native agent is currently configured and tested for x64. Use -Platform x64.'
}

cmake -S (Join-Path $Root 'NativeAgent') -B $NativeBuild -A $Platform -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE." }

cmake --build $NativeBuild --config $Configuration --target ArcVibranceAgent
if ($LASTEXITCODE -ne 0) { throw "Native agent build failed with exit code $LASTEXITCODE." }

$AgentExe = Join-Path $NativeBuild "$Configuration\ArcVibrance.Agent.exe"
$AgentPdb = Join-Path $NativeBuild "$Configuration\ArcVibrance.Agent.pdb"
if (-not (Test-Path $AgentExe)) {
    throw "Native build completed without producing $AgentExe."
}

if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Path $Dist | Out-Null

$Project = Join-Path $Root 'ArcVibrance.WinUI\ArcVibrance.WinUI.csproj'
$Runtime = "win-$($Platform.ToLowerInvariant())"
$SelfContainedValue = $SelfContained.IsPresent.ToString().ToLowerInvariant()

# Pass the freshly built native binary into MSBuild.  The csproj marks it as
# content for both normal build output and publish output, which guarantees
# that ArcVibrance.Agent.exe sits beside every ArcVibrance.exe produced here.
$PublishArguments = @(
    'publish', $Project,
    '-c', $Configuration,
    '-r', $Runtime,
    "-p:Platform=$Platform",
    "-p:SelfContained=$SelfContainedValue",
    "-p:WindowsAppSDKSelfContained=$SelfContainedValue",
    "-p:NativeAgentSourcePath=$AgentExe",
    "-p:NativeAgentPdbSourcePath=$AgentPdb",
    '-o', $Dist
)
& dotnet @PublishArguments
if ($LASTEXITCODE -ne 0) { throw "WinUI publish failed with exit code $LASTEXITCODE." }

# Keep an explicit post-publish copy as a final safeguard against custom MSBuild
# output layouts or incremental publish behavior.
Copy-Item $AgentExe (Join-Path $Dist 'ArcVibrance.Agent.exe') -Force
if ($Configuration -ne 'Release' -and (Test-Path $AgentPdb)) {
    Copy-Item $AgentPdb (Join-Path $Dist 'ArcVibrance.Agent.pdb') -Force
}

# Include the two path-based artwork files explicitly. Single-file publishing
# bundles content for extraction, but the app intentionally loads these files
# from its installation directory for the window and About-page artwork.
$SourceAssets = Join-Path $Root 'ArcVibrance.WinUI\Assets'
$DistAssets = Join-Path $Dist 'Assets'
New-Item -ItemType Directory -Path $DistAssets -Force | Out-Null
Copy-Item (Join-Path $SourceAssets 'ArcVibrance.ico') (Join-Path $DistAssets 'ArcVibrance.ico') -Force
Copy-Item (Join-Path $SourceAssets 'ArcVibrance.png') (Join-Path $DistAssets 'ArcVibrance.png') -Force

# A developer may launch ArcVibrance.exe directly from bin instead of dist.
# Ensure each local WinUI output directory also receives the companion agent.
$UiBinRoot = Join-Path $Root "ArcVibrance.WinUI\bin\$Platform\$Configuration"
if (Test-Path $UiBinRoot) {
    $UiExecutables = @(Get-ChildItem $UiBinRoot -Filter 'ArcVibrance.exe' -File -Recurse -ErrorAction SilentlyContinue)
    foreach ($uiExecutable in $UiExecutables) {
        Copy-Item $AgentExe (Join-Path $uiExecutable.DirectoryName 'ArcVibrance.Agent.exe') -Force
        if ($Configuration -ne 'Release' -and (Test-Path $AgentPdb)) {
            Copy-Item $AgentPdb (Join-Path $uiExecutable.DirectoryName 'ArcVibrance.Agent.pdb') -Force
        }
    }
}

$UiExe = Join-Path $Dist 'ArcVibrance.exe'
$DistAgentExe = Join-Path $Dist 'ArcVibrance.Agent.exe'
if (-not (Test-Path $UiExe)) {
    throw "Distribution verification failed: $UiExe was not produced."
}
if (-not (Test-Path $DistAgentExe)) {
    throw "Distribution verification failed: $DistAgentExe was not copied beside ArcVibrance.exe."
}

$uiInfo = Get-Item $UiExe
$agentInfo = Get-Item $DistAgentExe
if ($uiInfo.Length -le 0 -or $agentInfo.Length -le 0) {
    throw 'Distribution verification failed: one or more executable files are empty.'
}

$UnexpectedDebugFiles = @(Get-ChildItem $Dist -Filter '*.pdb' -File -Recurse -ErrorAction SilentlyContinue)
if ($Configuration -eq 'Release' -and $UnexpectedDebugFiles.Count -gt 0) {
    throw "Distribution verification failed: Release output contains debug symbols: $($UnexpectedDebugFiles.FullName -join ', ')"
}

Write-Host "Build complete: $Dist" -ForegroundColor Cyan
Write-Host "  UI:    $UiExe" -ForegroundColor DarkCyan
Write-Host "  Agent: $DistAgentExe" -ForegroundColor DarkCyan
Write-Host "Launch this copy: $UiExe" -ForegroundColor Green
