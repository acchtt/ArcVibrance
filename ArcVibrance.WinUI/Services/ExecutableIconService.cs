using System.Runtime.InteropServices;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.FileProperties;

namespace ArcVibrance.Services;

public sealed class ExecutableIconService
{
    private const int RequestedIconSize = 96;
    private readonly Dictionary<string, ImageSource> _cache = new(StringComparer.OrdinalIgnoreCase);
    private readonly object _cacheLock = new();

    public async Task<ImageSource?> LoadAsync(string executablePath)
    {
        if (string.IsNullOrWhiteSpace(executablePath))
        {
            return null;
        }

        string normalizedPath;
        try
        {
            normalizedPath = Path.GetFullPath(executablePath);
        }
        catch
        {
            return null;
        }

        lock (_cacheLock)
        {
            if (_cache.TryGetValue(normalizedPath, out ImageSource? cached))
            {
                return cached;
            }
        }

        // The Storage thumbnail provider produces the best quality result, but it
        // can transiently return no thumbnail while Explorer's cache is warming.
        // Retry once, then fall back to extracting the executable's icon directly.
        ImageSource? image = await TryLoadStorageThumbnailAsync(normalizedPath);
        if (image is null)
        {
            byte[]? pixels = await Task.Run(() => TryExtractIconPixels(normalizedPath, RequestedIconSize));
            if (pixels is not null)
            {
                try
                {
                    image = await CreateImageSourceAsync(pixels, RequestedIconSize, RequestedIconSize);
                }
                catch
                {
                    image = null;
                }
            }
        }

        if (image is not null)
        {
            lock (_cacheLock)
            {
                _cache[normalizedPath] = image;
            }
        }

        return image;
    }

    private static async Task<ImageSource?> TryLoadStorageThumbnailAsync(string executablePath)
    {
        for (int attempt = 0; attempt < 2; attempt++)
        {
            try
            {
                StorageFile file = await StorageFile.GetFileFromPathAsync(executablePath);
                using StorageItemThumbnail thumbnail = await file.GetThumbnailAsync(
                    ThumbnailMode.SingleItem,
                    RequestedIconSize,
                    ThumbnailOptions.ResizeThumbnail);

                if (thumbnail is not null && thumbnail.Size > 0)
                {
                    var image = new BitmapImage
                    {
                        DecodePixelWidth = RequestedIconSize,
                        DecodePixelHeight = RequestedIconSize
                    };
                    await image.SetSourceAsync(thumbnail);
                    return image;
                }
            }
            catch when (attempt == 0)
            {
                await Task.Delay(90);
            }
            catch
            {
                return null;
            }
        }

        return null;
    }

    private static byte[]? TryExtractIconPixels(string executablePath, int size)
    {
        if (!File.Exists(executablePath))
        {
            return null;
        }

        IntPtr icon = IntPtr.Zero;
        try
        {
            uint iconId;
            uint extracted = PrivateExtractIcons(
                executablePath,
                0,
                size,
                size,
                out icon,
                out iconId,
                1,
                0);

            if (extracted == 0 || icon == IntPtr.Zero)
            {
                if (icon != IntPtr.Zero)
                {
                    DestroyIcon(icon);
                    icon = IntPtr.Zero;
                }

                var fileInfo = new ShellFileInfo();
                IntPtr result = SHGetFileInfo(
                    executablePath,
                    0,
                    out fileInfo,
                    (uint)Marshal.SizeOf<ShellFileInfo>(),
                    ShellGetFileInfoFlags.Icon | ShellGetFileInfoFlags.LargeIcon);
                if (result == IntPtr.Zero || fileInfo.Icon == IntPtr.Zero)
                {
                    return null;
                }

                icon = fileInfo.Icon;
            }

            return RenderIcon(icon, size, size);
        }
        catch
        {
            return null;
        }
        finally
        {
            if (icon != IntPtr.Zero)
            {
                DestroyIcon(icon);
            }
        }
    }

