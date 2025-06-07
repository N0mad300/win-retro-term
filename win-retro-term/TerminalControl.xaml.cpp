#include "pch.h"
#include "TerminalControl.xaml.h"
#if __has_include("TerminalControl.g.cpp")
#include "TerminalControl.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Core.h>

#include <string>

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;

namespace winrt::win_retro_term::implementation
{
    TerminalControl::TerminalControl()
    {
        InitializeComponent();
        RootGrid().IsTabStop(true);

        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        m_terminalBuffer = std::make_unique<Core::TerminalBuffer>(25, 80);
        m_ansiParser = std::make_unique<Core::AnsiParser>(*m_terminalBuffer.get());
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

        RootGrid().Focus(FocusState::Programmatic);

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

    void TerminalControl::SendInputToPty(const std::string& utf8Input) {
        if (m_ptyProcess && m_ptyProcess->IsRunning() && !utf8Input.empty()) {
            m_ptyProcess->WriteInput(utf8Input);
        }
    }

    void TerminalControl::RootGrid_OnGotFocus(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args) {
        m_isFocused = true;
        OutputDebugStringA("TerminalControl got focus.\n");
        // TODO: Update cursor appearance (e.g : start blinking)
    }

    void TerminalControl::RootGrid_OnLostFocus(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args) {
        m_isFocused = false;
        OutputDebugStringA("TerminalControl lost focus.\n");
        // TODO: Update cursor appearance (e.g : stop blinking)
    }

    void TerminalControl::RootGrid_OnPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args) {
        RootGrid().Focus(winrt::Microsoft::UI::Xaml::FocusState::Pointer);
        args.Handled(true);
    }

    void TerminalControl::RootGrid_OnKeyDown(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (!m_isFocused) {
            return;
        }

        std::string inputSequence;
        bool handled = true;

        switch (args.Key()) {
        case winrt::Windows::System::VirtualKey::Enter:
            inputSequence = "\r";
            break;

        case winrt::Windows::System::VirtualKey::Back:
            inputSequence = "\x7F";
            break;
        default:
            handled = false;
            break;
        }

        if (!inputSequence.empty()) {
            SendInputToPty(inputSequence);
        }

        if (handled) {
            args.Handled(true);
        }
    }

    void TerminalControl::RootGrid_OnCharacterReceived(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::CharacterReceivedRoutedEventArgs const& args) {
        if (!m_isFocused) {
            return;
        }

        wchar_t ch = args.Character();

        if ((ch < 0x20 && ch != L'\t' && ch != L'\n' && ch != L'\r') || ch == 0x7F) {
            return;
        }

        char utf8Buffer[5] = { 0 };
        int bytesWritten = WideCharToMultiByte(
            CP_UTF8, 0, &ch, 1, utf8Buffer, sizeof(utf8Buffer) - 1, nullptr, nullptr);

        if (bytesWritten > 0) {
            SendInputToPty(std::string(utf8Buffer, bytesWritten));
            args.Handled(true);
        }
        else {
            args.Handled(false);
        }
    }
}
