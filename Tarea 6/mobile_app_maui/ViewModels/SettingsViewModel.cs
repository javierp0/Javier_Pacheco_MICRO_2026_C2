using System.Windows.Input;
using Microsoft.Maui.ApplicationModel;
using PortonSeguroIoT.Maui.Models;
using PortonSeguroIoT.Maui.Services;

namespace PortonSeguroIoT.Maui.ViewModels;

public sealed class SettingsViewModel : ObservableObject
{
    private readonly SettingsStore _store;
    private readonly MqttGateService _mqtt;
    private string _host;
    private string _port;
    private string _username;
    private string _password;
    private string _autoCloseS;
    private string _maxTravelS;
    private string _ftcWaitS;
    private string _reversePauseS;
    private bool _autoCloseSw;
    private bool _maintenanceSw;
    private string _status = "Configura la IP del broker local";

    public SettingsViewModel(SettingsStore store, MqttGateService mqtt)
    {
        _store = store;
        _mqtt = mqtt;

        var mqttSettings = _store.LoadMqtt();
        var gateConfig = _store.LoadGateConfig();

        _host = mqttSettings.Host;
        _port = mqttSettings.Port.ToString();
        _username = mqttSettings.Username;
        _password = mqttSettings.Password;
        _autoCloseS = gateConfig.AutoCloseS.ToString();
        _maxTravelS = gateConfig.MaxTravelS.ToString();
        _ftcWaitS = gateConfig.FtcWaitS.ToString();
        _reversePauseS = gateConfig.ReversePauseS.ToString();
        _autoCloseSw = gateConfig.AutoCloseSw;
        _maintenanceSw = gateConfig.MaintenanceSw;

        _mqtt.ConnectionChanged += (_, connected) => MainThread.BeginInvokeOnMainThread(() => Status = connected ? "MQTT conectado" : "MQTT desconectado");
        _mqtt.ConfigReceived += (_, config) => MainThread.BeginInvokeOnMainThread(() => ApplyConfig(config));

        SaveLocalCommand = new AsyncCommand(SaveLocalAsync);
        ConnectCommand = new AsyncCommand(ConnectAsync);
        DisconnectCommand = new AsyncCommand(() => _mqtt.DisconnectAsync());
        SendConfigCommand = new AsyncCommand(SendConfigAsync);
        ResetErrorCommand = new AsyncCommand(() => _mqtt.SendCommandAsync("RESET_ERROR"));
        CalibrateCommand = new AsyncCommand(() => _mqtt.SendCommandAsync("CALIBRATE"));
        SaveConfigInEspCommand = new AsyncCommand(() => _mqtt.SendCommandAsync("SAVE_CONFIG"));
    }

    public string Host { get => _host; set => SetProperty(ref _host, value); }
    public string Port { get => _port; set => SetProperty(ref _port, value); }
    public string Username { get => _username; set => SetProperty(ref _username, value); }
    public string Password { get => _password; set => SetProperty(ref _password, value); }
    public string AutoCloseS { get => _autoCloseS; set => SetProperty(ref _autoCloseS, value); }
    public string MaxTravelS { get => _maxTravelS; set => SetProperty(ref _maxTravelS, value); }
    public string FtcWaitS { get => _ftcWaitS; set => SetProperty(ref _ftcWaitS, value); }
    public string ReversePauseS { get => _reversePauseS; set => SetProperty(ref _reversePauseS, value); }
    public bool AutoCloseSw { get => _autoCloseSw; set => SetProperty(ref _autoCloseSw, value); }
    public bool MaintenanceSw { get => _maintenanceSw; set => SetProperty(ref _maintenanceSw, value); }
    public string Status { get => _status; private set => SetProperty(ref _status, value); }

    public ICommand SaveLocalCommand { get; }
    public ICommand ConnectCommand { get; }
    public ICommand DisconnectCommand { get; }
    public ICommand SendConfigCommand { get; }
    public ICommand ResetErrorCommand { get; }
    public ICommand CalibrateCommand { get; }
    public ICommand SaveConfigInEspCommand { get; }

    private Task SaveLocalAsync()
    {
        _store.Save(ToMqttSettings(), ToGateConfig());
        Status = "Configuracion guardada en el telefono";
        return Task.CompletedTask;
    }

    private async Task ConnectAsync()
    {
        await SaveLocalAsync();
        await _mqtt.ConnectAsync(ToMqttSettings());
    }

    private async Task SendConfigAsync()
    {
        await SaveLocalAsync();
        await _mqtt.SendConfigAsync(ToGateConfig());
        Status = "Configuracion enviada al ESP32";
    }

    private MqttSettings ToMqttSettings()
    {
        return new MqttSettings
        {
            Host = Host.Trim(),
            Port = ParseInt(Port, 1883),
            Username = Username.Trim(),
            Password = Password,
            BaseTopic = "porton/device01"
        };
    }

    private GateConfig ToGateConfig()
    {
        return new GateConfig
        {
            AutoCloseS = ParseInt(AutoCloseS, 30),
            MaxTravelS = ParseInt(MaxTravelS, 25),
            FtcWaitS = ParseInt(FtcWaitS, 4),
            ReversePauseS = ParseInt(ReversePauseS, 2),
            AutoCloseSw = AutoCloseSw,
            MaintenanceSw = MaintenanceSw
        };
    }

    private void ApplyConfig(GateConfig config)
    {
        AutoCloseS = config.AutoCloseS.ToString();
        MaxTravelS = config.MaxTravelS.ToString();
        FtcWaitS = config.FtcWaitS.ToString();
        ReversePauseS = config.ReversePauseS.ToString();
        AutoCloseSw = config.AutoCloseSw;
        MaintenanceSw = config.MaintenanceSw;
    }

    private static int ParseInt(string value, int fallback)
    {
        return int.TryParse(value, out var parsed) && parsed > 0 ? parsed : fallback;
    }
}