    private static byte[]? RenderIcon(IntPtr icon, int width, int height)
    {
        IntPtr deviceContext = CreateCompatibleDC(IntPtr.Zero);
        if (deviceContext == IntPtr.Zero)
        {
            return null;
        }

        IntPtr bitmap = IntPtr.Zero;
        IntPtr oldBitmap = IntPtr.Zero;
        try
        {
            var info = new BitmapInfo
            {
                Header = new BitmapInfoHeader
                {
                    Size = (uint)Marshal.SizeOf<BitmapInfoHeader>(),
                    Width = width,
                    Height = -height,
                    Planes = 1,
                    BitCount = 32,
                    Compression = 0,
                    SizeImage = (uint)(width * height * 4)
                }
            };

            bitmap = CreateDIBSection(
                deviceContext,
                ref info,
                0,
                out IntPtr bits,
                IntPtr.Zero,
                0);
            if (bitmap == IntPtr.Zero || bits == IntPtr.Zero)
            {
                return null;
            }

            oldBitmap = SelectObject(deviceContext, bitmap);
            byte[] pixels = new byte[width * height * 4];
            Marshal.Copy(pixels, 0, bits, pixels.Length);

            if (!DrawIconEx(
                    deviceContext,
                    0,
                    0,
                    icon,
                    width,
                    height,
                    0,
                    IntPtr.Zero,
                    0x0003))
            {
                return null;
            }

            Marshal.Copy(bits, pixels, 0, pixels.Length);
            RestoreMaskAlphaWhenNeeded(icon, pixels, width, height);
            return pixels;
        }
        finally
        {
            if (oldBitmap != IntPtr.Zero)
            {
                SelectObject(deviceContext, oldBitmap);
            }
            if (bitmap != IntPtr.Zero)
            {
                DeleteObject(bitmap);
            }
            DeleteDC(deviceContext);
        }
    }

    private static void RestoreMaskAlphaWhenNeeded(IntPtr icon, byte[] pixels, int width, int height)
    {
        bool hasAlpha = false;
        for (int index = 3; index < pixels.Length; index += 4)
        {
            if (pixels[index] != 0)
            {
                hasAlpha = true;
                break;
            }
        }

        if (hasAlpha || !GetIconInfo(icon, out IconInfo iconInfo))
        {
            return;
        }

        IntPtr maskDc = IntPtr.Zero;
        IntPtr oldMask = IntPtr.Zero;
        try
        {
            if (iconInfo.MaskBitmap == IntPtr.Zero)
            {
                return;
            }

            maskDc = CreateCompatibleDC(IntPtr.Zero);
            if (maskDc == IntPtr.Zero)
            {
                return;
            }

            oldMask = SelectObject(maskDc, iconInfo.MaskBitmap);
            int maskWidth = width;
            int maskHeight = height;
            if (GetObject(iconInfo.MaskBitmap, Marshal.SizeOf<NativeBitmap>(), out NativeBitmap maskBitmap) > 0)
            {
                maskWidth = Math.Max(1, maskBitmap.Width);
                maskHeight = Math.Max(1, iconInfo.ColorBitmap == IntPtr.Zero
                    ? maskBitmap.Height / 2
                    : maskBitmap.Height);
            }

            for (int y = 0; y < height; y++)
            {
                int maskY = Math.Min(maskHeight - 1, y * maskHeight / height);
                for (int x = 0; x < width; x++)
                {
                    int maskX = Math.Min(maskWidth - 1, x * maskWidth / width);
                    uint maskPixel = GetPixel(maskDc, maskX, maskY);
                    bool transparent = maskPixel != 0xFFFFFFFF &&
                                       (maskPixel & 0x00FFFFFF) == 0x00FFFFFF;
                    pixels[((y * width + x) * 4) + 3] = transparent ? (byte)0 : (byte)255;
                }
            }
        }
        finally
        {
            if (oldMask != IntPtr.Zero && maskDc != IntPtr.Zero)
            {
                SelectObject(maskDc, oldMask);
            }
            if (maskDc != IntPtr.Zero)
            {
                DeleteDC(maskDc);
            }
            if (iconInfo.ColorBitmap != IntPtr.Zero)
            {
                DeleteObject(iconInfo.ColorBitmap);
            }
            if (iconInfo.MaskBitmap != IntPtr.Zero)
            {
                DeleteObject(iconInfo.MaskBitmap);
            }
        }
    }

