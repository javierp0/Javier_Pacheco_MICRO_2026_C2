using System.Collections.ObjectModel;
using Microsoft.Maui.ApplicationModel;
using PortonSeguroIoT.Maui.Models;
using PortonSeguroIoT.Maui.Services;

namespace PortonSeguroIoT.Maui.ViewModels;

public sealed class EventsViewModel : ObservableObject
{
    public EventsViewModel(MqttGateService mqtt)
    {
        mqtt.LogReceived += (_, item) =>
        {
            MainThread.BeginInvokeOnMainThread(() =>
            {
                Events.Insert(0, item);
                while (Events.Count > 80)
                {
                    Events.RemoveAt(Events.Count - 1);
                }
            });
        };
    }

    public ObservableCollection<EventLogItem> Events { get; } = new();
}
