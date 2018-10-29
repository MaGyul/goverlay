#include "stable.h"
#include "dxgihook.h"
#include "overlay/hookapp.h"
#include "overlay/session.h"
#include "d3d11graphics.h"
#include "d3d10graphics.h"

HRESULT STDMETHODCALLTYPE H_DXGISwapChainPresent(IDXGISwapChain *swapChain, UINT a, UINT b)
{
    return session::dxgiHook()->Present_hook(swapChain, a, b);
}

HRESULT STDMETHODCALLTYPE H_DXGISwapChainResizeBuffers(IDXGISwapChain *swapChain, UINT a1, UINT a2, UINT a3, DXGI_FORMAT a4, UINT a5)
{
    return session::dxgiHook()->ResizeBuffers_hook(swapChain, a1, a2, a3, a4, a5);
}

HRESULT STDMETHODCALLTYPE H_DXGISwapChainResizeTarget(IDXGISwapChain *swapChain, const DXGI_MODE_DESC *a)
{
    return session::dxgiHook()->ResizeTarget_hook(swapChain, a);
}

HRESULT STDMETHODCALLTYPE H_DXGISwapChainPresent1(IDXGISwapChain1 *swapChain, UINT SyncInterval, UINT PresentFlags, _In_ const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
    return session::dxgiHook()->Present1_hook(swapChain, SyncInterval, PresentFlags, pPresentParameters);
}

DXGIHook::DXGIHook()
{
}

DXGIHook::~DXGIHook()
{
    unhook();
}

bool DXGIHook::hook()
{
    if (!loadLibInProc())
    {
        return false;
    }

    return tryHookDXGI();
}

void DXGIHook::unhook()
{
    if (dxgiSwapChainPresentHook_)
    {
        dxgiSwapChainPresentHook_.reset(nullptr);
    }
    if (dxgiSwapChainResizeBuffersHook_)
    {
        dxgiSwapChainResizeBuffersHook_.reset(nullptr);
    }
    if (dxgiSwapChainResizeTargetHook_)
    {
        dxgiSwapChainResizeTargetHook_.reset(nullptr);
    }
    if (dxgiSwapChainPresent1Hook_)
    {
        dxgiSwapChainPresent1Hook_.reset(nullptr);
    }
}

HRESULT STDMETHODCALLTYPE DXGIHook::Present_hook(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags)
{
    this->onBeforePresent(pSwapChain);

    HRESULT hr = this->dxgiSwapChainPresentHook_->callOrginal<HRESULT>(pSwapChain, SyncInterval, Flags);

    if (FAILED(hr))
    {
        LOGGER("n_overlay") << "DXGIHook Present_hook:" << hr;
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            Windows::ComPtr<ID3D11Device> device11;
            pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)device11.resetAndGetPointerAddress());
            hr = device11->GetDeviceRemovedReason();
            LOGGER("n_overlay") << "DXGIHook Present_hook GetDeviceRemovedReason:" << hr;
        }
    }


    this->onAfterPresent(pSwapChain);

    return hr;
}

HRESULT STDMETHODCALLTYPE DXGIHook::Present1_hook(IDXGISwapChain1 *pSwapChain, UINT SyncInterval, UINT PresentFlags, _In_ const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
    this->onBeforePresent(pSwapChain);

    HRESULT hr = this->dxgiSwapChainPresent1Hook_->callOrginal<HRESULT>(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);

    if (FAILED(hr))
    {
        LOGGER("n_overlay") << "DXGIHook Present_hook:" << hr;
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            Windows::ComPtr<ID3D11Device> device11;
            pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)device11.resetAndGetPointerAddress());
            hr = device11->GetDeviceRemovedReason();
            LOGGER("n_overlay") << "DXGIHook Present_hook GetDeviceRemovedReason:" << hr;
        }
    }


    this->onAfterPresent(pSwapChain);

    return hr;

}

