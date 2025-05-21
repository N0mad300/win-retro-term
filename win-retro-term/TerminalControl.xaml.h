#pragma once

#include "TerminalControl.g.h"
#include "Renderer/D3D11Renderer.h"
#include <winrt/Microsoft.UI.Xaml.Media.h>

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

        // CompositionTarget.Rendering event handler
        void OnRendering(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);

    private:
        std::unique_ptr<D3D11Renderer> m_renderer;
        winrt::event_token m_renderingEventToken{};
    };
}

namespace winrt::win_retro_term::factory_implementation
{
    struct TerminalControl : TerminalControlT<TerminalControl, implementation::TerminalControl>
    {
    };
}
