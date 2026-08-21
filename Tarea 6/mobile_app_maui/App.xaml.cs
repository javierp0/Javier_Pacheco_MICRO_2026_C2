using Microsoft.Maui.Controls;

namespace PortonSeguroIoT.Maui;

public partial class App : Application
{
    public App(AppShell shell)
    {
        InitializeComponent();
        MainPage = shell;
    }
}
