#include "platform/PresentationWindow.h"

#include <d3dcompiler.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <algorithm>
#include <array>
#include <format>

extern "C" HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(
    IDXGIDevice* dxgi_device,
    IInspectable** graphics_device);

namespace outframe {
namespace {

using Microsoft::WRL::ComPtr;
using winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool;
using winrt::Windows::Graphics::Capture::GraphicsCaptureItem;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
using winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;

constexpr wchar_t kPresentationClassName[] = L"OutframePresentationWindow";
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT kFrameIntervalMs = 8;

constexpr char kVertexShader[] = R"(
struct VertexOut {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOut main(uint vertex_id : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 uvs[3] = {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };

    VertexOut output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.uv = uvs[vertex_id];
    return output;
}
)";

constexpr char kPixelShader[] = R"(
Texture2D source_texture : register(t0);
SamplerState source_sampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    return source_texture.Sample(source_sampler, uv);
}
)";

template <typename T>
void SafeReset(T& value) {
    if (value) {
        value = nullptr;
    }
}

ComPtr<ID3DBlob> CompileShader(const char* source, const char* entry_point, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        source,
        strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entry_point,
        target,
        flags,
        0,
        &shader,
        &errors);

    if (FAILED(result)) {
        return nullptr;
    }

    return shader;
}

GraphicsCaptureItem CreateCaptureItemForWindow(HWND hwnd) {
    auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::com_ptr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item;
    winrt::check_hresult(interop->CreateForWindow(
        hwnd,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        item.put_void()));
    return item.as<GraphicsCaptureItem>();
}

IDirect3DDevice CreateCaptureDevice(ID3D11Device* d3d_device) {
    ComPtr<IDXGIDevice> dxgi_device;
    winrt::check_hresult(d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device)));

    winrt::com_ptr<IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), inspectable.put()));
    return inspectable.as<IDirect3DDevice>();
}

} // namespace

PresentationWindow::PresentationWindow(HINSTANCE instance, WindowInfo target)
    : instance_(instance), target_(std::move(target)) {}

PresentationWindow::~PresentationWindow() {
    SafeReset(capture_session_);
    SafeReset(frame_pool_);

    if (IsOpen()) {
        DestroyWindow(hwnd_);
    }
}

bool PresentationWindow::Create() {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpszClassName = kPresentationClassName;
    window_class.lpfnWndProc = PresentationWindow::WindowProc;
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (!RegisterClassExW(&window_class)) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kPresentationClassName,
        L"Outframe Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        720,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

bool PresentationWindow::IsOpen() const {
    return hwnd_ && IsWindow(hwnd_);
}

void PresentationWindow::Focus() {
    if (IsOpen()) {
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
    }
}

LRESULT CALLBACK PresentationWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    PresentationWindow* window = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<PresentationWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<PresentationWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(message, wparam, lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT PresentationWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        render_ready_ = InitializeDirect3D();
        capture_ready_ = render_ready_ && InitializeCapture();
        gdi_fallback_ = !capture_ready_;
        SetTimer(hwnd_, kFrameTimer, kFrameIntervalMs, nullptr);
        return 0;
    case WM_SIZE:
        width_ = LOWORD(lparam);
        height_ = HIWORD(lparam);
        ResizeSwapChain();
        return 0;
    case WM_TIMER:
        if (wparam == kFrameTimer) {
            Render();
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd_);
            return 0;
        }
        break;
    case WM_PAINT:
        if (render_ready_ && capture_ready_) {
            Render();
            ValidateRect(hwnd_, nullptr);
        } else if (gdi_fallback_) {
            PaintGdiPreview();
        } else {
            PaintFallback(capture_status_.empty() ? L"Preview is not ready." : capture_status_.c_str());
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kFrameTimer);
        SafeReset(capture_session_);
        SafeReset(frame_pool_);
        hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, wparam, lparam);
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

bool PresentationWindow::InitializeDirect3D() {
    RECT client{};
    GetClientRect(hwnd_, &client);
    width_ = std::max(1, static_cast<int>(client.right - client.left));
    height_ = std::max(1, static_cast<int>(client.bottom - client.top));

    return CreateDeviceResources() && CreateSwapChainResources() && CreateShaders();
}

bool PresentationWindow::CreateDeviceResources() {
    constexpr std::array<D3D_FEATURE_LEVEL, 4> feature_levels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL selected_feature_level{};
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        feature_levels.data(),
        static_cast<UINT>(feature_levels.size()),
        D3D11_SDK_VERSION,
        &d3d_device_,
        &selected_feature_level,
        &d3d_context_);

