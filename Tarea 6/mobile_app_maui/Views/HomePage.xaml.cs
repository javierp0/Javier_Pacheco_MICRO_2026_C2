using Microsoft.Maui.Controls;
using PortonSeguroIoT.Maui.ViewModels;

namespace PortonSeguroIoT.Maui.Views;

public partial class HomePage : ContentPage
{
    public HomePage(MainViewModel viewModel)
    {
        InitializeComponent();
        BindingContext = viewModel;
    }
}
