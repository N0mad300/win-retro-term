#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Core/ConPtyProcess.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

std::unique_ptr<ConPtyProcess> m_ptyProcess;

namespace winrt::win_retro_term::implementation
{
    MainWindow::MainWindow()
    {
        m_ptyProcess = std::make_unique<ConPtyProcess>();

        COORD size = { 80, 25 };

        // Define a callback for data
        auto dataCallback = [](const char* buffer, size_t length) {
            // For now, just print to debug output.
            // IMPORTANT: This callback might be from a different thread!
            // If updating UI, you'll need to dispatch to the UI thread.
            std::string data(buffer, length);
            OutputDebugStringA("PTY Data: ");
            OutputDebugStringA(data.c_str());
            OutputDebugStringA("\n");
            };

        if (m_ptyProcess->Start(L"cmd.exe /k prompt $g$s", size, dataCallback)) // /k keeps cmd open, prompt $g for '>'
        {
            OutputDebugStringA("ConPTY started successfully.\n");
        }
        else
        {
            OutputDebugStringA("Failed to start ConPTY.\n");
        }
    }

    MainWindow::~MainWindow()
    {
        if (m_ptyProcess) {
            m_ptyProcess->Stop();
        }
    }
}
