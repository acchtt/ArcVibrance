using System.Collections.ObjectModel;
using ArcVibrance.Infrastructure;
using ArcVibrance.Models;
using ArcVibrance.Services;
using Microsoft.UI.Xaml;

namespace ArcVibrance.ViewModels;

public sealed class MainViewModel : ObservableObject
{
    private readonly ProfileStore _profileStore = new();
    private readonly ProfileNameStore _profileNameStore = new();
    private readonly SettingsStore _settingsStore = new();
    private readonly AgentClient _agentClient = new();
    private readonly ExecutableIconService _iconService = new();
    private readonly GameNameResolver _gameNameResolver = new();

    private ProfileViewModel? _selectedProfile;
    private double _editorSaturation = 100;
    private bool _editorEnabled = true;
    private AgentStatus _agentStatus = AgentStatus.Disconnected;
    private string _notification = string.Empty;
    private bool _isBusy;
    private bool _startWithWindows;
    private ThemeMode _themeMode;
    private CloseBehavior _closeBehavior;

    public ObservableCollection<ProfileViewModel> Profiles { get; } = [];

    public ProfileViewModel? SelectedProfile
    {
        get => _selectedProfile;
        set
        {
            if (_selectedProfile == value)
            {
                return;
            }

            if (_selectedProfile is not null)
            {
                _selectedProfile.IsSelected = false;
            }

            if (SetProperty(ref _selectedProfile, value))
            {
                if (value is not null)
                {
                    value.IsSelected = true;
                    EditorSaturation = value.SaturationPercent;
                    EditorEnabled = value.Enabled;
                }

                OnPropertyChanged(nameof(HasSelectedProfile));
                OnPropertyChanged(nameof(HasNoSelectedProfile));
                OnPropertyChanged(nameof(SelectedProfileVisibility));
                OnPropertyChanged(nameof(NoSelectedProfileVisibility));
            }
        }
    }

    public bool HasSelectedProfile => SelectedProfile is not null;
    public bool HasNoSelectedProfile => SelectedProfile is null;
    public Visibility SelectedProfileVisibility => HasSelectedProfile ? Visibility.Visible : Visibility.Collapsed;
    public Visibility NoSelectedProfileVisibility => HasNoSelectedProfile ? Visibility.Visible : Visibility.Collapsed;

    public double EditorSaturation
    {
        get => _editorSaturation;
        set => SetProperty(ref _editorSaturation, Math.Clamp(value, 0d, 300d));
    }

    public bool EditorEnabled
    {
        get => _editorEnabled;
        set => SetProperty(ref _editorEnabled, value);
    }

    public AgentStatus AgentStatus
    {
        get => _agentStatus;
        private set
        {
            if (SetProperty(ref _agentStatus, value))
            {
                OnPropertyChanged(nameof(AgentConnectionText));
                OnPropertyChanged(nameof(MonitoringText));
                OnPropertyChanged(nameof(ActiveProfileText));
                OnPropertyChanged(nameof(AppliedSaturationText));
                OnPropertyChanged(nameof(IsAgentConnected));
            }
        }
    }

    public bool IsAgentConnected => AgentStatus.AgentRunning;
    public string AgentConnectionText => AgentStatus.AgentRunning
        ? AgentStatus.RuntimeInitialized ? "Agent connected" : "Agent connected · Intel backend initializing"
        : "Agent offline";
    public string MonitoringText => AgentStatus.GameActive
        ? "Active"
        : AgentStatus.AgentRunning
            ? AgentStatus.RuntimeInitialized ? "Waiting" : "Initializing"
            : "Offline";
    public string ActiveProfileText
    {
        get
        {
            if (!AgentStatus.GameActive || string.IsNullOrWhiteSpace(AgentStatus.ActiveExecutableName))
            {
                return "No active profile";
            }

            ProfileViewModel? profile = Profiles.FirstOrDefault(item =>
                string.Equals(item.ExecutableName, AgentStatus.ActiveExecutableName, StringComparison.OrdinalIgnoreCase));
            return profile?.DisplayName ?? AgentStatus.ActiveExecutableName;
        }
    }
    public string AppliedSaturationText => $"{AgentStatus.AppliedSaturationPercent}%";

