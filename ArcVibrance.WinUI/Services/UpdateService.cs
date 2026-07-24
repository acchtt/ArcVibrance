using System.Net;
using System.Net.Http.Headers;
using System.Diagnostics;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace ArcVibrance.Services;

public sealed class UpdateService
{
    private const string LatestReleaseEndpoint =
        "https://api.github.com/repos/acchtt/ArcVibrance/releases/latest";

    public static string CurrentVersionTag { get; } = GetCurrentVersionTag();

    private static readonly HttpClient Client = CreateHttpClient();

    public async Task<UpdateCheckResult> CheckForUpdatesAsync(
        CancellationToken cancellationToken = default)
    {
        using HttpRequestMessage request = new(HttpMethod.Get, LatestReleaseEndpoint);
        request.Headers.Accept.Add(
            new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        request.Headers.Add("X-GitHub-Api-Version", "2022-11-28");

        using HttpResponseMessage response = await Client.SendAsync(
            request,
            HttpCompletionOption.ResponseHeadersRead,
            cancellationToken);

        if (response.StatusCode == HttpStatusCode.Forbidden &&
            response.Headers.TryGetValues("X-RateLimit-Remaining", out IEnumerable<string>? remaining) &&
            remaining.Contains("0", StringComparer.Ordinal))
        {
            throw new InvalidOperationException(
                "GitHub's update-check limit has been reached. Please try again later.");
        }

        response.EnsureSuccessStatusCode();

        await using Stream stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        GitHubRelease? release = await JsonSerializer.DeserializeAsync<GitHubRelease>(
            stream,
            cancellationToken: cancellationToken);

        if (release is null ||
            release.Draft ||
            release.Prerelease ||
            string.IsNullOrWhiteSpace(release.TagName))
        {
            throw new InvalidDataException(
                "GitHub did not return a valid stable ArcVibrance release.");
        }

        Version currentVersion = ParseVersion(CurrentVersionTag);
        Version latestVersion = ParseVersion(release.TagName);
        Uri releaseUri = GetTrustedRepositoryUri(release.HtmlUrl, "release page");

        string expectedAssetName = $"ArcVibrance-{release.TagName}-win-x64.zip";
        GitHubReleaseAsset? asset = release.Assets.FirstOrDefault(item =>
            string.Equals(item.Name, expectedAssetName, StringComparison.OrdinalIgnoreCase));
        GitHubReleaseAsset? checksumAsset = release.Assets.FirstOrDefault(item =>
            string.Equals(
                item.Name,
                $"{expectedAssetName}.sha256",
                StringComparison.OrdinalIgnoreCase));

        Uri? downloadUri = asset is null
            ? null
            : GetTrustedRepositoryUri(asset.DownloadUrl, "release download");
        Uri? checksumDownloadUri = checksumAsset is null
            ? null
            : GetTrustedRepositoryUri(checksumAsset.DownloadUrl, "release checksum");

        return new UpdateCheckResult(
            IsUpdateAvailable: latestVersion > currentVersion,
            CurrentVersionTag,
            LatestVersionTag: NormalizeTag(release.TagName),
            ReleaseName: string.IsNullOrWhiteSpace(release.Name)
                ? $"ArcVibrance {NormalizeTag(release.TagName)}"
                : release.Name.Trim(),
            ReleaseNotes: release.Body?.Trim() ?? string.Empty,
            releaseUri,
            downloadUri,
            checksumDownloadUri,
            AssetName: asset?.Name,
            AssetSizeBytes: asset?.Size ?? 0,
            release.PublishedAt);
    }

    public async Task<PreparedUpdate> PrepareUpdateAsync(
        UpdateCheckResult update,
        IProgress<UpdateProgress>? progress = null,
        CancellationToken cancellationToken = default)
    {
        if (!update.IsUpdateAvailable)
        {
            throw new InvalidOperationException("The selected release is not newer than this installation.");
        }

        if (update.DownloadUri is null ||
            update.ChecksumDownloadUri is null ||
            string.IsNullOrWhiteSpace(update.AssetName))
        {
            throw new InvalidDataException(
                "This release does not include both the Windows archive and its SHA-256 checksum.");
        }

        string updatesRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "ArcVibrance",
            "Updates");
        string updateRoot = Path.Combine(
            updatesRoot,
            SanitizePathComponent(update.LatestVersionTag));
        string archivePath = Path.Combine(updateRoot, update.AssetName);
        string stagingDirectory = Path.Combine(updateRoot, "staged");

        if (Directory.Exists(updateRoot))
        {
            Directory.Delete(updateRoot, recursive: true);
        }

        Directory.CreateDirectory(updateRoot);

        progress?.Report(new UpdateProgress("Downloading update…", 0));
        await DownloadFileAsync(
            update.DownloadUri,
            archivePath,
            update.AssetSizeBytes,
            progress,
            cancellationToken);

        progress?.Report(new UpdateProgress("Verifying SHA-256 checksum…", null));
        string checksumText = await Client.GetStringAsync(
            update.ChecksumDownloadUri,
            cancellationToken);
        string expectedHash = ParseChecksum(checksumText, update.AssetName);
        string actualHash = await ComputeSha256Async(archivePath, cancellationToken);
        if (!string.Equals(expectedHash, actualHash, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                "The downloaded update failed SHA-256 verification and was not installed.");
        }

        progress?.Report(new UpdateProgress("Preparing update files…", null));
        ExtractArchiveSafely(archivePath, stagingDirectory);
        ValidateStagedUpdate(stagingDirectory);
        EnsureInstallDirectoryWritable(AppContext.BaseDirectory);

        string installerScriptPath = Path.Combine(updatesRoot, "ArcVibrance.Update.ps1");
        Directory.CreateDirectory(updatesRoot);
        await File.WriteAllTextAsync(
            installerScriptPath,
            InstallerScript,
            cancellationToken);

        progress?.Report(new UpdateProgress("Ready to restart", 100));
        return new PreparedUpdate(
            update.LatestVersionTag,
            stagingDirectory,
            installerScriptPath);
    }

