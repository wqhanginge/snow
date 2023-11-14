#pragma once

#include "stdafx.h"
#include "snows.h"


struct ComException {
    HRESULT hr;
    ComException(HRESULT val) :hr(val) {}
};

class HandleWrapperIcon {
    HICON _hico;
public:
    HandleWrapperIcon(HICON hico) :_hico(hico) {}
    ~HandleWrapperIcon() noexcept { if (_hico) DestroyIcon(_hico); }
    HICON& operator*() { return _hico; }
    constexpr operator HICON() const { return _hico; }
};

class SnowRenderer {
public:
    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    static constexpr UINT BMPSIZE = 128;

    static inline void HR(const HRESULT hr) {
        if (FAILED(hr)) throw ComException(hr);
    }

private:
    UINT _width;
    UINT _height;
    HWND _hwnd;
    ComPtr<IWICBitmapSource> _snow_bmp;
    ComPtr<ID3D11Device> _d3d_device;
    ComPtr<ID2D1DeviceContext6> _d2d_dc;
    ComPtr<ID2D1Effect> _d2d_dc_bmp_affine;
    ComPtr<ID2D1Effect> _d2d_dc_bmp_opacity;
    ComPtr<ID2D1Image> _d2d_dc_bmp;
    ComPtr<IDXGISwapChain1> _dxgi_swap_chain;
    ComPtr<IDCompositionTarget> _dcomp_target;

    void _loadSnowBitmap();
    void _createDevice();
    void _createDeviceContext();
    void _createSwapChain(UINT width, UINT height);
    void _createRenderTarget();
    void _createDCompTarget(HWND hwnd);
    void _createRenderer(bool post_swap_chain = false);
    void _releaseRenderer(bool post_swap_chain = false);

public:
    SnowRenderer() :_width(0), _height(0), _hwnd(nullptr) {}
    ~SnowRenderer() { _releaseRenderer(); _snow_bmp.Reset(); }
    void initialize(UINT width, UINT height, HWND hwnd);
    void refreash();
    HRESULT presentTest();
    HRESULT render(const std::vector<Snow>& snows, float alpha = 1.0f);
    void resize(UINT width, UINT height);
    void setWindow(HWND hwnd);
};
