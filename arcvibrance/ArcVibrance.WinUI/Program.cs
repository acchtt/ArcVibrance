using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;
using WinRT;

namespace ArcVibrance;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        ComWrappersSupport.InitializeComWrappers();

        AppActivationArguments activationArgs = AppInstance.GetCurrent().GetActivatedEventArgs();
        AppInstance mainInstance = AppInstance.FindOrRegisterForKey("ArcVibrance.Main");

        if (!mainInstance.IsCurrent)
        {
            mainInstance.RedirectActivationToAsync(activationArgs).AsTask().GetAwaiter().GetResult();
            return 0;
        }

        Application.Start(p =>
        {
            DispatcherQueue dispatcher = DispatcherQueue.GetForCurrentThread();
            SynchronizationContext.SetSynchronizationContext(
                new DispatcherQueueSynchronizationContext(dispatcher));
            _ = new App();
        });

        return 0;
    }
}