    public static void LaunchPreparedUpdate(PreparedUpdate update)
    {
        string installationDirectory = Path.GetFullPath(AppContext.BaseDirectory);
        var startInfo = new ProcessStartInfo
        {
            FileName = "powershell.exe",
            UseShellExecute = false,
            CreateNoWindow = true
        };
        startInfo.ArgumentList.Add("-NoLogo");
        startInfo.ArgumentList.Add("-NoProfile");
        startInfo.ArgumentList.Add("-NonInteractive");
        startInfo.ArgumentList.Add("-ExecutionPolicy");
        startInfo.ArgumentList.Add("Bypass");
        startInfo.ArgumentList.Add("-File");
        startInfo.ArgumentList.Add(update.InstallerScriptPath);
        startInfo.ArgumentList.Add("-InstallDirectory");
        startInfo.ArgumentList.Add(installationDirectory);
        startInfo.ArgumentList.Add("-StagingDirectory");
        startInfo.ArgumentList.Add(update.StagingDirectory);
        startInfo.ArgumentList.Add("-ApplicationPid");
        startInfo.ArgumentList.Add(Environment.ProcessId.ToString());

        Process? process = Process.Start(startInfo);
        if (process is null)
        {
            throw new InvalidOperationException("Windows could not start the ArcVibrance updater.");
        }
    }

    internal static Version ParseVersion(string value)
    {
        string normalized = value.Trim();
        if (normalized.StartsWith('v') || normalized.StartsWith('V'))
        {
            normalized = normalized[1..];
        }

        int suffixIndex = normalized.IndexOfAny(['-', '+']);
        if (suffixIndex >= 0)
        {
            normalized = normalized[..suffixIndex];
        }

        string[] components = normalized.Split('.', StringSplitOptions.RemoveEmptyEntries);
        if (components.Length is < 1 or > 4)
        {
            throw new FormatException($"Unsupported ArcVibrance version: {value}");
        }

        int[] numbers = new int[4];
        for (int index = 0; index < components.Length; index++)
        {
            if (!int.TryParse(components[index], out numbers[index]) || numbers[index] < 0)
            {
                throw new FormatException($"Unsupported ArcVibrance version: {value}");
            }
        }

        return new Version(numbers[0], numbers[1], numbers[2], numbers[3]);
    }

