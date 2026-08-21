namespace PortonSeguroIoT.Maui.Models;

public sealed class EventLogItem
{
    public DateTime Time { get; init; } = DateTime.Now;
    public string Type { get; init; } = "APP";
    public string Message { get; init; } = "";

    public string TimeText => Time.ToString("HH:mm:ss");
}
