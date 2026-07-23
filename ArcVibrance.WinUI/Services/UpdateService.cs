using System.Net;
using System.Net.Http.Headers;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

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

        Uri? downloadUri = asset is null
            ? null
            : GetTrustedRepositoryUri(asset.DownloadUrl, "release download");

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
            AssetName: asset?.Name,
            AssetSizeBytes: asset?.Size ?? 0,
            release.PublishedAt);
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
            Timeout = TimeSpan.FromSeconds(30)
        };
        client.DefaultRequestHeaders.UserAgent.Add(
            new ProductInfoHeaderValue("ArcVibrance", CurrentVersionTag.TrimStart('v', 'V')));
        return client;
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
}

public sealed record UpdateCheckResult(
    bool IsUpdateAvailable,
    string CurrentVersionTag,
    string LatestVersionTag,
    string ReleaseName,
    string ReleaseNotes,
    Uri ReleaseUri,
    Uri? DownloadUri,
    string? AssetName,
    long AssetSizeBytes,
    DateTimeOffset? PublishedAt);