    private static HttpClient CreateHttpClient()
    {
        var client = new HttpClient
        {
            Timeout = TimeSpan.FromMinutes(5)
        };
        client.DefaultRequestHeaders.UserAgent.Add(
            new ProductInfoHeaderValue("ArcVibrance", CurrentVersionTag.TrimStart('v', 'V')));
        return client;
    }

    private static async Task DownloadFileAsync(
        Uri downloadUri,
        string destinationPath,
        long expectedSize,
        IProgress<UpdateProgress>? progress,
        CancellationToken cancellationToken)
    {
        using HttpRequestMessage request = new(HttpMethod.Get, downloadUri);
        using HttpResponseMessage response = await Client.SendAsync(
            request,
            HttpCompletionOption.ResponseHeadersRead,
            cancellationToken);
        response.EnsureSuccessStatusCode();

        long? contentLength = response.Content.Headers.ContentLength;
        if (expectedSize > 0 && contentLength is long responseSize && responseSize != expectedSize)
        {
            throw new InvalidDataException(
                "GitHub returned an update with an unexpected download size.");
        }

        await using Stream source = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using FileStream destination = new(
            destinationPath,
            FileMode.CreateNew,
            FileAccess.Write,
            FileShare.None,
            bufferSize: 81920,
            useAsync: true);

        byte[] buffer = new byte[81920];
        long bytesRead = 0;
        while (true)
        {
            int count = await source.ReadAsync(buffer, cancellationToken);
            if (count == 0)
            {
                break;
            }

            await destination.WriteAsync(buffer.AsMemory(0, count), cancellationToken);
            bytesRead += count;

            long total = expectedSize > 0 ? expectedSize : contentLength ?? 0;
            double? percent = total > 0
                ? Math.Clamp(bytesRead * 100d / total, 0d, 100d)
                : null;
            progress?.Report(new UpdateProgress("Downloading update…", percent));
        }

        if (expectedSize > 0 && bytesRead != expectedSize)
        {
            throw new InvalidDataException(
                "The update download was incomplete and was not installed.");
        }
    }

    private static string ParseChecksum(string content, string expectedFileName)
    {
        Match match = Regex.Match(
            content.Trim(),
            @"\A(?<hash>[a-fA-F0-9]{64})(?:\s+\*?(?<name>[^\r\n]+))?\z",
            RegexOptions.CultureInvariant);
        if (!match.Success)
        {
            throw new InvalidDataException("The release checksum file has an invalid format.");
        }

        string suppliedName = match.Groups["name"].Value.Trim();
        if (suppliedName.Length > 0 &&
            !string.Equals(
                Path.GetFileName(suppliedName),
                expectedFileName,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                "The release checksum does not belong to the downloaded update.");
        }

        return match.Groups["hash"].Value.ToLowerInvariant();
    }