    public string Notification
    {
        get => _notification;
        set => SetProperty(ref _notification, value);
    }

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                OnPropertyChanged(nameof(BusyVisibility));
            }
        }
    }

    public Visibility BusyVisibility => IsBusy ? Visibility.Visible : Visibility.Collapsed;

    public bool StartWithWindows
    {
        get => _startWithWindows;
        set
        {
            if (_startWithWindows == value)
            {
                return;
            }

            try
            {
                _settingsStore.SetAutoStartEnabled(value);
                SetProperty(ref _startWithWindows, value);
                Notification = value
                    ? "ArcVibrance will start in the system tray after Windows sign-in."
                    : "Windows startup disabled.";
            }
            catch (Exception exception)
            {
                OnPropertyChanged(nameof(StartWithWindows));
                Notification = $"Could not update Windows startup: {exception.Message}";
            }
        }
    }

    public ThemeMode ThemeMode
    {
        get => _themeMode;
        set
        {
            if (SetProperty(ref _themeMode, value))
            {
                _settingsStore.SetThemeMode(value);
            }
        }
    }

    public CloseBehavior CloseBehavior
    {
        get => _closeBehavior;
        set
        {
            if (SetProperty(ref _closeBehavior, value))
            {
                _settingsStore.SetCloseBehavior(value);
            }
        }
    }

    public async Task InitializeAsync()
    {
        IsBusy = true;
        try
        {
            _startWithWindows = _settingsStore.IsAutoStartEnabled();
            OnPropertyChanged(nameof(StartWithWindows));
            _themeMode = _settingsStore.GetThemeMode();
            _closeBehavior = _settingsStore.GetCloseBehavior();
            OnPropertyChanged(nameof(ThemeMode));
            OnPropertyChanged(nameof(CloseBehavior));

            // Start the tray agent before loading profiles. A malformed profile file
            // must never prevent the tray process from being created.
            await EnsureAgentRunningAsync();

            IReadOnlyDictionary<string, string> customNames = await _profileNameStore.LoadAsync();
            IReadOnlyList<GameProfile> profiles = await _profileStore.LoadAsync();
            foreach (GameProfile profile in profiles)
            {
                customNames.TryGetValue(profile.ExecutablePath, out string? customName);
                Profiles.Add(new ProfileViewModel(profile, customName));
            }

            _ = EnrichProfilesAsync(Profiles.ToArray());

            if (Profiles.Count > 0)
            {
                SelectedProfile = Profiles[0];
            }
        }
        catch (Exception exception)
        {
            Notification = exception.Message;
        }
        finally
        {
            IsBusy = false;
        }
    }

    public async Task<bool> EnsureAgentRunningAsync(CancellationToken cancellationToken = default)
    {
        try
        {
            await _agentClient.EnsureRunningAsync(cancellationToken);
            AgentStatus = await _agentClient.GetStatusAsync(cancellationToken);
            if (AgentStatus.AgentRunning && !AgentStatus.RuntimeInitialized)
            {
                Notification = "Tray agent started. Waiting for the Intel color backend.";
            }
            return AgentStatus.AgentRunning;
        }
        catch (Exception exception)
        {
            AgentStatus = AgentStatus.Disconnected;
            Notification = exception.Message;
            return false;
        }
    }

    public async Task AddExecutablesAsync(IEnumerable<string> executablePaths)
    {
        var added = new List<ProfileViewModel>();
        foreach (string inputPath in executablePaths)
        {
            string path;
            try
            {
                path = Path.GetFullPath(inputPath);
            }
            catch
            {
                continue;
            }

            if (!path.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ||
                Profiles.Any(profile => string.Equals(profile.ExecutablePath, path, StringComparison.OrdinalIgnoreCase)))
            {
                continue;
            }

            var profile = new ProfileViewModel(new GameProfile
            {
                ExecutablePath = path,
                SaturationPercent = 150,
                Enabled = true
            });
            Profiles.Add(profile);
            added.Add(profile);
        }

        if (added.Count == 0)
        {
            return;
        }

        SelectedProfile = added[^1];
        await SaveAndReloadAsync();
        await EnrichProfilesAsync(added);
        Notification = added.Count == 1
            ? $"Added {added[0].DisplayName}"
            : $"Added {added.Count} game profiles";
    }

    public async Task SaveEditorAsync()
    {
        if (SelectedProfile is null)
        {
            return;
        }

        SelectedProfile.SaturationPercent = (int)Math.Round(EditorSaturation);
        await SaveAndReloadAsync();
        Notification = $"Saved {SelectedProfile.DisplayName}";
    }

    public void CancelEditor()
    {
        if (SelectedProfile is null)
        {
            return;
        }

        EditorSaturation = SelectedProfile.SaturationPercent;
    }

    public async Task ToggleProfileAsync(ProfileViewModel profile, bool enabled)
    {
        profile.Enabled = enabled;
        if (profile == SelectedProfile)
        {
            EditorEnabled = enabled;
        }
        await SaveAndReloadAsync();
    }

    public async Task RemoveProfileAsync(ProfileViewModel profile)
    {
        int index = Profiles.IndexOf(profile);
        Profiles.Remove(profile);
        SelectedProfile = Profiles.Count == 0 ? null : Profiles[Math.Clamp(index, 0, Profiles.Count - 1)];
        await SaveCustomNamesAsync();
        await SaveAndReloadAsync();
        OnPropertyChanged(nameof(ActiveProfileText));
    }

    public async Task RenameProfileAsync(ProfileViewModel profile, string? customName)
    {
        string? normalized = string.IsNullOrWhiteSpace(customName) ? null : customName.Trim();
        if (string.Equals(normalized, profile.DetectedName, StringComparison.OrdinalIgnoreCase))
        {
            normalized = null;
        }
        profile.SetCustomName(normalized);
        await SaveCustomNamesAsync();
        OnPropertyChanged(nameof(ActiveProfileText));
        Notification = profile.HasCustomName
            ? $"Renamed profile to {profile.DisplayName}"
            : $"Using detected name: {profile.DisplayName}";
    }

    public async Task RedetectGameNameAsync(ProfileViewModel profile)
    {
        GameNameResult result = await _gameNameResolver.ResolveAsync(profile.ExecutablePath);
        profile.ApplyDetectedName(result);
        OnPropertyChanged(nameof(ActiveProfileText));
        Notification = $"{profile.DisplayName} · {profile.NameSourceText}";
    }

    public async Task RedetectAllGameNamesAsync()
    {
        IsBusy = true;
        try
        {
            await Task.WhenAll(Profiles.Select(RedetectGameNameCoreAsync));
            OnPropertyChanged(nameof(ActiveProfileText));
            Notification = $"Updated names for {Profiles.Count} profiles";
        }
        finally
        {
            IsBusy = false;
        }
    }

    public async Task ReloadProfilesAsync()
    {
        AgentStatus = await _agentClient.ReloadProfilesAsync();
        Notification = "Profiles reloaded";
    }

    public async Task RefreshAgentStatusAsync()
    {
        AgentStatus = await _agentClient.GetStatusAsync();
    }

    public async Task ReapplyActiveProfileAsync()
    {
        AgentStatus = await _agentClient.ReapplyActiveProfileAsync();
        Notification = "Active profile reapplied";
    }

    public async Task RestoreDesktopAsync()
    {
        AgentStatus = await _agentClient.RestoreDesktopAsync();
        Notification = "Desktop saturation restored";
    }

    private async Task SaveAndReloadAsync()
    {
        await _profileStore.SaveAsync(Profiles.Select(profile => profile.Model));
        try
        {
            AgentStatus = await _agentClient.ReloadProfilesAsync();
        }
        catch
        {
            AgentStatus = AgentStatus.Disconnected;
            Notification = "Profiles saved. ArcVibrance Agent is offline.";
        }
    }

    private Task SaveCustomNamesAsync() => _profileNameStore.SaveAsync(
        Profiles.Where(profile => profile.HasCustomName)
            .Select(profile => new KeyValuePair<string, string>(profile.ExecutablePath, profile.CustomName!)));

    private async Task EnrichProfilesAsync(IEnumerable<ProfileViewModel> profiles)
    {
        await Task.WhenAll(profiles.Select(async profile =>
        {
            Task<Microsoft.UI.Xaml.Media.ImageSource?> iconTask = _iconService.LoadAsync(profile.ExecutablePath);
            Task<GameNameResult> nameTask = _gameNameResolver.ResolveAsync(profile.ExecutablePath);

            Microsoft.UI.Xaml.Media.ImageSource? icon = await iconTask;
            if (icon is null)
            {
                // Explorer can still be populating its icon cache immediately after
                // profile discovery. A delayed second attempt prevents a transient
                // miss from leaving the initial-letter fallback for the whole session.
                await Task.Delay(600);
                icon = await _iconService.LoadAsync(profile.ExecutablePath);
            }

            profile.Icon = icon;
            profile.ApplyDetectedName(await nameTask);
        }));
        OnPropertyChanged(nameof(ActiveProfileText));
    }

    private async Task RedetectGameNameCoreAsync(ProfileViewModel profile)
    {
        GameNameResult result = await _gameNameResolver.ResolveAsync(profile.ExecutablePath);
        profile.ApplyDetectedName(result);
    }
}
