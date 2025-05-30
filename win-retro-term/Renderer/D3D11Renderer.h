#pragma once

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <d2d1_3.h>
#include <dwrite_3.h>

namespace winrt::Microsoft::UI::Xaml::Controls {
    struct SwapChainPanel;
}

namespace winrt::win_retro_term::Renderer { class D3D11Renderer; }

class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    void Initialize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel);
    void SetLogicalSize(winrt::Windows::Foundation::Size logicalSize);
    void SetCompositionScale(float compositionScaleX, float compositionScaleY);
    void ValidateDevice();

    void Render();
    void Present();

    void Suspend();
    void Resume();

    bool IsInitialized() const { return m_isInitialized; }

private:
    void CreateDeviceIndependentResources();
    void CreateDeviceResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();

    // DirectX Core Objects
    Microsoft::WRL::ComPtr<ID3D11Device1>         m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1>  m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>       m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;

    // New: Direct2D & DirectWrite Objects
    Microsoft::WRL::ComPtr<ID2D1Factory3>         m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory3>       m_dwriteFactory;
    Microsoft::WRL::ComPtr<ID2D1Device2>          m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext2>   m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1>          m_d2dTargetBitmap;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>  m_textBrush;
    Microsoft::WRL::ComPtr<IDWriteTextFormat>     m_textFormat;

    // Cached Panel and properties
    winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel m_swapChainPanel{ nullptr };
    winrt::Windows::Foundation::Size m_logicalSize{ 0, 0 };
    float m_compositionScaleX = 1.0f;
    float m_compositionScaleY = 1.0f;
    D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_9_1;

    // Physical pixel size of the swap chain
    UINT m_renderTargetWidth = 0;
    UINT m_renderTargetHeight = 0;

    bool m_isInitialized = false;
    bool m_deviceLost = false;
};