    private static async Task<string> ComputeSha256Async(
        string path,
        CancellationToken cancellationToken)
    {
        await using FileStream stream = new(
            path,
            FileMode.Open,
            FileAccess.Read,
            FileShare.Read,
            bufferSize: 81920,
            useAsync: true);
        byte[] hash = await SHA256.HashDataAsync(stream, cancellationToken);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static void ExtractArchiveSafely(string archivePath, string destinationDirectory)
    {
        Directory.CreateDirectory(destinationDirectory);
        string destinationRoot = Path.GetFullPath(destinationDirectory)
            .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;

        using ZipArchive archive = ZipFile.OpenRead(archivePath);
        if (archive.Entries.Count == 0)
        {
            throw new InvalidDataException("The update archive is empty.");
        }

        foreach (ZipArchiveEntry entry in archive.Entries)
        {
            string destinationPath = Path.GetFullPath(
                Path.Combine(destinationRoot, entry.FullName));
            if (!destinationPath.StartsWith(destinationRoot, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    "The update archive contains an unsafe file path.");
            }

            if (string.IsNullOrEmpty(entry.Name))
            {
                Directory.CreateDirectory(destinationPath);
                continue;
            }

            Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
            entry.ExtractToFile(destinationPath, overwrite: false);
        }
    }

    private static void ValidateStagedUpdate(string stagingDirectory)
    {
        string[] requiredFiles =
        [
            "ArcVibrance.exe",
            "ArcVibrance.Agent.exe",
            Path.Combine("Assets", "ArcVibrance.ico"),
            Path.Combine("Assets", "ArcVibrance.png")
        ];

        foreach (string relativePath in requiredFiles)
        {
            string fullPath = Path.Combine(stagingDirectory, relativePath);
            if (!File.Exists(fullPath) || new FileInfo(fullPath).Length <= 0)
            {
                throw new InvalidDataException(
                    $"The update archive is missing the required file '{relativePath}'.");
            }
        }
    }

    private static void EnsureInstallDirectoryWritable(string installationDirectory)
    {
        string probePath = Path.Combine(
            installationDirectory,
            $".arcvibrance-update-{Guid.NewGuid():N}.tmp");
        try
        {
            using FileStream _ = new(
                probePath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                1,
                FileOptions.DeleteOnClose);
        }
        catch (UnauthorizedAccessException exception)
        {
            throw new InvalidOperationException(
                "ArcVibrance cannot update this installation without write permission. " +
                "Move it to a user-writable folder or run it as administrator.",
                exception);
        }
    }

    private static string SanitizePathComponent(string value)
    {
        char[] invalidCharacters = Path.GetInvalidFileNameChars();
        return string.Concat(value.Select(character =>
            invalidCharacters.Contains(character) ? '_' : character));
    }

    private static string GetCurrentVersionTag()
    {
        Assembly assembly = Assembly.GetEntryAssembly() ?? typeof(UpdateService).Assembly;
        string? informationalVersion = assembly
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()
            ?.InformationalVersion
            .Split('+', 2)[0]
            .Trim();

        if (!string.IsNullOrWhiteSpace(informationalVersion))
        {
            return NormalizeTag(informationalVersion);
        }

        Version version = assembly.GetName().Version ?? new Version(1, 0);
        return $"v{version.Major}.{version.Minor}" +
               (version.Build > 0 ? $".{version.Build}" : string.Empty);
    }

    private static string NormalizeTag(string value)
    {
        string trimmed = value.Trim();
        return trimmed.StartsWith('v') || trimmed.StartsWith('V')
            ? $"v{trimmed[1..]}"
            : $"v{trimmed}";
    }

    private static Uri GetTrustedRepositoryUri(string? value, string description)
    {
        if (!Uri.TryCreate(value, UriKind.Absolute, out Uri? uri) ||
            uri.Scheme != Uri.UriSchemeHttps ||
            !string.Equals(uri.Host, "github.com", StringComparison.OrdinalIgnoreCase) ||
            !uri.AbsolutePath.StartsWith(
                "/acchtt/ArcVibrance/",
                StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                $"GitHub returned an unexpected ArcVibrance {description} URL.");
        }

        return uri;
    }

    private sealed class GitHubRelease
    {
        [JsonPropertyName("tag_name")]
        public string TagName { get; init; } = string.Empty;

        [JsonPropertyName("name")]
        public string Name { get; init; } = string.Empty;

        [JsonPropertyName("body")]
        public string? Body { get; init; }

        [JsonPropertyName("html_url")]
        public string HtmlUrl { get; init; } = string.Empty;

        [JsonPropertyName("draft")]
        public bool Draft { get; init; }

        [JsonPropertyName("prerelease")]
        public bool Prerelease { get; init; }

        [JsonPropertyName("published_at")]
        public DateTimeOffset? PublishedAt { get; init; }

        [JsonPropertyName("assets")]
        public List<GitHubReleaseAsset> Assets { get; init; } = [];
    }

    private sealed class GitHubReleaseAsset
    {
        [JsonPropertyName("name")]
        public string Name { get; init; } = string.Empty;

        [JsonPropertyName("browser_download_url")]
        public string DownloadUrl { get; init; } = string.Empty;

        [JsonPropertyName("size")]
        public long Size { get; init; }
    }

    private const string InstallerScript =
        """
        param(
            [Parameter(Mandatory=$true)][string]$InstallDirectory,
            [Parameter(Mandatory=$true)][string]$StagingDirectory,
            [Parameter(Mandatory=$true)][int]$ApplicationPid
        )

        $ErrorActionPreference = 'Stop'
        $install = [IO.Path]::GetFullPath($InstallDirectory)
        $staging = [IO.Path]::GetFullPath($StagingDirectory)
        $updateRoot = Split-Path -Parent $staging
        $backup = Join-Path $updateRoot 'backup'
        $log = Join-Path $updateRoot 'update.log'
        $createdFiles = [Collections.Generic.List[string]]::new()

        try {
            "Waiting for ArcVibrance PID $ApplicationPid to exit." | Set-Content $log
            try { Wait-Process -Id $ApplicationPid -Timeout 60 -ErrorAction Stop } catch {
                Stop-Process -Id $ApplicationPid -Force -ErrorAction SilentlyContinue
                Start-Sleep -Milliseconds 500
            }

            $agentPath = Join-Path $install 'ArcVibrance.Agent.exe'
            Get-CimInstance Win32_Process -Filter "Name='ArcVibrance.Agent.exe'" -ErrorAction SilentlyContinue |
                Where-Object {
                    -not [string]::IsNullOrWhiteSpace($_.ExecutablePath) -and
                    [IO.Path]::GetFullPath($_.ExecutablePath).Equals(
                        $agentPath,
                        [StringComparison]::OrdinalIgnoreCase)
                } |
                ForEach-Object {
                    Stop-Process -Id ([int]$_.ProcessId) -Force -ErrorAction Stop
                    Wait-Process -Id ([int]$_.ProcessId) -Timeout 15 -ErrorAction SilentlyContinue
                }

            if (Test-Path $backup) { Remove-Item $backup -Recurse -Force }
            New-Item -ItemType Directory -Path $backup | Out-Null

            $files = @(Get-ChildItem $staging -File -Recurse)
            foreach ($source in $files) {
                $relative = $source.FullName.Substring($staging.Length).TrimStart('\')
                $destination = Join-Path $install $relative
                $destinationDirectory = Split-Path -Parent $destination
                New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null

                if (Test-Path $destination -PathType Leaf) {
                    $backupPath = Join-Path $backup $relative
                    New-Item -ItemType Directory -Path (Split-Path -Parent $backupPath) -Force | Out-Null
                    Copy-Item $destination $backupPath -Force
                } else {
                    $createdFiles.Add($destination)
                }

                Copy-Item $source.FullName $destination -Force
            }

            "Update installed successfully." | Add-Content $log
            Start-Process (Join-Path $install 'ArcVibrance.exe') -WorkingDirectory $install
        }
        catch {
            "Update failed: $($_.Exception.Message)" | Add-Content $log

            foreach ($createdFile in $createdFiles) {
                Remove-Item $createdFile -Force -ErrorAction SilentlyContinue
            }

            if (Test-Path $backup) {
                Get-ChildItem $backup -File -Recurse | ForEach-Object {
                    $relative = $_.FullName.Substring($backup.Length).TrimStart('\')
                    $destination = Join-Path $install $relative
                    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
                    Copy-Item $_.FullName $destination -Force
                }
            }

            if (Test-Path (Join-Path $install 'ArcVibrance.exe')) {
                Start-Process (Join-Path $install 'ArcVibrance.exe') -WorkingDirectory $install
            }
            exit 1
        }
        """;
}

public sealed record UpdateCheckResult(
    bool IsUpdateAvailable,
    string CurrentVersionTag,
    string LatestVersionTag,
    string ReleaseName,
    string ReleaseNotes,
    Uri ReleaseUri,
    Uri? DownloadUri,
    Uri? ChecksumDownloadUri,
    string? AssetName,
    long AssetSizeBytes,
    DateTimeOffset? PublishedAt);

public sealed record UpdateProgress(string Message, double? Percentage);

public sealed record PreparedUpdate(
    string VersionTag,
    string StagingDirectory,
    string InstallerScriptPath);
