namespace PortonSeguroIoT.Maui.Models;

public sealed class GateConfig
{
    public int AutoCloseS { get; set; } = 30;
    public int MaxTravelS { get; set; } = 25;
    public int FtcWaitS { get; set; } = 4;
    public int ReversePauseS { get; set; } = 2;
    public bool AutoCloseSw { get; set; } = true;
    public bool MaintenanceSw { get; set; }
}
