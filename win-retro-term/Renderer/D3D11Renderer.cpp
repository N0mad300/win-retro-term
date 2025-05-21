#include "pch.h"
#include "D3D11Renderer.h"
#include <windows.ui.xaml.media.dxinterop.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.h>

inline void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        winrt::throw_hresult(hr);
    }
}

D3D11Renderer::D3D11Renderer() {
    CreateDeviceIndependentResources();
}

D3D11Renderer::~D3D11Renderer() {
    ReleaseDeviceDependentResources();
}

void D3D11Renderer::CreateDeviceIndependentResources() {
    // Initialize D2D, DirectWrite factories here if needed
}

void D3D11Renderer::Initialize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel) {
    m_swapChainPanel = panel;
    CreateDeviceResources();
    CreateWindowSizeDependentResources(); // Initial setup based on panel's current size (if any)
    m_isInitialized = true;
}

void D3D11Renderer::CreateDeviceResources() {
    if (m_d3dDevice) return; // Already created

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // BGRA is good for XAML interop

#if defined(_DEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

    ThrowIfFailed(D3D11CreateDevice(
        nullptr,                    // Specify nullptr to use the default adapter.
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &device,                    // Returns the Direct3D device created.
        &m_featureLevel,            // Returns feature level of device created.
        &context                    // Returns the device immediate context.
    ));

    // Get ID3D11Device1 and ID3D11DeviceContext1
    ThrowIfFailed(device.As(&m_d3dDevice));
    ThrowIfFailed(context.As(&m_d3dContext));

    m_deviceLost = false;
}

void D3D11Renderer::CreateWindowSizeDependentResources() {
    if (!m_swapChainPanel || !m_d3dDevice) return;

    // Release existing RTV if any
    m_renderTargetView = nullptr;

    // Calculate the necessary swap chain dimensions.
    // The SwapChainPanel's dimensions are in logical DIPs.
    // We need to convert this to physical pixels using the composition scale.
    m_renderTargetWidth = static_cast<UINT>(std::max(1.0f, m_logicalSize.Width * m_compositionScaleX));
    m_renderTargetHeight = static_cast<UINT>(std::max(1.0f, m_logicalSize.Height * m_compositionScaleY));


    if (m_swapChain) {
        // If the swap chain already exists, resize it.
        HRESULT hr = m_swapChain->ResizeBuffers(
            2, // Double-buffered swap chain.
            m_renderTargetWidth,
            m_renderTargetHeight,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0 //DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING (if using variable refresh rate and supported)
        );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            m_deviceLost = true;
            // Don't proceed, HandleDeviceLost will be called.
            return;
        }
        else {
            ThrowIfFailed(hr);
        }
    }
    else {
        // Create a new swap chain using the same adapter as the Direct3D device.
        Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice;
        ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
        ThrowIfFailed(dxgiDevice->GetAdapter(&dxgiAdapter));

        Microsoft::WRL::ComPtr<IDXGIFactory3> dxgiFactory; // Use Factory3 for CreateSwapChainForComposition
        ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
        swapChainDesc.Width = m_renderTargetWidth;
        swapChainDesc.Height = m_renderTargetHeight;
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // Most common for XAML
        swapChainDesc.Stereo = false;
        swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2; // Use double-buffering to minimize latency.
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // Or DXGI_SCALING_ASPECT_RATIO_STRETCH
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // Recommended for modern UWP/WinUI
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE; // Or PREMULTIPLIED if needed
        swapChainDesc.Flags = 0; // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING (if using variable refresh rate)

        // This is the C++/WinRT way to get the IUnknown for the SwapChainPanel
        ::IUnknown* panelUnknown = static_cast<::IUnknown*>(winrt::get_unknown(m_swapChainPanel));

        ThrowIfFailed(dxgiFactory->CreateSwapChainForComposition(
            m_d3dDevice.Get(),
            &swapChainDesc,
            nullptr, // Don't restrict to any particular output.
            &m_swapChain
        ));

        // Associate the swap chain with the SwapChainPanel
        // This must be done via the ISwapChainPanelNative interface.
        Microsoft::WRL::ComPtr<ISwapChainPanelNative> panelNative;
        ThrowIfFailed(reinterpret_cast<IUnknown*>(winrt::get_unknown(m_swapChainPanel))->QueryInterface(IID_PPV_ARGS(&panelNative)));
        ThrowIfFailed(panelNative->SetSwapChain(m_swapChain.Get()));
    }

    // Create a render target view of the swap chain's back buffer.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        &m_renderTargetView
    ));

    // Set the viewport.
    D3D11_VIEWPORT viewport = { 0 };
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_renderTargetWidth);
    viewport.Height = static_cast<float>(m_renderTargetHeight);
    viewport.MinDepth = D3D11_MIN_DEPTH;
    viewport.MaxDepth = D3D11_MAX_DEPTH;
    m_d3dContext->RSSetViewports(1, &viewport);
}


