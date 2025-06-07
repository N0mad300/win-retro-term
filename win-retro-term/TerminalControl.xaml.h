#pragma once

#include "TerminalControl.g.h"

#include "Renderer/D3D11Renderer.h"
#include "Core/ConPtyProcess.h"
#include "Core/TerminalBuffer.h"
#include "Core/AnsiParser.h"

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Dispatching.h>

namespace winrt::win_retro_term::implementation
{
    struct TerminalControl : TerminalControlT<TerminalControl>
    {
        TerminalControl();
        ~TerminalControl();

        void OnLoaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnUnloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnSizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnCompositionScaleChanged(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& sender, winrt::Windows::Foundation::IInspectable const& args);
        void OnRendering(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);

        void RootGrid_OnGotFocus(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RootGrid_OnLostFocus(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RootGrid_OnPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

        void RootGrid_OnKeyDown(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void RootGrid_OnCharacterReceived(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::CharacterReceivedRoutedEventArgs const& args);

    private:
        void InitializePtyAndBuffer();
        void PtyDataReceived(const char* buffer, size_t length);
        void UpdateTerminalSize();
        void SendInputToPty(const std::string& utf8Input);

        std::unique_ptr<D3D11Renderer> m_renderer;
        std::unique_ptr<ConPtyProcess> m_ptyProcess;
        std::unique_ptr<Core::TerminalBuffer> m_terminalBuffer;
        std::unique_ptr<Core::AnsiParser> m_ansiParser;

        winrt::event_token m_renderingEventToken{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        float m_charWidthApprox = 8.0f;
        float m_charHeightApprox = 16.0f;

        bool m_isFocused = false; // For cursor rendering later
    };
}

namespace winrt::win_retro_term::factory_implementation
{
    struct TerminalControl : TerminalControlT<TerminalControl, implementation::TerminalControl>
    {
    };
}
