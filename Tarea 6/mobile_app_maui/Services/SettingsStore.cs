using Microsoft.Maui.Storage;
using PortonSeguroIoT.Maui.Models;

namespace PortonSeguroIoT.Maui.Services;

public sealed class SettingsStore
{
    public MqttSettings LoadMqtt()
    {
        return new MqttSettings
        {
            Host = Preferences.Default.Get("mqtt_host", "192.168.1.100"),
            Port = Preferences.Default.Get("mqtt_port", 1883),
            Username = Preferences.Default.Get("mqtt_user", ""),
            Password = Preferences.Default.Get("mqtt_pass", ""),
            BaseTopic = Preferences.Default.Get("mqtt_base_topic", "porton/device01")
        };
    }

    public GateConfig LoadGateConfig()
    {
        return new GateConfig
        {
            AutoCloseS = Preferences.Default.Get("cfg_auto_close_s", 30),
            MaxTravelS = Preferences.Default.Get("cfg_max_travel_s", 25),
            FtcWaitS = Preferences.Default.Get("cfg_ftc_wait_s", 4),
            ReversePauseS = Preferences.Default.Get("cfg_reverse_pause_s", 2),
            AutoCloseSw = Preferences.Default.Get("cfg_auto_close_sw", true),
            MaintenanceSw = Preferences.Default.Get("cfg_maintenance_sw", false)
        };
    }

    public void Save(MqttSettings mqtt, GateConfig config)
    {
        Preferences.Default.Set("mqtt_host", mqtt.Host);
        Preferences.Default.Set("mqtt_port", mqtt.Port);
        Preferences.Default.Set("mqtt_user", mqtt.Username);
        Preferences.Default.Set("mqtt_pass", mqtt.Password);
        Preferences.Default.Set("mqtt_base_topic", mqtt.BaseTopic);

        Preferences.Default.Set("cfg_auto_close_s", config.AutoCloseS);
        Preferences.Default.Set("cfg_max_travel_s", config.MaxTravelS);
        Preferences.Default.Set("cfg_ftc_wait_s", config.FtcWaitS);
        Preferences.Default.Set("cfg_reverse_pause_s", config.ReversePauseS);
        Preferences.Default.Set("cfg_auto_close_sw", config.AutoCloseSw);
        Preferences.Default.Set("cfg_maintenance_sw", config.MaintenanceSw);
    }
}
