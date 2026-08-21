namespace PortonSeguroIoT.Maui.Models;

public sealed class MqttSettings
{
    public string Host { get; set; } = "192.168.1.100";
    public int Port { get; set; } = 1883;
    public string Username { get; set; } = "";
    public string Password { get; set; } = "";
    public string BaseTopic { get; set; } = "porton/device01";
}
