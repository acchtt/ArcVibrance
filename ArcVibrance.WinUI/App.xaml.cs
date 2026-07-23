using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;

namespace ArcVibrance;

public partial class App : Application
{
    public static MainWindow? MainWindowInstance { get; private set; }

    public App()
    {
        InitializeComponent();
        AppInstance.GetCurrent().Activated += Current_Activated;
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        MainWindowInstance = new MainWindow();
        MainWindowInstance.Activate();
    }

    private void Current_Activated(object? sender, AppActivationArguments args)
    {
        MainWindowInstance?.DispatcherQueue.TryEnqueue(() => MainWindowInstance.ShowAndActivate());
    }
}
