#include "pch.h"
#include "TerminalControl.xaml.h"
#if __has_include("TerminalControl.g.cpp")
#include "TerminalControl.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::win_retro_term::implementation
{
    TerminalControl::TerminalControl()
    {
        m_renderer = std::make_unique<D3D11Renderer>();

        this->Loaded({ this, &TerminalControl::OnLoaded });
        this->Unloaded({ this, &TerminalControl::OnUnloaded });
    }

    TerminalControl::~TerminalControl()
    {
    }

    void TerminalControl::OnLoaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        m_renderer->Initialize(dxSwapChainPanel());

        m_renderer->SetLogicalSize({ (float)dxSwapChainPanel().ActualWidth(), (float)dxSwapChainPanel().ActualHeight() });
        m_renderer->SetCompositionScale(dxSwapChainPanel().CompositionScaleX(), dxSwapChainPanel().CompositionScaleY());

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
        m_renderer.reset();
    }

    void TerminalControl::OnSizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args)
    {
        if (m_renderer)
        {
            m_renderer->SetLogicalSize(args.NewSize());
        }
    }

    void TerminalControl::OnCompositionScaleChanged(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& sender, winrt::Windows::Foundation::IInspectable const& args)
    {
        if (m_renderer)
        {
            m_renderer->SetCompositionScale(sender.CompositionScaleX(), sender.CompositionScaleY());
        }
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