    private static async Task<ImageSource> CreateImageSourceAsync(byte[] pixels, int width, int height)
    {
        using var bitmap = new SoftwareBitmap(
            BitmapPixelFormat.Bgra8,
            width,
            height,
            BitmapAlphaMode.Premultiplied);

        using (BitmapBuffer buffer = bitmap.LockBuffer(BitmapBufferAccessMode.Write))
        using (IMemoryBufferReference reference = buffer.CreateReference())
        {
            ((IMemoryBufferByteAccess)reference).GetBuffer(out IntPtr destination, out uint capacity);
            if (destination == IntPtr.Zero || capacity < (uint)pixels.Length)
            {
                throw new InvalidOperationException("Could not allocate the executable icon bitmap buffer.");
            }
            Marshal.Copy(pixels, 0, destination, pixels.Length);
        }

        var source = new SoftwareBitmapSource();
        await source.SetBitmapAsync(bitmap);
        return source;
    }

    [ComImport]
    [Guid("5B0D3235-4DBA-4D44-865B-1D289D0E3F88")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMemoryBufferByteAccess
    {
        void GetBuffer(out IntPtr buffer, out uint capacity);
    }

    [Flags]
    private enum ShellGetFileInfoFlags : uint
    {
        Icon = 0x00000100,
        LargeIcon = 0x00000000
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct ShellFileInfo
    {
        public IntPtr Icon;
        public int IconIndex;
        public uint Attributes;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string DisplayName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)] public string TypeName;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BitmapInfoHeader
    {
        public uint Size;
        public int Width;
        public int Height;
        public ushort Planes;
        public ushort BitCount;
        public uint Compression;
        public uint SizeImage;
        public int XPelsPerMeter;
        public int YPelsPerMeter;
        public uint ColorsUsed;
        public uint ColorsImportant;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BitmapInfo
    {
        public BitmapInfoHeader Header;
        public uint Colors;
    }


    [StructLayout(LayoutKind.Sequential)]
    private struct NativeBitmap
    {
        public int Type;
        public int Width;
        public int Height;
        public int WidthBytes;
        public ushort Planes;
        public ushort BitsPixel;
        public IntPtr Bits;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IconInfo
    {
        [MarshalAs(UnmanagedType.Bool)] public bool IsIcon;
        public uint XHotspot;
        public uint YHotspot;
        public IntPtr MaskBitmap;
        public IntPtr ColorBitmap;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "PrivateExtractIconsW", SetLastError = true)]
    private static extern uint PrivateExtractIcons(
        string fileName,
        int iconIndex,
        int iconWidth,
        int iconHeight,
        out IntPtr icon,
        out uint iconId,
        uint iconCount,
        uint flags);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, EntryPoint = "SHGetFileInfoW")]
    private static extern IntPtr SHGetFileInfo(
        string path,
        uint fileAttributes,
        out ShellFileInfo fileInfo,
        uint fileInfoSize,
        ShellGetFileInfoFlags flags);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyIcon(IntPtr icon);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DrawIconEx(
        IntPtr deviceContext,
        int x,
        int y,
        IntPtr icon,
        int width,
        int height,
        uint step,
        IntPtr flickerFreeBrush,
        uint flags);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetIconInfo(IntPtr icon, out IconInfo iconInfo);

    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateCompatibleDC(IntPtr deviceContext);

    [DllImport("gdi32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DeleteDC(IntPtr deviceContext);

    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateDIBSection(
        IntPtr deviceContext,
        ref BitmapInfo bitmapInfo,
        uint usage,
        out IntPtr bits,
        IntPtr section,
        uint offset);

    [DllImport("gdi32.dll")]
    private static extern IntPtr SelectObject(IntPtr deviceContext, IntPtr graphicsObject);

    [DllImport("gdi32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DeleteObject(IntPtr graphicsObject);

    [DllImport("gdi32.dll", EntryPoint = "GetObjectW")]
    private static extern int GetObject(IntPtr graphicsObject, int bufferSize, out NativeBitmap bitmap);

    [DllImport("gdi32.dll")]
    private static extern uint GetPixel(IntPtr deviceContext, int x, int y);
}
