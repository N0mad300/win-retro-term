#include "pch.h"
#include "D3D11Renderer.h"

#include <microsoft.ui.xaml.media.dxinterop.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.h>

#include <appmodel.h>
#include <shlobj.h>
#include <pathcch.h>

#pragma comment(lib, "Pathcch.lib")

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

    // Get the path to the application's package installation folder.
    // For unpackaged apps, you might need to get the executable's directory.
    wchar_t packagePath[MAX_PATH] = {};
    if (GetCurrentPackageFullName(nullptr, packagePath) == ERROR_INSUFFICIENT_BUFFER) {
        UINT32 length = 0;
        GetCurrentPackageFullName(&length, nullptr); // Get required length
        std::vector<wchar_t> tempPath(length);
        if (GetCurrentPackageFullName(&length, tempPath.data()) == ERROR_SUCCESS) {
            wcscpy_s(packagePath, MAX_PATH, tempPath.data());
        }
        else {
            // Fallback for unpackaged apps or if GetCurrentPackageFullName fails:
            // Get executable path
            GetModuleFileName(nullptr, packagePath, MAX_PATH);
            PathCchRemoveFileSpec(packagePath, MAX_PATH); // Remove the exe name
        }
    }
    else if (wcslen(packagePath) == 0) { // Could happen if not packaged and GetCurrentPackageFullName fails without ERROR_INSUFFICIENT_BUFFER
        // Fallback for unpackaged apps: Get executable path
        GetModuleFileName(nullptr, packagePath, MAX_PATH);
        PathCchRemoveFileSpec(packagePath, MAX_PATH); // Remove the exe name
    }

    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> baseFontSetBuilder;
    ThrowIfFailed(m_dwriteFactory->CreateFontSetBuilder(baseFontSetBuilder.GetAddressOf()));

    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
    ThrowIfFailed(baseFontSetBuilder.As(&fontSetBuilder));

    // Define the fonts you want to load, with their subdirectory and filename
    struct FontToLoad {
        const wchar_t* subDirectory;
        const wchar_t* fileName;
    };

    std::vector<FontToLoad> fonts = {
        { L"1971-ibm-3278", L"3270-Regular.ttf" },
        { L"1977-apple2", L"PrintChar21.ttf" }
    };

    bool atLeastOneFontLoaded = false;
    for (const auto& fontInfo : fonts) {
        wchar_t fontSubFolderPath[MAX_PATH] = {};
        wchar_t specificFontPath[MAX_PATH] = {};

        ThrowIfFailed(PathCchCombine(fontSubFolderPath, MAX_PATH, packagePath, L"Assets\\Fonts"));
        ThrowIfFailed(PathCchCombine(fontSubFolderPath, MAX_PATH, fontSubFolderPath, fontInfo.subDirectory));
        ThrowIfFailed(PathCchCombine(specificFontPath, MAX_PATH, fontSubFolderPath, fontInfo.fileName));

        Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile;
        HRESULT hrFontFile = m_dwriteFactory->CreateFontFileReference(specificFontPath, nullptr, &fontFile);

        if (SUCCEEDED(hrFontFile)) {
            ThrowIfFailed(fontSetBuilder->AddFontFile(fontFile.Get()));
            OutputDebugString((L"Successfully referenced font file: " + std::wstring(specificFontPath) + L"\n").c_str());
            atLeastOneFontLoaded = true;
        }
        else {
            OutputDebugString((L"Failed to create font file reference for: " + std::wstring(specificFontPath) + L" Error: 0x" + std::to_wstring(hrFontFile) + L"\n").c_str());
        }
    }

    if (atLeastOneFontLoaded) {
        Microsoft::WRL::ComPtr<IDWriteFontSet> fontSet;
        ThrowIfFailed(fontSetBuilder->CreateFontSet(&fontSet));
        ThrowIfFailed(m_dwriteFactory->CreateFontCollectionFromFontSet(fontSet.Get(), &m_retroFontCollection));
        if (m_retroFontCollection) {
            OutputDebugString(L"Custom font collection created from loaded fonts.\n");
        }
    }
    else {
        OutputDebugString(L"No custom fonts were successfully loaded into the font set.\n");
    }
}

