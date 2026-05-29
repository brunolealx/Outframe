#pragma once

#include "platform/WindowInfo.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace outframe {

class PresentationWindow {
public:
    PresentationWindow(HINSTANCE instance, WindowInfo target);
    ~PresentationWindow();

    bool Create();
    bool IsOpen() const;
    void Focus();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    bool InitializeDirect3D();
    bool InitializeCapture();
    void ResizeSwapChain();
    void Render();
    void PaintGdiPreview();
    void PaintFallback(const wchar_t* message);

    bool CreateDeviceResources();
    bool CreateSwapChainResources();
    bool CreateShaders();
    void ReleaseSwapChainResources();

    HINSTANCE instance_ = nullptr;
    WindowInfo target_;
    HWND hwnd_ = nullptr;
    bool capture_ready_ = false;
    bool gdi_fallback_ = false;
    bool render_ready_ = false;
    int width_ = 0;
    int height_ = 0;
    std::wstring capture_status_;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> current_frame_view_;

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem capture_item_{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession capture_session_{nullptr};
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice capture_device_{nullptr};
};

} // namespace outframe
