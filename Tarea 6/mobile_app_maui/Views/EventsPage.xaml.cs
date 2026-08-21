using Microsoft.Maui.Controls;
using PortonSeguroIoT.Maui.ViewModels;

namespace PortonSeguroIoT.Maui.Views;

public partial class EventsPage : ContentPage
{
    public EventsPage(EventsViewModel viewModel)
    {
        InitializeComponent();
        BindingContext = viewModel;
    }
}
