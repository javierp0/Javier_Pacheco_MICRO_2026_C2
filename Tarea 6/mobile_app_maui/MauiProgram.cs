using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Maui.Hosting;
using PortonSeguroIoT.Maui.Services;
using PortonSeguroIoT.Maui.ViewModels;
using PortonSeguroIoT.Maui.Views;

namespace PortonSeguroIoT.Maui;

public static class MauiProgram
{
    public static MauiApp CreateMauiApp()
    {
        var builder = MauiApp.CreateBuilder();
        builder
            .UseMauiApp<App>();

        builder.Services.AddSingleton<SettingsStore>();
        builder.Services.AddSingleton<MqttGateService>();

        builder.Services.AddSingleton<MainViewModel>();
        builder.Services.AddSingleton<SettingsViewModel>();
        builder.Services.AddSingleton<EventsViewModel>();

        builder.Services.AddSingleton<HomePage>();
        builder.Services.AddSingleton<SettingsPage>();
        builder.Services.AddSingleton<EventsPage>();
        builder.Services.AddSingleton<AppShell>();

#if DEBUG
        builder.Logging.AddDebug();
#endif

        return builder.Build();
    }
}
