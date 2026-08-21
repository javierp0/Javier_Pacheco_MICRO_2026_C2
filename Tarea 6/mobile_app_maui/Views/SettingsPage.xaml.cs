using Microsoft.Maui.Controls;
using PortonSeguroIoT.Maui.ViewModels;

namespace PortonSeguroIoT.Maui.Views;

public partial class SettingsPage : ContentPage
{
    public SettingsPage(SettingsViewModel viewModel)
    {
        InitializeComponent();
        BindingContext = viewModel;
    }
}
