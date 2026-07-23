using ArcVibrance.Models;
using ArcVibrance.Services;
using ArcVibrance.ViewModels;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Windows.ApplicationModel.DataTransfer;
using Windows.Storage;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace ArcVibrance;

public sealed partial class MainWindow : Window
{
    private static readonly Uri WebsiteUri = new("https://acchtt.github.io/ArcVibrance/");
    private static readonly Uri RepositoryUri = new("https://github.com/acchtt/ArcVibrance");
    private static readonly Uri ReleasesUri = new("https://github.com/acchtt/ArcVibrance/releases");

    private readonly MainViewModel _viewModel = new();
    private readonly UpdateService _updateService = new();
    private readonly UiCloseSignal _closeSignal = new();
    private readonly DispatcherTimer _statusTimer = new() { Interval = TimeSpan.FromSeconds(2) };
    private AppWindow? _appWindow;
    private UpdateCheckResult? _availableUpdate;
    private bool _forceClose;
    private bool _initialized;
    private bool _hideToTrayInProgress;
    private bool _syncingSaturation;
    private bool _updateCheckInProgress;

    public MainWindow()
    {
        InitializeComponent();
        FooterVersionText.Text =
            $"Version {UpdateService.CurrentVersionTag}  •  WinUI 3  •  .NET 8";
        AboutVersionText.Text =
            $"Version {UpdateService.CurrentVersionTag} • Windows App SDK 2.2 • .NET 8";
        Root.DataContext = _viewModel;
        Root.ActualThemeChanged += Root_ActualThemeChanged;
        _viewModel.Profiles.CollectionChanged += (_, args) =>
        {
            if (args.NewItems is null)
            {
                return;
            }

            bool useLightTheme = Root.ActualTheme == ElementTheme.Light;
            foreach (ProfileViewModel profile in args.NewItems)
            {
                profile.SetLightTheme(useLightTheme);
            }
        };

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        ConfigureWindow();

        _statusTimer.Tick += StatusTimer_Tick;
        _closeSignal.Start(() => DispatcherQueue.TryEnqueue(() =>
        {
            _forceClose = true;
            Close();
        }));

        Activated += MainWindow_Activated;
        Closed += MainWindow_Closed;
    }

    public void ShowAndActivate()
    {
        _hideToTrayInProgress = false;
        NativeWindow.ShowAndActivate(this);
    }

