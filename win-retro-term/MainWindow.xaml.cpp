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

            std::thread inputSender([this]() { // Capture m_ptyProcess if it's a member
               Sleep(2000); // Wait for cmd to be ready
               if (m_ptyProcess && m_ptyProcess->IsRunning()) {
                   OutputDebugStringA("Sending 'dir' command...\n");
                   m_ptyProcess->WriteInput("dir\r\n");
               }
               Sleep(5000); // Wait for dir output
               if (m_ptyProcess && m_ptyProcess->IsRunning()) {
                   OutputDebugStringA("Sending 'exit' command...\n");
                   m_ptyProcess->WriteInput("exit\r\n");
               }
            });
            inputSender.detach();
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

    int32_t MainWindow::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainWindow::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainWindow::myButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        myButton().Content(box_value(L"Clicked"));
    }
}
