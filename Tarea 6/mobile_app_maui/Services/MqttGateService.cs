using System.Text;
using System.Text.Json;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using PortonSeguroIoT.Maui.Models;

namespace PortonSeguroIoT.Maui.Services;

public sealed class MqttGateService
{
    private readonly MqttFactory _factory = new();
    private IMqttClient? _client;
    private MqttSettings _settings = new();

    public event EventHandler<bool>? ConnectionChanged;
    public event EventHandler<GateSnapshot>? SnapshotChanged;
    public event EventHandler<GateConfig>? ConfigReceived;
    public event EventHandler<EventLogItem>? LogReceived;

    public bool IsConnected => _client?.IsConnected == true;

    public async Task ConnectAsync(MqttSettings settings, CancellationToken cancellationToken = default)
    {
        _settings = settings;

        if (_client?.IsConnected == true)
        {
            await _client.DisconnectAsync();
        }

        _client = _factory.CreateMqttClient();
        _client.ConnectedAsync += OnConnectedAsync;
        _client.DisconnectedAsync += OnDisconnectedAsync;
        _client.ApplicationMessageReceivedAsync += OnMessageReceivedAsync;

        var optionsBuilder = new MqttClientOptionsBuilder()
            .WithClientId($"porton_maui_{Guid.NewGuid():N}")
            .WithTcpServer(settings.Host, settings.Port)
            .WithCleanSession();

        if (!string.IsNullOrWhiteSpace(settings.Username))
        {
            optionsBuilder.WithCredentials(settings.Username, settings.Password);
        }

        await _client.ConnectAsync(optionsBuilder.Build(), cancellationToken);
    }

    public async Task DisconnectAsync(CancellationToken cancellationToken = default)
    {
        if (_client?.IsConnected == true)
        {
            await _client.DisconnectAsync();
        }
    }

    public Task SendCommandAsync(string command, CancellationToken cancellationToken = default)
    {
        return PublishAsync($"{_settings.BaseTopic}/command", command, cancellationToken);
    }

    public Task SendConfigAsync(GateConfig config, CancellationToken cancellationToken = default)
    {
        var payload = JsonSerializer.Serialize(new
        {
            auto_close_s = config.AutoCloseS,
            max_travel_s = config.MaxTravelS,
            ftc_wait_s = config.FtcWaitS,
            reverse_pause_s = config.ReversePauseS,
            auto_close_sw = config.AutoCloseSw,
            maintenance_sw = config.MaintenanceSw
        });

        return PublishAsync($"{_settings.BaseTopic}/config/set", payload, cancellationToken);
    }

    private async Task PublishAsync(string topic, string payload, CancellationToken cancellationToken)
    {
        if (_client?.IsConnected != true)
        {
            AddLog("APP", "MQTT no esta conectado");
            return;
        }

        var message = new MqttApplicationMessageBuilder()
            .WithTopic(topic)
            .WithPayload(payload)
            .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
            .Build();

        await _client.PublishAsync(message, cancellationToken);
    }

    private async Task OnConnectedAsync(MqttClientConnectedEventArgs args)
    {
        ConnectionChanged?.Invoke(this, true);
        AddLog("MQTT", "Conexion establecida");

        if (_client is null)
        {
            return;
        }

        var topics = new[]
        {
            "availability",
            "state",
            "telemetry",
            "event",
            "config/state",
            "ack"
        };

        foreach (var leaf in topics)
        {
            await _client.SubscribeAsync($"{_settings.BaseTopic}/{leaf}");
        }
    }

    private Task OnDisconnectedAsync(MqttClientDisconnectedEventArgs args)
    {
        ConnectionChanged?.Invoke(this, false);
        AddLog("MQTT", "Conexion cerrada");
        return Task.CompletedTask;
    }

