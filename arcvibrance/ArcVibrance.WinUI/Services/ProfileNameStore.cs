using System.Text.Json;

namespace ArcVibrance.Services;

public sealed class ProfileNameStore
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true
    };

    public string NamesPath { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "ArcVibrance",
        "profile-names.json");

    public async Task<IReadOnlyDictionary<string, string>> LoadAsync(CancellationToken cancellationToken = default)
    {
        if (!File.Exists(NamesPath))
        {
            return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        }

        try
        {
            await using FileStream stream = File.OpenRead(NamesPath);
            Dictionary<string, string>? stored = await JsonSerializer.DeserializeAsync<Dictionary<string, string>>(
                stream,
                JsonOptions,
                cancellationToken);
            return stored is null
                ? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                : new Dictionary<string, string>(stored, StringComparer.OrdinalIgnoreCase);
        }
        catch
        {
            // A damaged optional display-name cache must not prevent profiles loading.
            return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        }
    }

    public async Task SaveAsync(
        IEnumerable<KeyValuePair<string, string>> customNames,
        CancellationToken cancellationToken = default)
    {
        string? directory = Path.GetDirectoryName(NamesPath);
        if (directory is null)
        {
            return;
        }

        Directory.CreateDirectory(directory);
        var output = new SortedDictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (KeyValuePair<string, string> pair in customNames)
        {
            if (!string.IsNullOrWhiteSpace(pair.Key) && !string.IsNullOrWhiteSpace(pair.Value))
            {
                output[pair.Key] = pair.Value.Trim();
            }
        }

        string temporaryPath = NamesPath + ".tmp";
        await using (FileStream stream = File.Create(temporaryPath))
        {
            await JsonSerializer.SerializeAsync(stream, output, JsonOptions, cancellationToken);
        }
        File.Move(temporaryPath, NamesPath, true);
    }
}
