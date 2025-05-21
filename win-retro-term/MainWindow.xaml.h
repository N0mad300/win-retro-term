#pragma once

#include "MainWindow.g.h"

namespace winrt::win_retro_term::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();
    };
}

namespace winrt::win_retro_term::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
