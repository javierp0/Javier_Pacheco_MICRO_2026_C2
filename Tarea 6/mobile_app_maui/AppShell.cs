using Microsoft.Maui.Controls;
using PortonSeguroIoT.Maui.Views;

namespace PortonSeguroIoT.Maui;

public sealed class AppShell : Shell
{
    public AppShell(HomePage homePage, SettingsPage settingsPage, EventsPage eventsPage)
    {
        Title = "Porton Seguro IoT";
        FlyoutBehavior = FlyoutBehavior.Disabled;

        Items.Add(new TabBar
        {
            Items =
            {
                new ShellContent { Title = "Inicio", Content = homePage },
                new ShellContent { Title = "Configuracion", Content = settingsPage },
                new ShellContent { Title = "Eventos", Content = eventsPage }
            }
        });
    }
}
