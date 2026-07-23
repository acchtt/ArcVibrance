using System.Diagnostics;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace ArcVibrance.Services;

public enum GameNameSource
{
    SteamManifest,
    EpicManifest,
    ExecutableProductName,
    ExecutableDescription,
    KnownExecutable,
    InstallationFolder,
    ExecutableFileName
}

public sealed record GameNameResult(string Name, GameNameSource Source)
{
    public string SourceText => Source switch
    {
        GameNameSource.SteamManifest => "Detected from Steam",
        GameNameSource.EpicManifest => "Detected from Epic Games",
        GameNameSource.ExecutableProductName => "Detected from executable product information",
        GameNameSource.ExecutableDescription => "Detected from executable description",
        GameNameSource.KnownExecutable => "Recognized executable",
        GameNameSource.InstallationFolder => "Detected from installation folder",
        _ => "Derived from executable filename"
    };
}

public sealed partial class GameNameResolver
{
    private static readonly Dictionary<string, string> KnownExecutables = new(StringComparer.OrdinalIgnoreCase)
    {
        ["TslGame"] = "PUBG: BATTLEGROUNDS",
        ["VALORANT-Win64-Shipping"] = "VALORANT",
        ["VALORANT"] = "VALORANT",
        ["cs2"] = "Counter-Strike 2",
        ["csgo"] = "Counter-Strike: Global Offensive",
        ["LeagueClient"] = "League of Legends",
        ["LeagueClientUx"] = "League of Legends",
        ["LeagueClientUxRender"] = "League of Legends",
        ["Cyberpunk2077"] = "Cyberpunk 2077",
        ["RDR2"] = "Red Dead Redemption 2",
        ["SkyrimSE"] = "The Elder Scrolls V: Skyrim Special Edition",
        ["GTA5"] = "Grand Theft Auto V",
        ["eldenring"] = "ELDEN RING",
        ["FortniteClient-Win64-Shipping"] = "Fortnite",
        ["Overwatch"] = "Overwatch 2",
        ["RocketLeague"] = "Rocket League",
        ["MinecraftLauncher"] = "Minecraft"
    };

    private readonly Lazy<Task<IReadOnlyList<InstallManifest>>> _epicManifests;

    public GameNameResolver()
    {
        _epicManifests = new Lazy<Task<IReadOnlyList<InstallManifest>>>(LoadEpicManifestsAsync);
    }

    public Task<GameNameResult> ResolveAsync(string executablePath, CancellationToken cancellationToken = default) =>
        Task.Run(() => ResolveCoreAsync(executablePath, cancellationToken), cancellationToken);

    private async Task<GameNameResult> ResolveCoreAsync(string executablePath, CancellationToken cancellationToken)
    {
        string fullPath;
        try
        {
            fullPath = Path.GetFullPath(executablePath);
        }
        catch
        {
            fullPath = executablePath;
        }

        cancellationToken.ThrowIfCancellationRequested();

        GameNameResult? result = TryResolveSteam(fullPath);
        if (result is not null)
        {
            return result;
        }

        result = await TryResolveEpicAsync(fullPath, cancellationToken);
        if (result is not null)
        {
            return result;
        }

        string executableStem = Path.GetFileNameWithoutExtension(fullPath);
        if (KnownExecutables.TryGetValue(executableStem, out string? knownName))
        {
            return new GameNameResult(knownName, GameNameSource.KnownExecutable);
        }

        result = TryResolveExecutableMetadata(fullPath);
        if (result is not null)
        {
            return result;
        }

        result = TryResolveInstallationFolder(fullPath);
        if (result is not null)
        {
            return result;
        }

        string fallback = CleanCandidate(executableStem);
        return new GameNameResult(
            string.IsNullOrWhiteSpace(fallback) ? Path.GetFileName(fullPath) : fallback,
            GameNameSource.ExecutableFileName);
    }

