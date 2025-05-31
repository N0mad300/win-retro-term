#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.h>
#include <iostream>
#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::win_retro_term::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
    }
}