    private void ConfigureWindow()
    {
        _appWindow = NativeWindow.GetAppWindow(this);
        _appWindow.Resize(new Windows.Graphics.SizeInt32(1280, 820));
        if (_appWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.IsResizable = true;
            presenter.IsMaximizable = true;
            presenter.IsMinimizable = true;
        }
        _appWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "Assets", "ArcVibrance.ico"));
        _appWindow.Closing += AppWindow_Closing;

        UpdateTitleBarColors(Root.ActualTheme);
    }

    private async void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
    {
        if (_initialized)
        {
            return;
        }

        _initialized = true;
        await _viewModel.InitializeAsync();
        ApplyTheme(_viewModel.ThemeMode);
        ThemeCombo.SelectedIndex = (int)_viewModel.ThemeMode;
        MinimizeRadio.IsChecked = _viewModel.CloseBehavior == CloseBehavior.MinimizeToTray;
        ExitRadio.IsChecked = _viewModel.CloseBehavior == CloseBehavior.ExitCompletely;
        SyncSaturationEditor();
        ShowPage(ProfilesPage, ProfilesNav);
        _statusTimer.Start();
        _ = CheckForUpdatesAsync(showResult: false);
    }

    private void AppWindow_Closing(AppWindow sender, AppWindowClosingEventArgs args)
    {
        if (_forceClose || _viewModel.CloseBehavior != CloseBehavior.MinimizeToTray)
        {
            return;
        }

        args.Cancel = true;
        if (!_hideToTrayInProgress)
        {
            _ = HideToTrayAsync();
        }
    }

    private async Task HideToTrayAsync()
    {
        _hideToTrayInProgress = true;
        try
        {
            // Never hide the only visible window unless the tray process is alive;
            // otherwise there would be no reliable way to reopen the UI.
            if (!await _viewModel.EnsureAgentRunningAsync())
            {
                ShowAndActivate();
                return;
            }

            NativeWindow.Hide(this);
        }
        finally
        {
            _hideToTrayInProgress = false;
        }
    }

    private void MainWindow_Closed(object sender, WindowEventArgs args)
    {
        _statusTimer.Stop();
        _closeSignal.Dispose();
    }

    private void ShowPage(FrameworkElement page, Button selectedButton)
    {
        ProfilesPage.Visibility = page == ProfilesPage ? Visibility.Visible : Visibility.Collapsed;
        SettingsPage.Visibility = page == SettingsPage ? Visibility.Visible : Visibility.Collapsed;
        AboutPage.Visibility = page == AboutPage ? Visibility.Visible : Visibility.Collapsed;

        UpdateNavigationButton(ProfilesNav, ProfilesNavChrome, ProfilesNavText, selectedButton == ProfilesNav);
        UpdateNavigationButton(SettingsNav, SettingsNavChrome, SettingsNavText, selectedButton == SettingsNav);
        UpdateNavigationButton(AboutNav, AboutNavChrome, AboutNavText, selectedButton == AboutNav);
    }

    private void UpdateNavigationButton(Button button, Border chrome, TextBlock label, bool isActive)
    {
        // Persistent selection is painted with a fill only. Keeping both the
        // Button and its content chrome borderless avoids a hard outline in
        // the left navigation while preserving the active-page highlight.
        button.Background = new SolidColorBrush(Colors.Transparent);
        chrome.Background = isActive
            ? (Brush)Application.Current.Resources["NavActiveBrush"]
            : new SolidColorBrush(Colors.Transparent);
        chrome.BorderBrush = new SolidColorBrush(Colors.Transparent);
        chrome.BorderThickness = new Thickness(0);
        bool useLightTheme = Root.ActualTheme == ElementTheme.Light;
        label.Foreground = new SolidColorBrush(isActive
            ? Windows.UI.Color.FromArgb(255, 245, 247, 251)
            : useLightTheme
                ? Windows.UI.Color.FromArgb(255, 107, 120, 144)
                : Windows.UI.Color.FromArgb(255, 167, 177, 196));
        label.FontWeight = isActive
            ? Microsoft.UI.Text.FontWeights.SemiBold
            : Microsoft.UI.Text.FontWeights.Normal;
    }

    private void ProfilesNav_Click(object sender, RoutedEventArgs e) => ShowPage(ProfilesPage, ProfilesNav);
    private void SettingsNav_Click(object sender, RoutedEventArgs e) => ShowPage(SettingsPage, SettingsNav);
    private void AboutNav_Click(object sender, RoutedEventArgs e) => ShowPage(AboutPage, AboutNav);
    private async void FooterVisitWebsite_Click(object sender, RoutedEventArgs e) =>
        await Windows.System.Launcher.LaunchUriAsync(WebsiteUri);

    private async void FooterVisitGitHub_Click(object sender, RoutedEventArgs e) =>
        await Windows.System.Launcher.LaunchUriAsync(RepositoryUri);

    private async void FooterCheckUpdates_Click(object sender, RoutedEventArgs e) =>
        await CheckForUpdatesAsync(showResult: true);

    private async Task CheckForUpdatesAsync(bool showResult)
    {
        if (_updateCheckInProgress)
        {
            return;
        }

        _updateCheckInProgress = true;
        UpdatesButton.IsEnabled = false;
        UpdatesButtonText.Text = "Checking...";

        try
        {
            UpdateCheckResult result = await _updateService.CheckForUpdatesAsync();
            _availableUpdate = result.IsUpdateAvailable ? result : null;

            UpdatesButtonText.Text = result.IsUpdateAvailable
                ? $"Update {result.LatestVersionTag}"
                : "Up to date";

            if (showResult)
            {
                if (result.IsUpdateAvailable)
                {
                    await ShowUpdateAvailableDialogAsync(result);
                }
                else
                {
                    await ShowUpToDateDialogAsync(result);
                }
            }
        }
        catch (Exception exception)
        {
            UpdatesButtonText.Text = _availableUpdate is null
                ? "Updates"
                : $"Update {_availableUpdate.LatestVersionTag}";

            if (showResult)
            {
                await ShowUpdateErrorDialogAsync(exception);
            }
        }
        finally
        {
            _updateCheckInProgress = false;
            UpdatesButton.IsEnabled = true;
        }
    }

    private async Task ShowUpdateAvailableDialogAsync(UpdateCheckResult update)
    {
        var content = new StackPanel
        {
            Spacing = 12,
            MinWidth = 430
        };

        content.Children.Add(new TextBlock
        {
            Text = $"{update.CurrentVersionTag}  →  {update.LatestVersionTag}",
            FontSize = 18,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            Foreground = (Brush)Application.Current.Resources["CyanBrush"]
        });

        string releaseDetails = update.PublishedAt is DateTimeOffset publishedAt
            ? $"Published {publishedAt.ToLocalTime():d}"
            : "Latest stable GitHub release";

        if (update.AssetSizeBytes > 0)
        {
            releaseDetails += $"  •  {FormatFileSize(update.AssetSizeBytes)}";
        }

        content.Children.Add(new TextBlock
        {
            Text = releaseDetails,
            Foreground = (Brush)Application.Current.Resources["TextMutedBrush"]
        });

        if (!string.IsNullOrWhiteSpace(update.ReleaseNotes))
        {
            content.Children.Add(new ScrollViewer
            {
                MaxHeight = 230,
                VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
                Content = new TextBlock
                {
                    Text = update.ReleaseNotes,
                    TextWrapping = TextWrapping.Wrap,
                    Foreground = (Brush)Application.Current.Resources["TextSecondaryBrush"]
                }
            });
        }

        bool hasDirectDownload = update.DownloadUri is not null;
        var dialog = new ContentDialog
        {
            XamlRoot = Root.XamlRoot,
            Title = $"{update.ReleaseName} is available",
            Content = content,
            PrimaryButtonText = hasDirectDownload ? "Download update" : "View release",
            SecondaryButtonText = hasDirectDownload ? "View release" : string.Empty,
            CloseButtonText = "Later",
            DefaultButton = ContentDialogButton.Primary
        };

        ContentDialogResult result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            await Windows.System.Launcher.LaunchUriAsync(
                update.DownloadUri ?? update.ReleaseUri);
        }
        else if (result == ContentDialogResult.Secondary)
        {
            await Windows.System.Launcher.LaunchUriAsync(update.ReleaseUri);
        }
    }

    private async Task ShowUpToDateDialogAsync(UpdateCheckResult update)
    {
        var dialog = new ContentDialog
        {
            XamlRoot = Root.XamlRoot,
            Title = "ArcVibrance is up to date",
            Content =
                $"You are running {update.CurrentVersionTag}. " +
                $"The latest stable release is {update.LatestVersionTag}.",
            PrimaryButtonText = "View releases",
            CloseButtonText = "OK",
            DefaultButton = ContentDialogButton.Close
        };

        if (await dialog.ShowAsync() == ContentDialogResult.Primary)
        {
            await Windows.System.Launcher.LaunchUriAsync(update.ReleaseUri);
        }
    }

    private async Task ShowUpdateErrorDialogAsync(Exception exception)
    {
        string message = exception is HttpRequestException
            ? "ArcVibrance could not reach GitHub. Check your internet connection and try again."
            : exception.Message;

        var dialog = new ContentDialog
        {
            XamlRoot = Root.XamlRoot,
            Title = "Could not check for updates",
            Content = message,
            PrimaryButtonText = "View releases",
            CloseButtonText = "Close",
            DefaultButton = ContentDialogButton.Close
        };

        if (await dialog.ShowAsync() == ContentDialogResult.Primary)
        {
            await Windows.System.Launcher.LaunchUriAsync(ReleasesUri);
        }
    }

    private static string FormatFileSize(long sizeInBytes)
    {
        const double mebibyte = 1024d * 1024d;
        return sizeInBytes >= mebibyte
            ? $"{sizeInBytes / mebibyte:0.0} MB"
            : $"{sizeInBytes / 1024d:0.0} KB";
    }


    private void GlobalMenu_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button)
        {
            return;
        }

        var flyout = new MenuFlyout();

        var reload = new MenuFlyoutItem
        {
            Text = "Reload profile list",
            Icon = new FontIcon { Glyph = "\uE72C" }
        };
        reload.Click += async (_, _) => await RunActionAsync(_viewModel.ReloadProfilesAsync);

        var redetectNames = new MenuFlyoutItem
        {
            Text = "Detect game names",
            Icon = new FontIcon { Glyph = "\uE8B7" }
        };
        redetectNames.Click += async (_, _) => await RunActionAsync(_viewModel.RedetectAllGameNamesAsync);

        var reapply = new MenuFlyoutItem
        {
            Text = "Reapply active profile",
            Icon = new FontIcon { Glyph = "\uE895" }
        };
        reapply.Click += async (_, _) => await RunActionAsync(_viewModel.ReapplyActiveProfileAsync);

        var restore = new MenuFlyoutItem
        {
            Text = "Restore desktop colors",
            Icon = new FontIcon { Glyph = "\uE790" }
        };
        restore.Click += async (_, _) => await RunActionAsync(_viewModel.RestoreDesktopAsync);

        var openFolder = new MenuFlyoutItem
        {
            Text = "Open profiles folder",
            Icon = new FontIcon { Glyph = "\uE838" }
        };
        openFolder.Click += (_, _) => OpenProfilesFolder();

        flyout.Items.Add(reload);
        flyout.Items.Add(redetectNames);
        flyout.Items.Add(new MenuFlyoutSeparator());
        flyout.Items.Add(reapply);
        flyout.Items.Add(restore);
        flyout.Items.Add(new MenuFlyoutSeparator());
        flyout.Items.Add(openFolder);
        flyout.ShowAt(button);
    }

    private void OpenProfilesFolder()
    {
        try
        {
            string directory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "ArcVibrance");
            Directory.CreateDirectory(directory);
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = "explorer.exe",
                Arguments = $"\"{directory}\"",
                UseShellExecute = true
            });
        }
        catch (Exception exception)
        {
            _viewModel.Notification = $"Could not open the profiles folder: {exception.Message}";
        }
    }

    private async void AddGame_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FileOpenPicker
        {
            SuggestedStartLocation = PickerLocationId.ComputerFolder,
            ViewMode = PickerViewMode.List
        };
        picker.FileTypeFilter.Add(".exe");
        InitializeWithWindow.Initialize(picker, NativeWindow.GetHandle(this));
        IReadOnlyList<StorageFile> files = await picker.PickMultipleFilesAsync();
        await _viewModel.AddExecutablesAsync(files.Select(file => file.Path));
        SyncSaturationEditor();
    }

    private void DropZone_DragOver(object sender, DragEventArgs e)
    {
        e.AcceptedOperation = DataPackageOperation.Copy;
        e.DragUIOverride.Caption = "Add game profile";
        e.DragUIOverride.IsCaptionVisible = true;
    }

    private async void DropZone_Drop(object sender, DragEventArgs e)
    {
        if (!e.DataView.Contains(StandardDataFormats.StorageItems))
        {
            return;
        }

        IReadOnlyList<IStorageItem> items = await e.DataView.GetStorageItemsAsync();
        await _viewModel.AddExecutablesAsync(items.OfType<StorageFile>()
            .Where(file => file.FileType.Equals(".exe", StringComparison.OrdinalIgnoreCase))
            .Select(file => file.Path));
    }

    private void ProfilesList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (ProfilesList.SelectedItem is ProfileViewModel profile)
        {
            _viewModel.SelectedProfile = profile;
            SyncSaturationEditor();
        }
    }

    private async void ProfileToggle_Toggled(object sender, RoutedEventArgs e)
    {
        if (!_initialized || sender is not ToggleSwitch toggle || toggle.Tag is not ProfileViewModel profile)
        {
            return;
        }
        if (profile.Enabled == toggle.IsOn)
        {
            return;
        }
        await _viewModel.ToggleProfileAsync(profile, toggle.IsOn);
    }

    private void ProfileMenu_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button || button.Tag is not ProfileViewModel profile)
        {
            return;
        }

        var flyout = new MenuFlyout();
        var edit = new MenuFlyoutItem { Text = "Edit profile", Icon = new FontIcon { Glyph = "\uE70F" } };
        edit.Click += (_, _) =>
        {
            _viewModel.SelectedProfile = profile;
            SyncSaturationEditor();
        };

        var rename = new MenuFlyoutItem
        {
            Text = "Rename profile",
            Icon = new FontIcon { Glyph = "\uE8AC" }
        };
        rename.Click += async (_, _) => await PromptRenameAsync(profile);

        var redetect = new MenuFlyoutItem
        {
            Text = "Detect game name again",
            Icon = new FontIcon { Glyph = "\uE8B7" }
        };
        redetect.Click += async (_, _) => await RunActionAsync(() => _viewModel.RedetectGameNameAsync(profile));

        var toggle = new MenuFlyoutItem
        {
            Text = profile.Enabled ? "Disable profile" : "Enable profile",
            Icon = new FontIcon { Glyph = "\uE73E" }
        };
        toggle.Click += async (_, _) => await _viewModel.ToggleProfileAsync(profile, !profile.Enabled);

        var remove = new MenuFlyoutItem { Text = "Remove", Icon = new FontIcon { Glyph = "\uE74D" } };
        remove.Click += async (_, _) => await ConfirmRemoveAsync(profile);

        flyout.Items.Add(edit);
        flyout.Items.Add(rename);
        flyout.Items.Add(redetect);
        if (profile.HasCustomName)
        {
            var useDetected = new MenuFlyoutItem
            {
                Text = $"Use detected name: {profile.DetectedName}",
                Icon = new FontIcon { Glyph = "\uE73E" }
            };
            useDetected.Click += async (_, _) => await _viewModel.RenameProfileAsync(profile, null);
            flyout.Items.Add(useDetected);
        }
        flyout.Items.Add(new MenuFlyoutSeparator());
        flyout.Items.Add(toggle);
        flyout.Items.Add(new MenuFlyoutSeparator());
        flyout.Items.Add(remove);
        flyout.ShowAt(button);
    }

    private async Task PromptRenameAsync(ProfileViewModel profile)
    {
        var input = new TextBox
        {
            Text = profile.DisplayName,
            PlaceholderText = profile.DetectedName,
            MaxLength = 100,
            MinWidth = 320,
            SelectionStart = profile.DisplayName.Length
        };

        var content = new StackPanel { Spacing = 10 };
        content.Children.Add(new TextBlock
        {
            Text = $"Executable: {profile.ExecutableName}",
            Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 170, 180, 200)),
            TextWrapping = TextWrapping.Wrap
        });
        content.Children.Add(input);
        content.Children.Add(new TextBlock
        {
            Text = "Leave the field empty to use the automatically detected game name.",
            Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 115, 128, 151)),
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap
        });

        var dialog = new ContentDialog
        {
            XamlRoot = Root.XamlRoot,
            Title = "Rename profile",
            Content = content,
            PrimaryButtonText = "Save name",
            SecondaryButtonText = "Use detected name",
            CloseButtonText = "Cancel",
            DefaultButton = ContentDialogButton.Primary
        };

        ContentDialogResult result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            await _viewModel.RenameProfileAsync(profile, input.Text);
        }
        else if (result == ContentDialogResult.Secondary)
        {
            await _viewModel.RenameProfileAsync(profile, null);
        }
    }

    private async Task ConfirmRemoveAsync(ProfileViewModel profile)
    {
        var dialog = new ContentDialog
        {
            XamlRoot = Root.XamlRoot,
            Title = "Remove profile?",
            Content = $"Remove {profile.DisplayName} from ArcVibrance?",
            PrimaryButtonText = "Remove",
            CloseButtonText = "Cancel",
            DefaultButton = ContentDialogButton.Close
        };

        if (await dialog.ShowAsync() == ContentDialogResult.Primary)
        {
            await _viewModel.RemoveProfileAsync(profile);
        }
    }

    private async void SaveEditor_Click(object sender, RoutedEventArgs e)
    {
        CommitSaturationText();
        await _viewModel.SaveEditorAsync();
        SyncSaturationEditor();
    }

    private void CancelEditor_Click(object sender, RoutedEventArgs e)
    {
        _viewModel.CancelEditor();
        SyncSaturationEditor();
    }

    private void CloseEditor_Click(object sender, RoutedEventArgs e) => _viewModel.SelectedProfile = null;
    private async void ReloadProfiles_Click(object sender, RoutedEventArgs e) => await RunActionAsync(_viewModel.ReloadProfilesAsync);
    private async void Reapply_Click(object sender, RoutedEventArgs e) => await RunActionAsync(_viewModel.ReapplyActiveProfileAsync);
    private async void RestoreDesktop_Click(object sender, RoutedEventArgs e) => await RunActionAsync(_viewModel.RestoreDesktopAsync);

    private async Task RunActionAsync(Func<Task> action)
    {
        try
        {
            await action();
        }
        catch (Exception exception)
        {
            _viewModel.Notification = exception.Message;
        }
    }

    private void SyncSaturationEditor()
    {
        if (SaturationTextBox is null || SaturationSlider is null)
        {
            return;
        }

        SetSaturationValue(_viewModel.EditorSaturation);
    }

    private void SetSaturationValue(double value, bool updateText = true)
    {
        int normalized = Math.Clamp((int)Math.Round(value), 0, 300);
        _syncingSaturation = true;
        try
        {
            _viewModel.EditorSaturation = normalized;
            SaturationSlider.Value = normalized;
            if (updateText)
            {
                string nextText = normalized.ToString();
                if (!string.Equals(SaturationTextBox.Text, nextText, StringComparison.Ordinal))
                {
                    SaturationTextBox.Text = nextText;
                }
            }
        }
        finally
        {
            _syncingSaturation = false;
        }
    }

    private void SaturationSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
    {
        if (!_syncingSaturation)
        {
            SetSaturationValue(e.NewValue);
        }
    }

    private void SaturationTextBox_BeforeTextChanging(TextBox sender, TextBoxBeforeTextChangingEventArgs args)
    {
        // Allow a temporarily empty field while the user replaces the value.
        // Range clamping happens on commit so typing does not fight the caret.
        args.Cancel = !string.IsNullOrEmpty(args.NewText)
            && (args.NewText.Length > 3 || !args.NewText.All(char.IsDigit));
    }

    private void SaturationTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_syncingSaturation)
        {
            return;
        }

        // Preview valid values without rewriting the TextBox.  Avoiding a
        // SetWindowText-style round trip keeps selection and caret behavior stable.
        if (int.TryParse(SaturationTextBox.Text, out int value) && value <= 300)
        {
            _syncingSaturation = true;
            try
            {
                _viewModel.EditorSaturation = value;
                SaturationSlider.Value = value;
            }
            finally
            {
                _syncingSaturation = false;
            }
        }
    }

    private void SaturationTextBox_LostFocus(object sender, RoutedEventArgs e) => CommitSaturationText();

    private void SaturationTextBox_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        switch (e.Key)
        {
            case Windows.System.VirtualKey.Up:
                AdjustSaturation(1);
                e.Handled = true;
                break;
            case Windows.System.VirtualKey.Down:
                AdjustSaturation(-1);
                e.Handled = true;
                break;
            case Windows.System.VirtualKey.PageUp:
                AdjustSaturation(10);
                e.Handled = true;
                break;
            case Windows.System.VirtualKey.PageDown:
                AdjustSaturation(-10);
                e.Handled = true;
                break;
            case Windows.System.VirtualKey.Enter:
                CommitSaturationText();
                SaturationSlider.Focus(FocusState.Programmatic);
                e.Handled = true;
                break;
            case Windows.System.VirtualKey.Escape:
                SetSaturationValue(_viewModel.EditorSaturation);
                SaturationSlider.Focus(FocusState.Programmatic);
                e.Handled = true;
                break;
        }
    }

    private void IncreaseSaturation_Click(object sender, RoutedEventArgs e) => AdjustSaturation(1);

    private void DecreaseSaturation_Click(object sender, RoutedEventArgs e) => AdjustSaturation(-1);

    private void AdjustSaturation(int delta)
    {
        int current = int.TryParse(SaturationTextBox.Text, out int typed)
            ? Math.Clamp(typed, 0, 300)
            : (int)Math.Round(_viewModel.EditorSaturation);
        SetSaturationValue(current + delta);
        SaturationTextBox.SelectionStart = SaturationTextBox.Text.Length;
    }

    private void CommitSaturationText()
    {
        int value = int.TryParse(SaturationTextBox.Text, out int typed)
            ? Math.Clamp(typed, 0, 300)
            : (int)Math.Round(_viewModel.EditorSaturation);
        SetSaturationValue(value);
    }

    private async void StatusTimer_Tick(object? sender, object e) => await _viewModel.RefreshAgentStatusAsync();

    private void CloseBehavior_Checked(object sender, RoutedEventArgs e)
    {
        if (!_initialized || sender is not RadioButton radio || radio.Tag is not string tag)
        {
            return;
        }
        _viewModel.CloseBehavior = tag == "1" ? CloseBehavior.ExitCompletely : CloseBehavior.MinimizeToTray;
    }

    private void ThemeCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_initialized || ThemeCombo.SelectedIndex < 0)
        {
            return;
        }
        _viewModel.ThemeMode = (ThemeMode)ThemeCombo.SelectedIndex;
        ApplyTheme(_viewModel.ThemeMode);
    }

    private void ApplyTheme(ThemeMode theme)
    {
        Root.RequestedTheme = theme switch
        {
            ThemeMode.Light => ElementTheme.Light,
            ThemeMode.Dark => ElementTheme.Dark,
            _ => ElementTheme.Default
        };

        // Theme resources update immediately, while ActualTheme can settle on
        // the next dispatcher turn when following the Windows setting.
        DispatcherQueue.TryEnqueue(RefreshThemeDependentVisuals);
    }

    private void Root_ActualThemeChanged(FrameworkElement sender, object args) =>
        RefreshThemeDependentVisuals();

    private void RefreshThemeDependentVisuals()
    {
        ElementTheme actualTheme = Root.ActualTheme == ElementTheme.Light
            ? ElementTheme.Light
            : ElementTheme.Dark;
        bool useLightTheme = actualTheme == ElementTheme.Light;

        UpdateTitleBarColors(actualTheme);

        foreach (ProfileViewModel profile in _viewModel.Profiles)
        {
            profile.SetLightTheme(useLightTheme);
        }

        UpdateNavigationButton(ProfilesNav, ProfilesNavChrome, ProfilesNavText, ProfilesPage.Visibility == Visibility.Visible);
        UpdateNavigationButton(SettingsNav, SettingsNavChrome, SettingsNavText, SettingsPage.Visibility == Visibility.Visible);
        UpdateNavigationButton(AboutNav, AboutNavChrome, AboutNavText, AboutPage.Visibility == Visibility.Visible);
    }

    private void UpdateTitleBarColors(ElementTheme theme)
    {
        if (_appWindow is null)
        {
            return;
        }

        bool useLightTheme = theme == ElementTheme.Light;
        AppWindowTitleBar titleBar = _appWindow.TitleBar;
        titleBar.ButtonBackgroundColor = Colors.Transparent;
        titleBar.ButtonInactiveBackgroundColor = Colors.Transparent;
        titleBar.ButtonForegroundColor = useLightTheme
            ? Windows.UI.Color.FromArgb(255, 24, 34, 52)
            : Colors.White;
        titleBar.ButtonInactiveForegroundColor = useLightTheme
            ? Windows.UI.Color.FromArgb(255, 123, 135, 154)
            : Windows.UI.Color.FromArgb(255, 125, 135, 150);
        titleBar.ButtonHoverBackgroundColor = useLightTheme
            ? Windows.UI.Color.FromArgb(12, 22, 36, 58)
            : Windows.UI.Color.FromArgb(18, 255, 255, 255);
        titleBar.ButtonPressedBackgroundColor = useLightTheme
            ? Windows.UI.Color.FromArgb(22, 22, 36, 58)
            : Windows.UI.Color.FromArgb(30, 255, 255, 255);
        titleBar.ButtonHoverForegroundColor = titleBar.ButtonForegroundColor;
        titleBar.ButtonPressedForegroundColor = titleBar.ButtonForegroundColor;
    }
}