HRESULT STDMETHODCALLTYPE DXGIHook::ResizeBuffers_hook(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    this->onResize(pSwapChain);

    return this->dxgiSwapChainResizeBuffersHook_->callOrginal<HRESULT>(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

HRESULT STDMETHODCALLTYPE DXGIHook::ResizeTarget_hook(IDXGISwapChain *pSwapChain, __in const DXGI_MODE_DESC *pNewTargetParameters)
{
    this->onResize(pSwapChain);

    return this->dxgiSwapChainResizeTargetHook_->callOrginal<HRESULT>(pSwapChain, pNewTargetParameters);
}

bool DXGIHook::loadLibInProc()
{
    if (dxgiLibraryLinked_)
    {
        return true;
    }

    dxgiModule_ = GetModuleHandleW(L"dxgi.dll");
    session::dxgiHookInfo().dxgiDll = dxgiModule_;

    dxgiLibraryLinked_ = linkDX11Library() || linkDX10Library();

    return dxgiLibraryLinked_ && !!dxgiModule_;
}

bool DXGIHook::linkDX10Library()
{
    if (d3d10Module_)
    {
        return true;
    }

    d3d10Module_ = GetModuleHandleW(L"d3d10.dll");
    if (d3d10Module_)
    {
        session::dxgiHookInfo().d3d10Dll = d3d10Module_;
        d3d10Create_ = (pFnD3D10CreateDeviceAndSwapChain)GetProcAddress(d3d10Module_, "D3D10CreateDeviceAndSwapChain");
    }

    return !!d3d10Module_;
}

bool DXGIHook::linkDX11Library()
{
    if (d3d11Module_)
    {
        return true;
    }

    d3d11Module_ = GetModuleHandleW(L"d3d11.dll");
    if (d3d11Module_)
    {
        session::dxgiHookInfo().d3d11Dll = d3d11Module_;
        d3d11Create_ = (pFnD3D11CreateDeviceAndSwapChain)GetProcAddress(d3d11Module_, "D3D11CreateDeviceAndSwapChain");
    }

    return !!d3d11Module_;
}

bool DXGIHook::tryHookDXGI()
{
    bool hooked = false;

    HWND window = ::CreateWindowA("STATIC", k_overlayIWindow, WS_POPUP, 0, 0, 2, 2, HWND_MESSAGE, NULL, NULL, NULL);
    if (window)
    {
        DXGI_SWAP_CHAIN_DESC desc = {};
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.Width = 2;
        desc.BufferDesc.Height = 2;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 1;
        desc.OutputWindow = window;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        if (d3d11Create_)
        {
            D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_11_0;
            UINT creationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;

            Windows::ComPtr<IDXGISwapChain> pSwapChain;
            Windows::ComPtr<ID3D11Device> pD3dDevice;
            Windows::ComPtr<ID3D11DeviceContext> pD3dContext;

            HRESULT hr = d3d11Create_(0, D3D_DRIVER_TYPE_HARDWARE, 0, creationFlags, NULL, 0, D3D11_SDK_VERSION, &desc,
                                      pSwapChain.resetAndGetPointerAddress(),
                                      pD3dDevice.resetAndGetPointerAddress(),
                                      &featureLevel_,
                                      pD3dContext.resetAndGetPointerAddress());

            if (SUCCEEDED(hr))
            {
                hooked = hookSwapChain(pSwapChain);
            }
        }
        else if (d3d10Create_)
        {
            Windows::ComPtr<IDXGISwapChain> pSwapChain;
            Windows::ComPtr<ID3D10Device> pD3dDevice;
            HRESULT hr = d3d10Create_(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &desc,
                                      pSwapChain.resetAndGetPointerAddress(), pD3dDevice.resetAndGetPointerAddress());

            if (SUCCEEDED(hr))
            {
                hooked = hookSwapChain(pSwapChain);
            }
        }

        DestroyWindow(window);
    }

    return hooked;
}

bool DXGIHook::hookSwapChain(Windows::ComPtr<IDXGISwapChain> pSwapChain)
{
    bool hooked = false;

    DWORD_PTR *swapChainPresentAddr = getVFunctionAddr((DWORD_PTR *)pSwapChain.get(), 8);
    DWORD_PTR *hookSwapChainPresentAddr = (DWORD_PTR *)H_DXGISwapChainPresent;

    DWORD_PTR *swapChainResizeBuffersAddr = getVFunctionAddr((DWORD_PTR *)pSwapChain.get(), 13);
    DWORD_PTR *hookSwapChainResizeBuffersAddr = (DWORD_PTR *)H_DXGISwapChainResizeBuffers;

    DWORD_PTR *swapChainResizeTargetAddr = getVFunctionAddr((DWORD_PTR *)pSwapChain.get(), 14);
    DWORD_PTR *hookSwapChainResizeTargetAddr = (DWORD_PTR *)H_DXGISwapChainResizeTarget;

    dxgiSwapChainPresentHook_.reset(new ApiHook<DXGISwapChainPresentType>(L"DXGISwapChainPresentType", swapChainPresentAddr, hookSwapChainPresentAddr));
    dxgiSwapChainPresentHook_->activeHook();

    dxgiSwapChainResizeBuffersHook_.reset(new ApiHook<DXGISwapChainResizeBuffersType>(L"DXGISwapChainResizeBuffers", swapChainResizeBuffersAddr, hookSwapChainResizeBuffersAddr));
    dxgiSwapChainResizeBuffersHook_->activeHook();

    dxgiSwapChainResizeTargetHook_.reset(new ApiHook<DXGISwapChainResizeTargetType>(L"DXGISwapChainPresentType", swapChainResizeTargetAddr, hookSwapChainResizeTargetAddr));
    dxgiSwapChainResizeTargetHook_->activeHook();

    LOGGER("n_overlay") << "Hook DXGISwapChainPresent:" << dxgiSwapChainPresentHook_->succeed();
    LOGGER("n_overlay") << "Hook DXGISwapChainResizeBuffers:" << dxgiSwapChainResizeBuffersHook_->succeed();
    LOGGER("n_overlay") << "Hook DXGISwapChainResizeTarget:" << dxgiSwapChainResizeTargetHook_->succeed();

    hooked = (dxgiSwapChainPresentHook_->succeed() && dxgiSwapChainResizeBuffersHook_->succeed() && dxgiSwapChainResizeTargetHook_->succeed());

    Windows::ComPtr<IDXGISwapChain1> pDXGISwapChain1;
    pSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void **)pDXGISwapChain1.resetAndGetPointerAddress());
    if (pDXGISwapChain1)
    {
        DWORD_PTR *swapChainPresent1Addr = getVFunctionAddr((DWORD_PTR *)pDXGISwapChain1.get(), 22);
        DWORD_PTR *hookSwapChainPresent1Addr = (DWORD_PTR *)H_DXGISwapChainPresent1;

        dxgiSwapChainPresent1Hook_.reset(new ApiHook<DXGISwapChainPresent1Type>(L"DXGISwapChainPresent1Type", swapChainPresent1Addr, hookSwapChainPresent1Addr));
        dxgiSwapChainPresent1Hook_->activeHook();

        hooked = dxgiSwapChainPresent1Hook_->succeed();
        session::dxgiHookInfo().present1Hooked = dxgiSwapChainPresent1Hook_->succeed();
    }

    session::dxgiHookInfo().presentHooked = dxgiSwapChainPresentHook_->succeed();
    session::dxgiHookInfo().resizeBufferHooked = dxgiSwapChainResizeBuffersHook_->succeed();
    session::dxgiHookInfo().resizeTargetHooked = dxgiSwapChainResizeTargetHook_->succeed();

    HookApp::instance()->overlayConnector()->sendGraphicsHookInfo(session::dxgiHookInfo());

    return hooked;
}

