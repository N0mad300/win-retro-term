#pragma once

#include "MainWindow.g.h"
#include "Core/ConPtyProcess.h"
#include <memory>

namespace winrt::win_retro_term::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
    };
}

namespace winrt::win_retro_term::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
