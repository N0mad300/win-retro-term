#include "pch.h"
#include "TerminalControl.xaml.h"
#if __has_include("TerminalControl.g.cpp")
#include "TerminalControl.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <string>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::win_retro_term::implementation
{
    TerminalControl::TerminalControl()
    {
        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        m_terminalBuffer = std::make_unique<Core::TerminalBuffer>(25, 80);
        m_ansiParser = std::make_unique<Core::AnsiParser>(*m_terminalBuffer);
        m_renderer = std::make_unique<D3D11Renderer>();
        m_ptyProcess = std::make_unique<ConPtyProcess>();

        this->Loaded({ this, &TerminalControl::OnLoaded });
        this->Unloaded({ this, &TerminalControl::OnUnloaded });
    }

    TerminalControl::~TerminalControl()
    {
    }

    void TerminalControl::InitializePtyAndBuffer() {
        // Get initial terminal dimensions
        UpdateTerminalSize();

        auto ptyCallback = [this](const char* buffer, size_t length) {
            this->PtyDataReceived(buffer, length);
            };

        COORD ptyInitialSize = { static_cast<SHORT>(m_terminalBuffer->GetCols()), static_cast<SHORT>(m_terminalBuffer->GetRows()) };

        if (m_ptyProcess->Start(L"cmd.exe", ptyInitialSize, ptyCallback)) {
            OutputDebugStringA("TerminalControl: ConPTY started successfully.\n");
        }
        else {
            OutputDebugStringA("TerminalControl: Failed to start ConPTY.\n");
            // TODO: Display an error in the terminal UI
            m_terminalBuffer->AddChar(L'E');
            m_terminalBuffer->AddChar(L'R');
            m_terminalBuffer->AddChar(L'R');
            m_terminalBuffer->AddChar(L'O');
            m_terminalBuffer->AddChar(L'R');
        }
    }

    void TerminalControl::OnLoaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        m_renderer->Initialize(dxSwapChainPanel(), m_terminalBuffer.get());

        m_renderer->SetLogicalSize({ (float)dxSwapChainPanel().ActualWidth(), (float)dxSwapChainPanel().ActualHeight() });
        m_renderer->SetCompositionScale(dxSwapChainPanel().CompositionScaleX(), dxSwapChainPanel().CompositionScaleY());

        if (m_renderer->IsInitialized()) {
            m_charWidthApprox = m_renderer->GetFontCharWidth();
            m_charHeightApprox = m_renderer->GetFontCharHeight();
        }

        InitializePtyAndBuffer();

        dxSwapChainPanel().SizeChanged({ this, &TerminalControl::OnSizeChanged });
        dxSwapChainPanel().CompositionScaleChanged({ this, &TerminalControl::OnCompositionScaleChanged });

        m_renderingEventToken = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::Rendering({ this, &TerminalControl::OnRendering });
    }

    void TerminalControl::OnUnloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        if (m_renderingEventToken)
        {
            winrt::Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(m_renderingEventToken);
            m_renderingEventToken = {};
        }
        if (m_ptyProcess) {
            m_ptyProcess->Stop();
            m_ptyProcess.reset();
        }
        m_renderer.reset();
        m_terminalBuffer.reset();
        m_ansiParser.reset();
    }

    void TerminalControl::PtyDataReceived(const char* buffer, size_t length) {
        std::vector<char> dataCopy(buffer, buffer + length);

        m_dispatcherQueue.TryEnqueue([this, data = std::move(dataCopy)]() {
            if (m_ansiParser) {
                m_ansiParser->Parse(data.data(), data.size());
            }
            });
    }

    void TerminalControl::UpdateTerminalSize() {
        if (!m_renderer || !m_renderer->IsInitialized() || !m_terminalBuffer || !m_ptyProcess) {
            return;
        }

        float panelWidth = static_cast<float>(dxSwapChainPanel().ActualWidth());
        float panelHeight = static_cast<float>(dxSwapChainPanel().ActualHeight());

        if (panelWidth <= 0 || panelHeight <= 0 || m_charWidthApprox <= 0 || m_charHeightApprox <= 0) return;

        int newCols = static_cast<int>(panelWidth / m_charWidthApprox);
        int newRows = static_cast<int>(panelHeight / m_charHeightApprox);

        newCols = std::max(1, newCols);
        newRows = std::max(1, newRows);

        if (newCols != m_terminalBuffer->GetCols() || newRows != m_terminalBuffer->GetRows()) {
            OutputDebugStringA(("TerminalControl Resizing to R: " + std::to_string(newRows) + " C: " + std::to_string(newCols) + "\n").c_str());
            m_terminalBuffer->Resize(newRows, newCols);
            if (m_ptyProcess->IsRunning()) {
                m_ptyProcess->Resize({ static_cast<SHORT>(newCols), static_cast<SHORT>(newRows) });
            }
        }
    }

    void TerminalControl::OnSizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
    {
        if (m_renderer)
        {
            m_renderer->SetLogicalSize(args.NewSize());
        }
        UpdateTerminalSize();
    }

    void TerminalControl::OnCompositionScaleChanged(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& sender, winrt::Windows::Foundation::IInspectable const& args)
    {
        if (m_renderer)
        {
            m_renderer->SetCompositionScale(sender.CompositionScaleX(), sender.CompositionScaleY());
            m_charWidthApprox = m_renderer->GetFontCharWidth();
            m_charHeightApprox = m_renderer->GetFontCharHeight();
        }
        UpdateTerminalSize();
    }

    void TerminalControl::OnRendering(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args)
    {
        if (m_renderer && m_renderer->IsInitialized())
        {
            m_renderer->Render();
            m_renderer->Present();
        }
    }
}