void D3D11Renderer::Initialize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel, winrt::win_retro_term::Core::TerminalBuffer* buffer) {
    m_swapChainPanel = panel;
    m_terminalBufferPtr = buffer;
    CreateDeviceResources();
    CreateWindowSizeDependentResources();
    UpdateFontMetrics();
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
    CreateColorPaletteBrushes();
    CreateTextFormats();

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

    UpdateFontMetrics();

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

void D3D11Renderer::UpdateFontMetrics() {
    if (!m_dwriteFactory || !m_textFormat) return;

    Microsoft::WRL::ComPtr<IDWriteTextLayout> tempLayout;
    const wchar_t* testString = L"M";
    ThrowIfFailed(m_dwriteFactory->CreateTextLayout(
        testString,
        (UINT32)wcslen(testString),
        m_textFormat.Get(),
        1000.0f,
        1000.0f,
        &tempLayout
    ));

    DWRITE_TEXT_METRICS textMetrics;
    ThrowIfFailed(tempLayout->GetMetrics(&textMetrics));
    m_avgCharWidth = textMetrics.width / wcslen(testString);
    m_lineHeight = textMetrics.height;

    OutputDebugStringA(("Font Metrics Updated: CharW=" + std::to_string(m_avgCharWidth) + " LineH=" + std::to_string(m_lineHeight) + "\n").c_str());
}

float D3D11Renderer::GetFontCharWidth() const {
    return m_avgCharWidth > 0 ? m_avgCharWidth : 8.0f;
}
float D3D11Renderer::GetFontCharHeight() const {
    return m_lineHeight > 0 ? m_lineHeight : 16.0f;
}

D2D1_COLOR_F D3D11Renderer::GetD2DColor(winrt::win_retro_term::Core::AnsiColor color, bool isForeground) 
{
    static const D2D1_COLOR_F ansiPalette[] = {
        D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f),        // Black
        D2D1::ColorF(0.66f, 0.0f, 0.0f, 1.0f),       // Red (darker)
        D2D1::ColorF(0.0f, 0.66f, 0.0f, 1.0f),       // Green (darker)
        D2D1::ColorF(0.66f, 0.66f, 0.0f, 1.0f),      // Yellow (darker, often brown)
        D2D1::ColorF(0.0f, 0.0f, 0.66f, 1.0f),       // Blue (darker)
        D2D1::ColorF(0.66f, 0.0f, 0.66f, 1.0f),      // Magenta (darker)
        D2D1::ColorF(0.0f, 0.66f, 0.66f, 1.0f),      // Cyan (darker)
        D2D1::ColorF(0.82f, 0.82f, 0.82f, 1.0f),     // White (light gray)

        D2D1::ColorF(0.33f, 0.33f, 0.33f, 1.0f),     // Bright Black (dark gray)
        D2D1::ColorF(1.0f, 0.2f, 0.2f, 1.0f),        // Bright Red
        D2D1::ColorF(0.2f, 1.0f, 0.2f, 1.0f),        // Bright Green
        D2D1::ColorF(1.0f, 1.0f, 0.2f, 1.0f),        // Bright Yellow
        D2D1::ColorF(0.2f, 0.2f, 1.0f, 1.0f),        // Bright Blue
        D2D1::ColorF(1.0f, 0.2f, 1.0f, 1.0f),        // Bright Magenta
        D2D1::ColorF(0.2f, 1.0f, 1.0f, 1.0f),        // Bright Cyan
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f),        // Bright White
    };
    static const D2D1_COLOR_F defaultTermFg = D2D1::ColorF(0.82f, 0.82f, 0.82f, 1.0f); // Light Gray
    static const D2D1_COLOR_F defaultTermBg = D2D1::ColorF(0.02f, 0.02f, 0.08f, 1.0f); // Dark Blue (matches current clear)

    if (color == winrt::win_retro_term::Core::AnsiColor::Foreground) return defaultTermFg;
    if (color == winrt::win_retro_term::Core::AnsiColor::Background) return defaultTermBg;

    uint8_t index = static_cast<uint8_t>(color);
    if (index < ARRAYSIZE(ansiPalette)) {
        return ansiPalette[index];
    }
    return isForeground ? defaultTermFg : defaultTermBg;
}

