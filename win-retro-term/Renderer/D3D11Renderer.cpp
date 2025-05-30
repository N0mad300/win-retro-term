#include "pch.h"
#include "D3D11Renderer.h"

#include <microsoft.ui.xaml.media.dxinterop.h>
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
    m_dwriteFactory = nullptr;
    m_d2dFactory = nullptr;
}

void D3D11Renderer::CreateDeviceIndependentResources() {

    // Create D2D Factory
    D2D1_FACTORY_OPTIONS d2dFactoryOptions = {};
#if defined(_DEBUG)
    d2dFactoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    ThrowIfFailed(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory3),
        &d2dFactoryOptions,
        &m_d2dFactory
    ));

    // Create DirectWrite Factory
    ThrowIfFailed(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory3),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
    ));
}

void D3D11Renderer::Initialize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel) {
    m_swapChainPanel = panel;
    CreateDeviceResources();
    CreateWindowSizeDependentResources();
    m_isInitialized = true;
}

void D3D11Renderer::CreateDeviceResources() {
    if (m_d3dDevice) return;

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

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

    // D2D/DWrite Device Resource Creation
    Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice;
    ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

    // Create D2D Device
    ThrowIfFailed(m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice));

    // Create D2D Device Context
    ThrowIfFailed(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext));

    // Create a solid color brush for text
    ThrowIfFailed(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightSkyBlue), &m_textBrush));

    // Create a DirectWrite text format
    ThrowIfFailed(m_dwriteFactory->CreateTextFormat(
        L"Consolas",                // Font family name
        nullptr,                    // Font collection (nullptr for system collection)
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.0f,                      // Font size in DIPs (Device Independent Pixels)
        L"en-US",                   // Locale name
        &m_textFormat
    ));

    ThrowIfFailed(m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
    ThrowIfFailed(m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

    m_deviceLost = false;
}

void D3D11Renderer::CreateWindowSizeDependentResources() {
    if (!m_swapChainPanel || !m_d3dDevice) return;

    // Release D2D target bitmap first, as it depends on the DXGI surface
    m_d2dContext->SetTarget(nullptr);
    m_d2dTargetBitmap = nullptr;
    m_renderTargetView = nullptr; // Release D3D RTV

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

    // Create D2D target bitmap and set it on the D2D context
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), // Use PREMULTIPLIED for good alpha blending
        m_compositionScaleX * 96.0f, // DPI X (CompositionScale is scale factor relative to 96 DPI)
        m_compositionScaleY * 96.0f  // DPI Y
    );

    Microsoft::WRL::ComPtr<IDXGISurface2> dxgiSurface;
    ThrowIfFailed(backBuffer.As(&dxgiSurface)); // Get the DXGI surface from the D3D back buffer
    ThrowIfFailed(m_d2dContext->CreateBitmapFromDxgiSurface(dxgiSurface.Get(), &bitmapProperties, &m_d2dTargetBitmap));

    m_d2dContext->SetTarget(m_d2dTargetBitmap.Get()); // Set D2D context to draw to this bitmap

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
    if (!m_renderTargetView || !m_d2dContext || !m_d2dTargetBitmap) return;

    // Clear the D3D render target (background color)
    const float darkTerminalBackground[] = { 0.02f, 0.02f, 0.08f, 1.0f }; // A very dark blue
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), darkTerminalBackground);

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0, 0, 0, 0));
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

    std::wstring sampleText = L"Hello Terminal! - DirectWrite on D3D11\nLine 2: {}()<>$%^&*";
    D2D1_RECT_F textLayoutRect = D2D1::RectF(
        10.0f,  // Left (in DIPs)
        10.0f,  // Top (in DIPs)
        static_cast<float>(m_logicalSize.Width - 10.0f),  // Right (in DIPs)
        static_cast<float>(m_logicalSize.Height - 10.0f) // Bottom (in DIPs)
    );

    if (m_textBrush && m_textFormat) {
        m_d2dContext->DrawText(
            sampleText.c_str(),
            static_cast<UINT32>(sampleText.length()),
            m_textFormat.Get(),
            &textLayoutRect,
            m_textBrush.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );
    }

    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        // The D2D target (our D3D surface) was lost.
        // This usually means the D3D device itself was lost.
        // Mark device as lost and try to recreate on next frame/ValidateDevice.
        OutputDebugStringA("D2DERR_RECREATE_TARGET in Render. Marking device lost.\n");
        m_deviceLost = true;
        // Release D2D-specific size resources so they are recreated
        m_d2dContext->SetTarget(nullptr);
        m_d2dTargetBitmap = nullptr;
    }
    else {
        ThrowIfFailed(hr); // Throw for other D2D errors
    }
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

    if (m_d2dContext) m_d2dContext->SetTarget(nullptr);
    m_d2dTargetBitmap = nullptr;
    m_textBrush = nullptr;
    m_textFormat = nullptr;
    m_d2dContext = nullptr;
    m_d2dDevice = nullptr;

    if (m_d3dContext) m_d3dContext->ClearState();
    if (m_d3dContext) m_d3dContext->Flush();

    m_d3dContext = nullptr;
    m_d3dDevice = nullptr; // This should be last for device-dependent resources
    m_isInitialized = false; // Mark as not initialized until re-created
}