void DXGIHook::onBeforePresent(IDXGISwapChain *swap)
{
    if (graphicsInit_)
    {
        if (!session::overlayConnected() || !session::overlayEnabled())
        {
            freeGraphics();
        }
    }

    if (!graphicsInit_ && session::overlayEnabled())
    {
        initGraphics(swap);
    }

    if (graphicsInit_)
    {
        dxgiGraphics_->beforePresent(swap);
    }
}

void DXGIHook::onAfterPresent(IDXGISwapChain *swap)
{
    if (graphicsInit_)
    {
        dxgiGraphics_->afterPresent(swap);
    }
}

void DXGIHook::onResize(IDXGISwapChain *swapChain)
{
    uninitGraphics(swapChain);
}

bool DXGIHook::initGraphics(IDXGISwapChain *swap)
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    HRESULT hr = swap->GetDesc(&swapChainDesc);
    if (FAILED(hr))
    {
        return false;
    }

    HWND graphicsWindow = swapChainDesc.OutputWindow;

    if (graphicsWindow != session::graphicsWindow())
    {
        if (!session::injectWindow())
        {
            session::setGraphicsWindow(graphicsWindow);
            std::cout << __FUNCTION__ << ", setGraphicsWindow: " << graphicsWindow << std::endl;
        }
        else
        {
            return false;
        }
    }

    if (HookApp::instance()->uiapp()->window() != graphicsWindow)
    {
        HookApp::instance()->async([graphicsWindow]() {
            HookApp::instance()->uiapp()->trySetupGraphicsWindow(graphicsWindow);
        });

        return false;
    }

    session::setGraphicsThreadId(GetCurrentThreadId());

    Windows::ComPtr<ID3D10Device> device10 = NULL;
    Windows::ComPtr<ID3D11Device> device11 = NULL;
    hr = swap->GetDevice(
        __uuidof(ID3D10Device), (void **)device10.resetAndGetPointerAddress());
    hr = swap->GetDevice(
        __uuidof(ID3D11Device), (void **)device11.resetAndGetPointerAddress());

    if (!device10 && !device11)
    {
        return false;
    }

    bool isDX11 = (device11 && !device10);

    if (isDX11)
    {
        std::unique_ptr<D3d11Graphics> graphics = std::make_unique<D3d11Graphics>();
        if (graphics->initGraphics(swap))
        {
            this->dxgiGraphics_ = std::move(graphics);
            graphicsInit_ = true;
        }
    }
    else
    {
        std::unique_ptr<D3d10Graphics> graphics = std::make_unique<D3d10Graphics>();
        if (graphics->initGraphics(swap))
        {
            this->dxgiGraphics_ = std::move(graphics);
            graphicsInit_ = true;
        }
    }

    session::setGraphicsActive(graphicsInit_);

    if (graphicsInit_)
    {
        session::setIsWindowed(dxgiGraphics_->isWindowed());
    }
    return graphicsInit_;
}

void DXGIHook::uninitGraphics(IDXGISwapChain *swap)
{
    if (graphicsInit_)
    {
        dxgiGraphics_->uninitGraphics(swap);
        session::setGraphicsActive(false);
        graphicsInit_ = false;
    }
}

void DXGIHook::freeGraphics()
{
    if (graphicsInit_)
    {
        dxgiGraphics_->freeGraphics();
        session::setGraphicsActive(false);
        graphicsInit_ = false;
    }
}
