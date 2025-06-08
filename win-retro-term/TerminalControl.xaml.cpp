#include "pch.h"
#include "TerminalControl.xaml.h"
#if __has_include("TerminalControl.g.cpp")
#include "TerminalControl.g.cpp"
#endif

#include <winrt/Microsoft.UI.Input.h>
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

        bool altDown = (winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Menu) & 
            winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;
        bool ctrlDown = (winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Control) &
            winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;
        bool shiftDown = (winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Shift) &
            winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;

        bool appCursorMode = m_terminalBuffer ? m_terminalBuffer->IsApplicationCursorKeysMode() : false;
        bool appKeypadMode = m_terminalBuffer ? m_terminalBuffer->IsApplicationKeypadMode() : false;

        if (ctrlDown) {
            winrt::Windows::System::VirtualKey key = args.Key();
            if (key >= winrt::Windows::System::VirtualKey::A && key <= winrt::Windows::System::VirtualKey::Z) {
                char ctrlChar = static_cast<char>(
                    static_cast<int>(key) - static_cast<int>(winrt::Windows::System::VirtualKey::A) + 1
                    );
                inputSequence += ctrlChar;
            }
            else {
                handled = false;
            }
        }

        switch (args.Key()) {
        case winrt::Windows::System::VirtualKey::Enter:
            if (args.OriginalKey() == (winrt::Windows::System::VirtualKey)0x0C && appKeypadMode) {
                inputSequence = "\x1BOM";
            }
            else {
                inputSequence = "\r";
            }
            break;
        case winrt::Windows::System::VirtualKey::Back:
            inputSequence = "\x7F";
            break;
        case winrt::Windows::System::VirtualKey::Tab:
            inputSequence = "\t";
            break;
        case winrt::Windows::System::VirtualKey::Escape:
            inputSequence = "\x1B";
            break;

        case winrt::Windows::System::VirtualKey::Up:
            inputSequence = appCursorMode ? "\x1BOA" : (ctrlDown ? "\x1B[1;5A" : (shiftDown ? "\x1B[1;2A" : "\x1B[A"));
            break;
        case winrt::Windows::System::VirtualKey::Down:
            inputSequence = appCursorMode ? "\x1BOB" : (ctrlDown ? "\x1B[1;5B" : (shiftDown ? "\x1B[1;2B" : "\x1B[B"));
            break;
        case winrt::Windows::System::VirtualKey::Right:
            inputSequence = appCursorMode ? "\x1BOC" : (ctrlDown ? "\x1B[1;5C" : (shiftDown ? "\x1B[1;2C" : "\x1B[C"));
            break;
        case winrt::Windows::System::VirtualKey::Left:
            inputSequence = appCursorMode ? "\x1BOD" : (ctrlDown ? "\x1B[1;5D" : (shiftDown ? "\x1B[1;2D" : "\x1B[D"));
            break;

        case winrt::Windows::System::VirtualKey::Home:
            inputSequence = ctrlDown ? "\x1B[1;5H" : (appCursorMode ? "\x1BOH" : "\x1B[H");
            break;
        case winrt::Windows::System::VirtualKey::End:
            inputSequence = ctrlDown ? "\x1B[1;5F" : (appCursorMode ? "\x1BOF" : "\x1B[4~");
            break;
        case winrt::Windows::System::VirtualKey::PageUp:
            inputSequence = ctrlDown ? "\x1B[5;5~" : "\x1B[5~";
            break;
        case winrt::Windows::System::VirtualKey::PageDown:
            inputSequence = ctrlDown ? "\x1B[6;5~" : "\x1B[6~";
            break;
        case winrt::Windows::System::VirtualKey::Delete:
            inputSequence = ctrlDown ? "\x1B[3;5~" : "\x1B[3~";
            break;
        case winrt::Windows::System::VirtualKey::Insert:
            inputSequence = ctrlDown ? "\x1B[2;5~" : (shiftDown ? "\x1B[2;2~" : "\x1B[2~");
            break;

        case winrt::Windows::System::VirtualKey::F1:
            inputSequence = shiftDown ? "\x1B[1;2P" : (ctrlDown ? "\x1B[1;5P" : "\x1BOP"); break;
        case winrt::Windows::System::VirtualKey::F2:    
            inputSequence = shiftDown ? "\x1B[1;2Q" : (ctrlDown ? "\x1B[1;5Q" : "\x1BOQ"); break;
        case winrt::Windows::System::VirtualKey::F3:    
            inputSequence = shiftDown ? "\x1B[1;2R" : (ctrlDown ? "\x1B[1;5R" : "\x1BOR"); break;
        case winrt::Windows::System::VirtualKey::F4:    
            inputSequence = shiftDown ? "\x1B[1;2S" : (ctrlDown ? "\x1B[1;5S" : "\x1BOS"); break;
        case winrt::Windows::System::VirtualKey::F5:    
            inputSequence = shiftDown ? "\x1B[15;2~" : (ctrlDown ? "\x1B[15;5~" : "\x1B[15~"); break;
        case winrt::Windows::System::VirtualKey::F6:    
            inputSequence = shiftDown ? "\x1B[17;2~" : (ctrlDown ? "\x1B[17;5~" : "\x1B[17~"); break;
        case winrt::Windows::System::VirtualKey::F7:    
            inputSequence = shiftDown ? "\x1B[18;2~" : (ctrlDown ? "\x1B[18;5~" : "\x1B[18~"); break;
        case winrt::Windows::System::VirtualKey::F8:    
            inputSequence = shiftDown ? "\x1B[19;2~" : (ctrlDown ? "\x1B[19;5~" : "\x1B[19~"); break;
        case winrt::Windows::System::VirtualKey::F9:    
            inputSequence = shiftDown ? "\x1B[20;2~" : (ctrlDown ? "\x1B[20;5~" : "\x1B[20~"); break;
        case winrt::Windows::System::VirtualKey::F10:   
            inputSequence = shiftDown ? "\x1B[21;2~" : (ctrlDown ? "\x1B[21;5~" : "\x1B[21~"); break;
        case winrt::Windows::System::VirtualKey::F11:   
            inputSequence = shiftDown ? "\x1B[23;2~" : (ctrlDown ? "\x1B[23;5~" : "\x1B[23~"); break;
        case winrt::Windows::System::VirtualKey::F12:   
            inputSequence = shiftDown ? "\x1B[24;2~" : (ctrlDown ? "\x1B[24;5~" : "\x1B[24~"); break;

        case winrt::Windows::System::VirtualKey::NumberPad0: 
            inputSequence = appKeypadMode ? "\x1BOp" : "0"; break;
        case winrt::Windows::System::VirtualKey::NumberPad1: 
            inputSequence = appKeypadMode ? "\x1BOq" : "1"; break;
        case winrt::Windows::System::VirtualKey::NumberPad2: 
            inputSequence = appKeypadMode ? "\x1BOr" : "2"; break;
        case winrt::Windows::System::VirtualKey::NumberPad3: 
            inputSequence = appKeypadMode ? "\x1BOs" : "3"; break;
        case winrt::Windows::System::VirtualKey::NumberPad4: 
            inputSequence = appKeypadMode ? "\x1BOt" : "4"; break;
        case winrt::Windows::System::VirtualKey::NumberPad5: 
            inputSequence = appKeypadMode ? "\x1BOu" : "5"; break;
        case winrt::Windows::System::VirtualKey::NumberPad6: 
            inputSequence = appKeypadMode ? "\x1BOv" : "6"; break;
        case winrt::Windows::System::VirtualKey::NumberPad7: 
            inputSequence = appKeypadMode ? "\x1BOw" : "7"; break;
        case winrt::Windows::System::VirtualKey::NumberPad8: 
            inputSequence = appKeypadMode ? "\x1BOx" : "8"; break;
        case winrt::Windows::System::VirtualKey::NumberPad9: 
            inputSequence = appKeypadMode ? "\x1BOy" : "9"; break;

        case winrt::Windows::System::VirtualKey::Decimal:    
            inputSequence = appKeypadMode ? "\x1BOn" : "."; break;
        case winrt::Windows::System::VirtualKey::Add:        
            inputSequence = appKeypadMode ? "\x1BOk" : "+"; break;
        case winrt::Windows::System::VirtualKey::Subtract:   
            inputSequence = appKeypadMode ? "\x1BOj" : "-"; break;
        case winrt::Windows::System::VirtualKey::Multiply:   
            inputSequence = appKeypadMode ? "\x1BOm" : "*"; break;
        case winrt::Windows::System::VirtualKey::Divide:     
            inputSequence = appKeypadMode ? "\x1BOo" : "/"; break;

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
