namespace PortonSeguroIoT.Maui.Models;

public sealed class GateSnapshot
{
    public string State { get; set; } = "SIN DATOS";
    public string Error { get; set; } = "NONE";
    public int PositionPct { get; set; }
    public bool StateKnown { get; set; }
    public bool TelemetryKnown { get; set; }
    public bool DeviceOnlineKnown { get; set; }
    public bool DeviceOnline { get; set; }
    public bool LimitOpen { get; set; }
    public bool LimitClosed { get; set; }
    public bool FtcBlocked { get; set; }
    public bool DipAutoClose { get; set; }
    public bool DipMaintenance { get; set; }
}
