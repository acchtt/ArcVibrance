namespace ArcVibrance.Services;

public sealed class UiCloseSignal : IDisposable
{
    private const string EventName = @"Local\ArcVibrance.Ui.Close.v1";
    private readonly EventWaitHandle _event = new(false, EventResetMode.AutoReset, EventName);
    private readonly CancellationTokenSource _cancellation = new();

    public void Start(Action callback)
    {
        _ = Task.Run(() =>
        {
            int result = WaitHandle.WaitAny([_event, _cancellation.Token.WaitHandle]);
            if (result == 0)
            {
                callback();
            }
        });
    }

    public void Dispose()
    {
        _cancellation.Cancel();
        _event.Dispose();
        _cancellation.Dispose();
    }
}
