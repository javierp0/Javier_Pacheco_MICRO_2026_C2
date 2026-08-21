using System.Windows.Input;

namespace PortonSeguroIoT.Maui.ViewModels;

public sealed class AsyncCommand : ICommand
{
    private readonly Func<Task> _execute;
    private readonly Func<bool>? _canExecute;
    private bool _isRunning;

    public AsyncCommand(Func<Task> execute, Func<bool>? canExecute = null)
    {
        _execute = execute;
        _canExecute = canExecute;
    }

    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter)
    {
        return !_isRunning && (_canExecute?.Invoke() ?? true);
    }

    public async void Execute(object? parameter)
    {
        if (!CanExecute(parameter))
        {
            return;
        }

        _isRunning = true;
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);

        try
        {
            await _execute();
        }
        finally
        {
            _isRunning = false;
            CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }
    }
}
