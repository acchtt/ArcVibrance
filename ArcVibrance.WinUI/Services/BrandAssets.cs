using System.Reflection;

namespace ArcVibrance.Services;

internal static class BrandAssets
{
    private const string IconResourceName = "ArcVibrance.Brand.ArcVibrance.ico";
    private const string LogoResourceName = "ArcVibrance.Brand.ArcVibrance.png";

    private static readonly object Sync = new();
    private static bool _ready;

    public static string IconPath
    {
        get
        {
            EnsureExtracted();
            return Path.Combine(GetBrandDirectory(), "ArcVibrance.ico");
        }
    }

    public static string LogoPath
    {
        get
        {
            EnsureExtracted();
            return Path.Combine(GetBrandDirectory(), "ArcVibrance.png");
        }
    }

    public static void EnsureExtracted()
    {
        lock (Sync)
        {
            if (_ready)
            {
                return;
            }

            string directory = GetBrandDirectory();
            Directory.CreateDirectory(directory);
            Assembly assembly = typeof(BrandAssets).Assembly;

            ExtractResource(
                assembly,
                IconResourceName,
                Path.Combine(directory, "ArcVibrance.ico"));
            ExtractResource(
                assembly,
                LogoResourceName,
                Path.Combine(directory, "ArcVibrance.png"));

            _ready = true;
        }
    }

    private static string GetBrandDirectory() =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "ArcVibrance",
            "Brand",
            UpdateService.CurrentVersionTag);

    private static void ExtractResource(
        Assembly assembly,
        string resourceName,
        string destinationPath)
    {
        using Stream source = assembly.GetManifestResourceStream(resourceName)
            ?? throw new InvalidOperationException(
                $"ArcVibrance is missing the embedded resource '{resourceName}'.");

        string temporaryPath = $"{destinationPath}.{Environment.ProcessId}.tmp";
        try
        {
            using (FileStream destination = new(
                       temporaryPath,
                       FileMode.Create,
                       FileAccess.Write,
                       FileShare.None))
            {
                source.CopyTo(destination);
                destination.Flush(flushToDisk: true);
            }

            File.Move(temporaryPath, destinationPath, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporaryPath))
            {
                File.Delete(temporaryPath);
            }
        }
    }
}