    private Task OnMessageReceivedAsync(MqttApplicationMessageReceivedEventArgs args)
    {
        var topic = args.ApplicationMessage.Topic;
        var payload = GetPayloadText(args.ApplicationMessage.PayloadSegment);
        var leaf = topic.StartsWith(_settings.BaseTopic, StringComparison.Ordinal)
            ? topic[(_settings.BaseTopic.Length + 1)..]
            : topic;

        try
        {
            switch (leaf)
            {
                case "availability":
                    SnapshotChanged?.Invoke(this, new GateSnapshot { DeviceOnlineKnown = true, DeviceOnline = payload.Trim() == "online" });
                    AddLog("ESP32", $"Disponibilidad: {payload}");
                    break;

                case "state":
                    SnapshotChanged?.Invoke(this, ParseState(payload));
                    break;

                case "telemetry":
                    SnapshotChanged?.Invoke(this, ParseTelemetry(payload));
                    break;

                case "config/state":
                    ConfigReceived?.Invoke(this, ParseConfig(payload));
                    AddLog("CONFIG", "Configuracion recibida del ESP32");
                    break;

                case "event":
                    AddLog("EVENTO", payload);
                    break;

                case "ack":
                    AddLog("ACK", payload);
                    break;
            }
        }
        catch (Exception ex)
        {
            AddLog("MQTT", $"Mensaje no procesado: {ex.Message}");
        }

        return Task.CompletedTask;
    }

    private static GateSnapshot ParseState(string payload)
    {
        using var doc = JsonDocument.Parse(payload);
        var root = doc.RootElement;
        return new GateSnapshot
        {
            StateKnown = true,
            State = GetString(root, "state", "SIN DATOS"),
            Error = GetString(root, "error", "NONE"),
            PositionPct = GetInt(root, "position_pct", 0)
        };
    }

    private static GateSnapshot ParseTelemetry(string payload)
    {
        using var doc = JsonDocument.Parse(payload);
        var root = doc.RootElement;
        var sensors = root.TryGetProperty("sensors", out var s) ? s : root;
        var dips = root.TryGetProperty("dips", out var d) ? d : root;

        return new GateSnapshot
        {
            TelemetryKnown = true,
            LimitOpen = GetBool(sensors, "limit_open", false),
            LimitClosed = GetBool(sensors, "limit_closed", false),
            FtcBlocked = GetBool(sensors, "ftc_blocked", false),
            DipAutoClose = GetBool(dips, "auto_close", false),
            DipMaintenance = GetBool(dips, "maintenance", false)
        };
    }

    private static GateConfig ParseConfig(string payload)
    {
        using var doc = JsonDocument.Parse(payload);
        var root = doc.RootElement;

        return new GateConfig
        {
            AutoCloseS = GetInt(root, "auto_close_s", 30),
            MaxTravelS = GetInt(root, "max_travel_s", 25),
            FtcWaitS = GetInt(root, "ftc_wait_s", 4),
            ReversePauseS = GetInt(root, "reverse_pause_s", 2),
            AutoCloseSw = GetBool(root, "auto_close_sw", true),
            MaintenanceSw = GetBool(root, "maintenance_sw", false)
        };
    }

    private static string GetString(JsonElement element, string name, string fallback)
    {
        return element.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? fallback
            : fallback;
    }

    private static int GetInt(JsonElement element, string name, int fallback)
    {
        return element.TryGetProperty(name, out var value) && value.TryGetInt32(out var result)
            ? result
            : fallback;
    }

    private static bool GetBool(JsonElement element, string name, bool fallback)
    {
        return element.TryGetProperty(name, out var value) && value.ValueKind is JsonValueKind.True or JsonValueKind.False
            ? value.GetBoolean()
            : fallback;
    }

    private static string GetPayloadText(ArraySegment<byte> payload)
    {
        return payload.Array is null
            ? ""
            : Encoding.UTF8.GetString(payload.Array, payload.Offset, payload.Count);
    }

    private void AddLog(string type, string message)
    {
        LogReceived?.Invoke(this, new EventLogItem
        {
            Time = DateTime.Now,
            Type = type,
            Message = message
        });
    }
}