void D3D11Renderer::CreateColorPaletteBrushes() {
    if (!m_d2dContext) return;
    m_colorBrushes.clear();
    m_colorBrushes.resize(18);

    for (uint8_t i = 0; i < 16; ++i) {
        ThrowIfFailed(m_d2dContext->CreateSolidColorBrush(
            GetD2DColor(static_cast<winrt::win_retro_term::Core::AnsiColor>(i), true),
            &m_colorBrushes[i]
        ));
    }

    ThrowIfFailed(m_d2dContext->CreateSolidColorBrush(
        GetD2DColor(winrt::win_retro_term::Core::AnsiColor::Foreground, true),
        &m_defaultFgBrush
    ));
    m_colorBrushes[static_cast<uint8_t>(winrt::win_retro_term::Core::AnsiColor::Foreground)] = m_defaultFgBrush;

    ThrowIfFailed(m_d2dContext->CreateSolidColorBrush(
        GetD2DColor(winrt::win_retro_term::Core::AnsiColor::Background, false),
        &m_defaultBgBrush
    ));
    m_colorBrushes[static_cast<uint8_t>(winrt::win_retro_term::Core::AnsiColor::Background)] = m_defaultBgBrush;
}

void D3D11Renderer::CreateTextFormats() {
    if (!m_dwriteFactory) return;

    const wchar_t* fontFamilyName = L"zrzef";
    float fontSize = 15.0f;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection = m_retroFontCollection;
    if (!fontCollection) { // Fallback if custom collection failed
        ThrowIfFailed(m_dwriteFactory->GetSystemFontCollection(&fontCollection, false));
        fontFamilyName = L"Consolas"; // Fallback font
    }

    // Normal
    ThrowIfFailed(m_dwriteFactory->CreateTextFormat(
        fontFamilyName, fontCollection.Get(), DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSize, L"en-US", &m_textFormatNormal));
    m_textFormatNormal->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_textFormatNormal->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // Bold
    ThrowIfFailed(m_dwriteFactory->CreateTextFormat(
        fontFamilyName, fontCollection.Get(), DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSize, L"en-US", &m_textFormatBold));
    m_textFormatBold->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_textFormatBold->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    m_textFormat = m_textFormatNormal;
    UpdateFontMetrics();
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
    if (!m_isInitialized || m_deviceLost || !m_terminalBufferPtr) return;
    if (!m_renderTargetView || !m_d2dContext || !m_d2dTargetBitmap || m_colorBrushes.empty()) return;

    D2D1_COLOR_F color = GetD2DColor(winrt::win_retro_term::Core::AnsiColor::Background, false);
    const float clearColor[4] = { color.r, color.g, color.b, color.a };
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    m_d2dContext->BeginDraw();
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

    const auto& screenData = m_terminalBufferPtr->GetScreenBuffer();
    int rows = m_terminalBufferPtr->GetRows();
    int cols = m_terminalBufferPtr->GetCols();

    float yPos = 5.0f; // Starting Y offset (DIPs)
    float xOffset = 5.0f; // Starting X offset (DIPs)
    float charWidth = GetFontCharWidth();  // From cached metrics
    float lineHeight = GetFontCharHeight(); // From cached metrics

    for (int r = 0; r < rows; ++r) {
        if (r >= screenData.size()) continue; // Should not happen

        // Render runs of characters with the same attributes
        int currentRunStartCol = 0;
        winrt::win_retro_term::Core::Cell firstCellInRun = screenData[r][0];

        for (int c = 0; c <= cols; ++c) { // Iterate one past last col to draw final run
            bool endOfLine = (c == cols);
            bool attributesChanged = false;
            winrt::win_retro_term::Core::Cell currentCell;

            if (!endOfLine) {
                currentCell = screenData[r][c];
                if (currentCell.foregroundColor != firstCellInRun.foregroundColor ||
                    currentCell.backgroundColor != firstCellInRun.backgroundColor ||
                    currentCell.attributes != firstCellInRun.attributes) {
                    attributesChanged = true;
                }
            }

            if (endOfLine || attributesChanged) {
                // Draw the run from currentRunStartCol to c-1
                int runLength = c - currentRunStartCol;
                if (runLength > 0) {
                    std::wstring runText;
                    runText.reserve(runLength);
                    for (int i = 0; i < runLength; ++i) {
                        runText += screenData[r][currentRunStartCol + i].character;
                    }

                    // Determine attributes for this run (from firstCellInRun)
                    winrt::win_retro_term::Core::AnsiColor fg = firstCellInRun.foregroundColor;
                    winrt::win_retro_term::Core::AnsiColor bg = firstCellInRun.backgroundColor;
                    winrt::win_retro_term::Core::CellAttributesFlags cellAttrs = firstCellInRun.attributes;

                    if ((cellAttrs & winrt::win_retro_term::Core::CellAttributesFlags::Inverse) != winrt::win_retro_term::Core::CellAttributesFlags::None) {
                        std::swap(fg, bg);
                    }

                    // Background fill for the run
                    ID2D1SolidColorBrush* bgBrush = m_colorBrushes[static_cast<uint8_t>(bg)].Get();
                    D2D1_RECT_F bgRect = D2D1::RectF(
                        xOffset + currentRunStartCol * charWidth,
                        yPos,
                        xOffset + c * charWidth, // Up to current column 'c'
                        yPos + lineHeight);
                    m_d2dContext->FillRectangle(&bgRect, bgBrush);

                    // Select TextFormat (Normal/Bold)
                    IDWriteTextFormat* currentTextFormat = m_textFormatNormal.Get();
                    if ((cellAttrs & winrt::win_retro_term::Core::CellAttributesFlags::Bold) != winrt::win_retro_term::Core::CellAttributesFlags::None) {
                        currentTextFormat = m_textFormatBold.Get();
                        // If Faint is also set, some terminals might prefer faint or normal
                        if ((cellAttrs & winrt::win_retro_term::Core::CellAttributesFlags::Dim) != winrt::win_retro_term::Core::CellAttributesFlags::None) {
                            // currentTextFormat = m_textFormatNormal.Get(); // Example: Faint overrides Bold
                        }
                    }
                    // TODO: Add Italic, BoldItalic if supported

                    // Foreground text brush
                    ID2D1SolidColorBrush* fgBrush = m_colorBrushes[static_cast<uint8_t>(fg)].Get();

                    // Text layout rect for the run
                    D2D1_RECT_F textLayoutRect = D2D1::RectF(
                        xOffset + currentRunStartCol * charWidth,
                        yPos,
                        xOffset + (currentRunStartCol + runLength) * charWidth + charWidth, // Give a bit extra for last char
                        yPos + lineHeight);

                    if (!runText.empty() && fgBrush && currentTextFormat &&
                        !((cellAttrs & winrt::win_retro_term::Core::CellAttributesFlags::Concealed) != winrt::win_retro_term::Core::CellAttributesFlags::None)) { // Don't draw concealed
                        m_d2dContext->DrawText(
                            runText.c_str(), (UINT32)runText.length(),
                            currentTextFormat, &textLayoutRect, fgBrush,
                            D2D1_DRAW_TEXT_OPTIONS_NONE // Or D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT if using color fonts
                        );
                    }
                    // TODO: Draw underline, strikethrough as separate lines/rects if needed
                }

                // Start new run
                if (!endOfLine) {
                    currentRunStartCol = c;
                    firstCellInRun = currentCell;
                }
            }
        }
        yPos += lineHeight;
    }

    // Render cursor
    int cursorR = m_terminalBufferPtr->GetCursorRow();
    int cursorC = m_terminalBufferPtr->GetCursorCol();
    float cursor_x_pos = 5.0f + cursorC * m_avgCharWidth;
    float cursor_y_pos = 5.0f + cursorR * m_lineHeight;

    // Draw a rectangle or block for the cursor
    D2D1_RECT_F cursorRect = D2D1::RectF(cursor_x_pos, cursor_y_pos, cursor_x_pos + m_avgCharWidth, cursor_y_pos + m_lineHeight);
    m_d2dContext->FillRectangle(&cursorRect, m_defaultFgBrush.Get());

    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        OutputDebugStringA("D2DERR_RECREATE_TARGET in Render. Marking device lost.\n");
        m_deviceLost = true;
        m_d2dContext->SetTarget(nullptr);
        m_d2dTargetBitmap = nullptr;
    }
    else {
        ThrowIfFailed(hr);
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
    m_textFormat = nullptr;
    m_d2dContext = nullptr;
    m_d2dDevice = nullptr;

    if (m_d3dContext) m_d3dContext->ClearState();
    if (m_d3dContext) m_d3dContext->Flush();

    m_d3dContext = nullptr;
    m_d3dDevice = nullptr; // This should be last for device-dependent resources
    m_isInitialized = false; // Mark as not initialized until re-created

    m_colorBrushes.clear();
    m_defaultFgBrush = nullptr;
    m_defaultBgBrush = nullptr;

    m_textFormatNormal = nullptr;
    m_textFormatBold = nullptr;
    m_textFormat = nullptr;
}