#if defined(_DEBUG)
    if (FAILED(result)) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            feature_levels.data(),
            static_cast<UINT>(feature_levels.size()),
            D3D11_SDK_VERSION,
            &d3d_device_,
            &selected_feature_level,
            &d3d_context_);
    }
#endif

    return SUCCEEDED(result);
}

bool PresentationWindow::CreateSwapChainResources() {
    ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(d3d_device_.As(&dxgi_device))) {
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(&adapter))) {
        return false;
    }

    ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.Width = static_cast<UINT>(std::max(1, width_));
    swap_chain_desc.Height = static_cast<UINT>(std::max(1, height_));
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    if (FAILED(factory->CreateSwapChainForHwnd(
            d3d_device_.Get(),
            hwnd_,
            &swap_chain_desc,
            nullptr,
            nullptr,
            &swap_chain_))) {
        return false;
    }

    ComPtr<ID3D11Texture2D> back_buffer;
    if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
        return false;
    }

    return SUCCEEDED(d3d_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_));
}

bool PresentationWindow::CreateShaders() {
    ComPtr<ID3DBlob> vertex_shader_blob = CompileShader(kVertexShader, "main", "vs_5_0");
    ComPtr<ID3DBlob> pixel_shader_blob = CompileShader(kPixelShader, "main", "ps_5_0");
    if (!vertex_shader_blob || !pixel_shader_blob) {
        return false;
    }

    if (FAILED(d3d_device_->CreateVertexShader(
            vertex_shader_blob->GetBufferPointer(),
            vertex_shader_blob->GetBufferSize(),
            nullptr,
            &vertex_shader_))) {
        return false;
    }

    if (FAILED(d3d_device_->CreatePixelShader(
            pixel_shader_blob->GetBufferPointer(),
            pixel_shader_blob->GetBufferSize(),
            nullptr,
            &pixel_shader_))) {
        return false;
    }

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

    return SUCCEEDED(d3d_device_->CreateSamplerState(&sampler_desc, &sampler_state_));
}

bool PresentationWindow::InitializeCapture() {
    try {
        if (!winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported()) {
            capture_status_ = L"Windows Graphics Capture is not supported or disabled on this system.";
            return false;
        }

        capture_item_ = CreateCaptureItemForWindow(target_.hwnd);
        capture_device_ = CreateCaptureDevice(d3d_device_.Get());
        frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
            capture_device_,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            capture_item_.Size());
        capture_session_ = frame_pool_.CreateCaptureSession(capture_item_);
        capture_session_.StartCapture();
        capture_status_.clear();
        return true;
    } catch (const winrt::hresult_error& error) {
        capture_status_ = std::format(L"Windows Graphics Capture failed: 0x{:08X} {}", static_cast<uint32_t>(error.code()), error.message().c_str());
        return false;
    } catch (...) {
        capture_status_ = L"Windows Graphics Capture failed with an unknown error.";
        return false;
    }
}

void PresentationWindow::ReleaseSwapChainResources() {
    render_target_view_.Reset();
    current_frame_view_.Reset();
}

void PresentationWindow::ResizeSwapChain() {
    if (!swap_chain_ || width_ <= 0 || height_ <= 0) {
        return;
    }

    ReleaseSwapChainResources();
    if (FAILED(swap_chain_->ResizeBuffers(0, static_cast<UINT>(width_), static_cast<UINT>(height_), DXGI_FORMAT_UNKNOWN, 0))) {
        render_ready_ = false;
        return;
    }

    ComPtr<ID3D11Texture2D> back_buffer;
    if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
        render_ready_ = false;
        return;
    }

    render_ready_ = SUCCEEDED(d3d_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_));
}

