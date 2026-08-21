using System.Windows.Input;
using Microsoft.Maui.ApplicationModel;
using PortonSeguroIoT.Maui.Models;
using PortonSeguroIoT.Maui.Services;

namespace PortonSeguroIoT.Maui.ViewModels;

public sealed class MainViewModel : ObservableObject
{
    private readonly MqttGateService _mqtt;
    private string _state = "SIN DATOS";
    private string _error = "NONE";
    private string _connectionText = "MQTT desconectado";
    private string _lastEvent = "Esperando datos del ESP32";
    private int _positionPct;
    private bool _deviceOnline;
    private bool _limitOpen;
    private bool _limitClosed;
    private bool _ftcBlocked;
    private bool _dipAutoClose;
    private bool _dipMaintenance;

    public MainViewModel(MqttGateService mqtt)
    {
        _mqtt = mqtt;
        _mqtt.ConnectionChanged += HandleConnectionChanged;
        _mqtt.SnapshotChanged += HandleSnapshotChanged;
        _mqtt.LogReceived += HandleLogReceived;

        OpenCommand = new AsyncCommand(() => _mqtt.SendCommandAsync("OPEN"));
        CloseCommand = new AsyncCommand(() => _mqtt.SendCommandAsync("CLOSE"));
        StopCommand = new AsyncCommand(() => _mqtt.SendCommandAsync("STOP"));
    }

    public string State { get => _state; private set => SetProperty(ref _state, value); }
    public string Error { get => _error; private set => SetProperty(ref _error, value); }
    public string ConnectionText { get => _connectionText; private set => SetProperty(ref _connectionText, value); }
    public string LastEvent { get => _lastEvent; private set => SetProperty(ref _lastEvent, value); }
    public int PositionPct { get => _positionPct; private set { if (SetProperty(ref _positionPct, value)) OnPropertyChanged(nameof(PositionProgress)); } }
    public double PositionProgress => Math.Clamp(PositionPct, 0, 100) / 100.0;
    public bool DeviceOnline { get => _deviceOnline; private set => SetProperty(ref _deviceOnline, value); }
    public bool LimitOpen { get => _limitOpen; private set => SetProperty(ref _limitOpen, value); }
    public bool LimitClosed { get => _limitClosed; private set => SetProperty(ref _limitClosed, value); }
    public bool FtcBlocked { get => _ftcBlocked; private set => SetProperty(ref _ftcBlocked, value); }
    public bool DipAutoClose { get => _dipAutoClose; private set => SetProperty(ref _dipAutoClose, value); }
    public bool DipMaintenance { get => _dipMaintenance; private set => SetProperty(ref _dipMaintenance, value); }

    public ICommand OpenCommand { get; }
    public ICommand CloseCommand { get; }
    public ICommand StopCommand { get; }

    private void HandleConnectionChanged(object? sender, bool connected)
    {
        MainThread.BeginInvokeOnMainThread(() =>
        {
            ConnectionText = connected ? "MQTT conectado" : "MQTT desconectado";
        });
    }

    private void HandleSnapshotChanged(object? sender, GateSnapshot snapshot)
    {
        MainThread.BeginInvokeOnMainThread(() =>
        {
            if (snapshot.StateKnown)
            {
                State = snapshot.State;
                Error = snapshot.Error;
                PositionPct = snapshot.PositionPct;
            }

            if (snapshot.DeviceOnlineKnown)
            {
                DeviceOnline = snapshot.DeviceOnline;
            }

            if (snapshot.TelemetryKnown)
            {
                LimitOpen = snapshot.LimitOpen;
                LimitClosed = snapshot.LimitClosed;
                FtcBlocked = snapshot.FtcBlocked;
                DipAutoClose = snapshot.DipAutoClose;
                DipMaintenance = snapshot.DipMaintenance;
            }
        });
    }

    private void HandleLogReceived(object? sender, EventLogItem item)
    {
        MainThread.BeginInvokeOnMainThread(() =>
        {
            LastEvent = $"{item.Type}: {item.Message}";
        });
    }
}
