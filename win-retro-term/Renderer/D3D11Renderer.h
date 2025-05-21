#pragma once

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

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
    void ValidateDevice(); // For handling device lost scenarios

    void Render();
    void Present(); // Separated Present from Render for more control if needed

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

    // Cached Panel and properties
    winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel m_swapChainPanel{ nullptr };
    winrt::Windows::Foundation::Size m_logicalSize{ 0, 0 };
    float m_compositionScaleX = 1.0f;
    float m_compositionScaleY = 1.0f;
    D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_9_1; // Default, will be updated

    // Physical pixel size of the swap chain
    UINT m_renderTargetWidth = 0;
    UINT m_renderTargetHeight = 0;

    bool m_isInitialized = false;
    bool m_deviceLost = false;
};