void D3D11Renderer::SetLogicalSize(winrt::Windows::Foundation::Size logicalSize) {
    if (m_logicalSize.Width != logicalSize.Width || m_logicalSize.Height != logicalSize.Height) {
        m_logicalSize = logicalSize;
        CreateWindowSizeDependentResources();
    }
}

void D3D11Renderer::SetCompositionScale(float compositionScaleX, float compositionScaleY) {
    if (m_compositionScaleX != compositionScaleX || m_compositionScaleY != compositionScaleY) {
        m_compositionScaleX = compositionScaleX;
        m_compositionScaleY = compositionScaleY;
        CreateWindowSizeDependentResources();
    }
}

void D3D11Renderer::ValidateDevice() {
    if (!m_d3dDevice) return;

    Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice;
    ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

    Microsoft::WRL::ComPtr<IDXGIAdapter> deviceAdapter;
    ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

    // Get the factory that created the device.
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory; // Factory2 is fine for EnumAdapters1
    ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

    Microsoft::WRL::ComPtr<IDXGIAdapter1> currentAdapter;
    // Check if the default adapter (index 0) has changed.
    if (SUCCEEDED(dxgiFactory->EnumAdapters1(0, &currentAdapter))) {
        if (currentAdapter.Get() != deviceAdapter.Get()) {
            // The default adapter has changed. The device is lost.
            m_deviceLost = true;
        }
    }
    else {
        // Failed to get the current adapter. Assume device is lost.
        m_deviceLost = true;
    }

    if (m_deviceLost) {
        // Ensure resources are recreated by calling Release and then Create.
        ReleaseDeviceDependentResources(); // This sets m_isInitialized = false
        // The render loop or next interaction should trigger re-creation
        // For now, we'll explicitly recreate here for simplicity,
        // but a more robust system might defer this.
        CreateDeviceResources();
        if (!m_deviceLost) {
            CreateWindowSizeDependentResources();
            if (!m_deviceLost) {
                m_isInitialized = true;
            }
        }
    }
}


void D3D11Renderer::Render() {
    if (!m_isInitialized || m_deviceLost) return;
    if (!m_renderTargetView) return; // Not ready yet

    // For now, just clear the screen to a color
    const float cornflowerBlue[] = { 0.392f, 0.584f, 0.929f, 1.000f }; // RGBA
    // Or a dark terminal color:
    // const float darkColor[] = { 0.05f, 0.05f, 0.15f, 1.0f };
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), cornflowerBlue);

    // This is where we are going to draw our terminal text texture, apply shaders, etc.
}

void D3D11Renderer::Present() {
    if (!m_isInitialized || m_deviceLost || !m_swapChain) return;

    DXGI_PRESENT_PARAMETERS parameters = { 0 }; // No parameters for FLIP_SEQUENTIAL
    HRESULT hr = m_swapChain->Present(1, 0); // Present with vsync (1)

    // If the device was removed either by a disconnect or a driver upgrade, we
    // must recreate all device resources.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        m_deviceLost = true;
        ValidateDevice(); // This will attempt to recreate
    }
    else {
        ThrowIfFailed(hr);
    }
}

void D3D11Renderer::Suspend() {
    // Called when the app is suspending. Trim DXGI resource usage.
    Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice;
    if (SUCCEEDED(m_d3dDevice.As(&dxgiDevice))) {
        dxgiDevice->Trim();
    }
}

void D3D11Renderer::Resume() {
    // Called when app resumes. Nothing specific to do for D3D11 here typically.
}

void D3D11Renderer::ReleaseDeviceDependentResources() {
    m_renderTargetView = nullptr;
    m_swapChain = nullptr;
    if (m_d3dContext) m_d3dContext->ClearState(); // Good practice
    if (m_d3dContext) m_d3dContext->Flush();      // Ensure commands are processed
    m_d3dContext = nullptr;
    m_d3dDevice = nullptr; // This should be last for device-dependent resources
    m_isInitialized = false; // Mark as not initialized until re-created
}