void PresentationWindow::Render() {
    if (!render_ready_ || !capture_ready_ || !swap_chain_ || !render_target_view_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    try {
        const auto frame = frame_pool_.TryGetNextFrame();
        if (frame) {
            const auto surface = frame.Surface();
            const auto access =
                surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

            ComPtr<ID3D11Texture2D> frame_texture;
            if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&frame_texture)))) {
                current_frame_view_.Reset();
                d3d_device_->CreateShaderResourceView(frame_texture.Get(), nullptr, &current_frame_view_);
            }
        }
    } catch (...) {
        capture_ready_ = false;
        gdi_fallback_ = true;
        capture_status_ = L"Windows Graphics Capture stopped. Falling back to compatibility preview.";
        InvalidateRect(hwnd_, nullptr, TRUE);
        return;
    }

    const FLOAT clear_color[4]{0.03f, 0.04f, 0.055f, 1.0f};
    d3d_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
    d3d_context_->ClearRenderTargetView(render_target_view_.Get(), clear_color);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(std::max(1, width_));
    viewport.Height = static_cast<float>(std::max(1, height_));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d_context_->RSSetViewports(1, &viewport);

    if (current_frame_view_) {
        d3d_context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d_context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        d3d_context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
        d3d_context_->PSSetSamplers(0, 1, sampler_state_.GetAddressOf());
        d3d_context_->PSSetShaderResources(0, 1, current_frame_view_.GetAddressOf());
        d3d_context_->Draw(3, 0);

        ID3D11ShaderResourceView* null_view = nullptr;
        d3d_context_->PSSetShaderResources(0, 1, &null_view);
    }

    swap_chain_->Present(1, 0);
}

void PresentationWindow::PaintGdiPreview() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(hwnd_, &paint);

    RECT client{};
    GetClientRect(hwnd_, &client);

    HBRUSH background = CreateSolidBrush(RGB(8, 10, 14));
    FillRect(dc, &client, background);
    DeleteObject(background);

    if (!IsWindow(target_.hwnd) || IsIconic(target_.hwnd)) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(230, 235, 240));
        DrawTextW(dc, L"Target window is unavailable or minimized.", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd_, &paint);
        return;
    }

    RECT source_bounds{};
    if (!GetWindowRect(target_.hwnd, &source_bounds)) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(230, 235, 240));
        DrawTextW(dc, L"Could not read the target window bounds.", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd_, &paint);
        return;
    }

    const int source_width = std::max(1, static_cast<int>(source_bounds.right - source_bounds.left));
    const int source_height = std::max(1, static_cast<int>(source_bounds.bottom - source_bounds.top));
    const int destination_width = std::max(1, static_cast<int>(client.right - client.left));
    const int destination_height = std::max(1, static_cast<int>(client.bottom - client.top));

    const double source_aspect = static_cast<double>(source_width) / static_cast<double>(source_height);
    const double destination_aspect = static_cast<double>(destination_width) / static_cast<double>(destination_height);

    RECT fitted = client;
    if (destination_aspect > source_aspect) {
        const int width = static_cast<int>(destination_height * source_aspect);
        fitted.left = client.left + (destination_width - width) / 2;
        fitted.right = fitted.left + width;
    } else {
        const int height = static_cast<int>(destination_width / source_aspect);
        fitted.top = client.top + (destination_height - height) / 2;
        fitted.bottom = fitted.top + height;
    }

    HDC source_dc = GetWindowDC(target_.hwnd);
    if (source_dc) {
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchBlt(
            dc,
            fitted.left,
            fitted.top,
            fitted.right - fitted.left,
            fitted.bottom - fitted.top,
            source_dc,
            0,
            0,
            source_width,
            source_height,
            SRCCOPY);
        ReleaseDC(target_.hwnd, source_dc);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    const std::wstring overlay = std::format(
        L"Compatibility preview: {}  |  WGC unavailable  |  Esc closes",
        target_.title);
    RECT overlay_rect{18, 14, client.right - 18, 46};
    DrawTextW(dc, overlay.c_str(), -1, &overlay_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetTextColor(dc, RGB(255, 214, 120));
    RECT status_rect{18, client.bottom - 56, client.right - 18, client.bottom - 18};
    const std::wstring status = capture_status_.empty()
        ? L"Using fallback capture. Some games may show black if protected or running exclusive fullscreen."
        : capture_status_;
    DrawTextW(dc, status.c_str(), -1, &status_rect, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    EndPaint(hwnd_, &paint);
}

void PresentationWindow::PaintFallback(const wchar_t* message) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(hwnd_, &paint);

    RECT client{};
    GetClientRect(hwnd_, &client);

    HBRUSH background = CreateSolidBrush(RGB(8, 10, 14));
    FillRect(dc, &client, background);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(230, 235, 240));

    const std::wstring text = std::format(L"{}\nTarget: {}\nPress Esc to close.", message, target_.title);
    DrawTextW(dc, text.c_str(), -1, &client, DT_CENTER | DT_VCENTER | DT_WORDBREAK);

    EndPaint(hwnd_, &paint);
}

} // namespace outframe