    private static GameNameResult? TryResolveSteam(string executablePath)
    {
        DirectoryInfo? directory;
        try
        {
            directory = new FileInfo(executablePath).Directory;
        }
        catch
        {
            return null;
        }

        DirectoryInfo? installDirectory = null;
        DirectoryInfo? steamAppsDirectory = null;
        while (directory is not null)
        {
            if (directory.Parent is not null &&
                directory.Parent.Name.Equals("common", StringComparison.OrdinalIgnoreCase) &&
                directory.Parent.Parent is not null &&
                directory.Parent.Parent.Name.Equals("steamapps", StringComparison.OrdinalIgnoreCase))
            {
                installDirectory = directory;
                steamAppsDirectory = directory.Parent.Parent;
                break;
            }

            directory = directory.Parent;
        }

        if (installDirectory is null || steamAppsDirectory is null)
        {
            return null;
        }

        try
        {
            foreach (string manifestPath in Directory.EnumerateFiles(
                         steamAppsDirectory.FullName,
                         "appmanifest_*.acf",
                         SearchOption.TopDirectoryOnly))
            {
                string content = File.ReadAllText(manifestPath);
                string? manifestInstallDirectory = ReadVdfValue(content, "installdir");
                if (!string.Equals(manifestInstallDirectory, installDirectory.Name, StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                string? name = ReadVdfValue(content, "name");
                if (IsUsefulCandidate(name, executablePath))
                {
                    return new GameNameResult(NormalizeDisplayName(name!), GameNameSource.SteamManifest);
                }
            }
        }
        catch
        {
            // A locked or malformed launcher manifest should never block adding a profile.
        }

        return null;
    }

    private async Task<GameNameResult?> TryResolveEpicAsync(string executablePath, CancellationToken cancellationToken)
    {
        IReadOnlyList<InstallManifest> manifests;
        try
        {
            manifests = await _epicManifests.Value;
        }
        catch
        {
            return null;
        }

        foreach (InstallManifest manifest in manifests)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (IsPathInside(executablePath, manifest.InstallLocation) && IsUsefulCandidate(manifest.DisplayName, executablePath))
            {
                return new GameNameResult(NormalizeDisplayName(manifest.DisplayName), GameNameSource.EpicManifest);
            }
        }

        return null;
    }

    private static GameNameResult? TryResolveExecutableMetadata(string executablePath)
    {
        try
        {
            FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(executablePath);
            if (IsUsefulCandidate(versionInfo.ProductName, executablePath))
            {
                return new GameNameResult(
                    NormalizeDisplayName(versionInfo.ProductName!),
                    GameNameSource.ExecutableProductName);
            }

            if (IsUsefulCandidate(versionInfo.FileDescription, executablePath))
            {
                return new GameNameResult(
                    NormalizeDisplayName(versionInfo.FileDescription!),
                    GameNameSource.ExecutableDescription);
            }
        }
        catch
        {
            // Some protected executables do not expose version resources.
        }

        return null;
    }

    private static GameNameResult? TryResolveInstallationFolder(string executablePath)
    {
        DirectoryInfo? directory;
        try
        {
            directory = new FileInfo(executablePath).Directory;
        }
        catch
        {
            return null;
        }

        // Steam's folder directly below steamapps\common is normally the
        // user-facing installation name and is more reliable than nested engine
        // folders such as Binaries, Win64, or TslGame.
        DirectoryInfo? cursor = directory;
        while (cursor is not null)
        {
            if (cursor.Parent is not null &&
                cursor.Parent.Name.Equals("common", StringComparison.OrdinalIgnoreCase) &&
                cursor.Parent.Parent is not null &&
                cursor.Parent.Parent.Name.Equals("steamapps", StringComparison.OrdinalIgnoreCase))
            {
                string steamName = CleanCandidate(cursor.Name);
                if (IsUsefulCandidate(steamName, executablePath))
                {
                    return new GameNameResult(steamName, GameNameSource.InstallationFolder);
                }
                break;
            }
            cursor = cursor.Parent;
        }

        while (directory is not null)
        {
            string candidate = directory.Name;
            if (!IsGenericFolder(candidate))
            {
                string cleaned = CleanCandidate(candidate);
                if (IsUsefulCandidate(cleaned, executablePath))
                {
                    return new GameNameResult(cleaned, GameNameSource.InstallationFolder);
                }
            }

            directory = directory.Parent;
        }

        return null;
    }

    private static async Task<IReadOnlyList<InstallManifest>> LoadEpicManifestsAsync()
    {
        var manifests = new List<InstallManifest>();
        string programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
        string manifestDirectory = Path.Combine(
            programData,
            "Epic",
            "EpicGamesLauncher",
            "Data",
            "Manifests");

        if (!Directory.Exists(manifestDirectory))
        {
            return manifests;
        }

        foreach (string path in Directory.EnumerateFiles(manifestDirectory, "*.item", SearchOption.TopDirectoryOnly))
        {
            try
            {
                await using FileStream stream = File.OpenRead(path);
                using JsonDocument document = await JsonDocument.ParseAsync(stream);
                JsonElement root = document.RootElement;
                if (!root.TryGetProperty("DisplayName", out JsonElement displayNameElement) ||
                    !root.TryGetProperty("InstallLocation", out JsonElement locationElement))
                {
                    continue;
                }

                string? displayName = displayNameElement.GetString();
                string? installLocation = locationElement.GetString();
                if (!string.IsNullOrWhiteSpace(displayName) && !string.IsNullOrWhiteSpace(installLocation))
                {
                    manifests.Add(new InstallManifest(displayName, installLocation));
                }
            }
            catch
            {
                // Ignore individual malformed manifests.
            }
        }

        return manifests;
    }

    private static string? ReadVdfValue(string content, string key)
    {
        Match match = Regex.Match(
            content,
            $"\\\"{Regex.Escape(key)}\\\"\\s*\\\"(?<value>[^\\\"]*)\\\"",
            RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        return match.Success ? match.Groups["value"].Value : null;
    }

    private static bool IsPathInside(string filePath, string directoryPath)
    {
        try
        {
            string normalizedFile = Path.GetFullPath(filePath);
            string normalizedDirectory = Path.GetFullPath(directoryPath)
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
            return normalizedFile.StartsWith(normalizedDirectory, StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    private static bool IsUsefulCandidate(string? value, string executablePath)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        string candidate = NormalizeDisplayName(value);
        if (candidate.Length < 2)
        {
            return false;
        }

        string normalized = candidate.ToLowerInvariant();
        if (GenericNames().IsMatch(normalized))
        {
            return false;
        }

        string executableStem = CleanCandidate(Path.GetFileNameWithoutExtension(executablePath));
        return !string.Equals(candidate, "Microsoft Visual C++", StringComparison.OrdinalIgnoreCase) &&
               !string.Equals(candidate, "Unreal Engine", StringComparison.OrdinalIgnoreCase) &&
               !(normalized.EndsWith(" launcher", StringComparison.Ordinal) &&
                 string.Equals(CleanCandidate(candidate[..^9]), executableStem, StringComparison.OrdinalIgnoreCase));
    }

    private static bool IsGenericFolder(string value)
    {
        string normalized = value.Trim().ToLowerInvariant();
        return normalized.Length == 0 || GenericFolders().IsMatch(normalized);
    }

    private static string CleanCandidate(string value)
    {
        string output = Path.GetFileNameWithoutExtension(value).Trim();
        output = CommonExecutableSuffixes().Replace(output, string.Empty);
        output = Separators().Replace(output, " ");
        output = PascalBoundary().Replace(output, "$1 $2");
        output = LetterDigitBoundary().Replace(output, "$1 $2");
        output = DigitLetterBoundary().Replace(output, "$1 $2");
        output = Whitespace().Replace(output, " ").Trim(' ', '-', '_', '.');
        return NormalizeDisplayName(output);
    }

    private static string NormalizeDisplayName(string value)
    {
        string output = Whitespace().Replace(value.Trim(), " ");
        output = output.Replace("®", string.Empty, StringComparison.Ordinal)
            .Replace("™", string.Empty, StringComparison.Ordinal)
            .Trim();
        return output;
    }

    private sealed record InstallManifest(string DisplayName, string InstallLocation);

    [GeneratedRegex(@"^(game|game client|launcher|application|shipping executable|win64 shipping|win32 shipping|client|bootstrapper|setup|update|updater|crash reporter|crashreportclient|easy anti-cheat|battleye|unreal engine|unity player)$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex GenericNames();

    [GeneratedRegex(@"^(bin|binary|binaries|win32|win64|x86|x64|shipping|client|clients|launcher|launchers|game|games|program files|program files \(x86\)|steamapps|common|epic games|gog galaxy|riot games|live|engine|redist|redistributables|system|system32)$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex GenericFolders();

    [GeneratedRegex(@"(?:[-_. ]?(?:win32|win64|x86|x64)?[-_. ]?shipping|[-_. ]?(?:launcher|client|release|retail|final))+$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex CommonExecutableSuffixes();

    [GeneratedRegex(@"[-_.]+", RegexOptions.CultureInvariant)]
    private static partial Regex Separators();

    [GeneratedRegex(@"([a-z])([A-Z])", RegexOptions.CultureInvariant)]
    private static partial Regex PascalBoundary();

    [GeneratedRegex(@"([A-Za-z])([0-9])", RegexOptions.CultureInvariant)]
    private static partial Regex LetterDigitBoundary();

    [GeneratedRegex(@"([0-9])([A-Za-z])", RegexOptions.CultureInvariant)]
    private static partial Regex DigitLetterBoundary();

    [GeneratedRegex(@"\s+", RegexOptions.CultureInvariant)]
    private static partial Regex Whitespace();
}
