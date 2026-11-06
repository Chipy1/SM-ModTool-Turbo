// Drop-in d3d11.dll proxy for the Mod Tool.
//
// The Mod Tool imports only D3D11CreateDevice and
// D3D11CreateDeviceAndSwapChain. Proton forces d3d11=native, so this file is
// loaded from the Mod Tool Release folder while the real implementation is
// loaded from the system directory. The two entry points are forwarded
// untouched; the ModTool Turbo hooks are installed by dllmain.cpp.
#include <windows.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <dxgi.h>

namespace {

HMODULE OriginalD3D11() {
    static HMODULE module = NULL;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        wchar_t system_dir[MAX_PATH] = {};
        UINT length = GetSystemDirectoryW(system_dir, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            wchar_t path[MAX_PATH] = {};
            wsprintfW(path, L"%ls\\d3d11.dll", system_dir);
            module = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }
    return module;
}

using CreateDeviceFn = HRESULT (WINAPI*)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
    UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

using CreateDeviceAndSwapChainFn = HRESULT (WINAPI*)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
    UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**,
    D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CreateDevice(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type, HMODULE software,
    UINT flags, const D3D_FEATURE_LEVEL* feature_levels, UINT feature_level_count,
    UINT sdk_version, ID3D11Device** device, D3D_FEATURE_LEVEL* feature_level,
    ID3D11DeviceContext** context) {
    const HMODULE module = OriginalD3D11();
    if (!module) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    const auto fn = reinterpret_cast<CreateDeviceFn>(
        GetProcAddress(module, "D3D11CreateDevice"));
    if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return fn(adapter, driver_type, software, flags, feature_levels,
              feature_level_count, sdk_version, device, feature_level, context);
}

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type, HMODULE software,
    UINT flags, const D3D_FEATURE_LEVEL* feature_levels, UINT feature_level_count,
    UINT sdk_version, const DXGI_SWAP_CHAIN_DESC* swap_chain_desc,
    IDXGISwapChain** swap_chain, ID3D11Device** device,
    D3D_FEATURE_LEVEL* feature_level, ID3D11DeviceContext** context) {
    const HMODULE module = OriginalD3D11();
    if (!module) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    const auto fn = reinterpret_cast<CreateDeviceAndSwapChainFn>(
        GetProcAddress(module, "D3D11CreateDeviceAndSwapChain"));
    if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return fn(adapter, driver_type, software, flags, feature_levels,
              feature_level_count, sdk_version, swap_chain_desc, swap_chain,
              device, feature_level, context);
}

// The current Mod Tool imports only the two device-creation entry points.
// These additional exports keep the proxy loadable if a future Mod Tool
// imports more of the standard D3D11 surface. D3D11On12CreateDevice is
// forwarded; the undocumented Core* entries fail cleanly instead of making
// the loader reject the proxy.
extern "C" __declspec(dllexport) HRESULT WINAPI D3D11On12CreateDevice(
    IUnknown* device, UINT flags, const D3D_FEATURE_LEVEL* feature_levels,
    UINT feature_level_count, IUnknown* const* queues, UINT queue_count,
    UINT node_mask, ID3D11Device** d3d11_device,
    ID3D11DeviceContext** d3d11_immediate_context,
    D3D_FEATURE_LEVEL* obtained_feature_level) {
    const HMODULE module = OriginalD3D11();
    if (!module) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    typedef HRESULT (WINAPI *Fn)(IUnknown*, UINT, const D3D_FEATURE_LEVEL*, UINT,
                                 IUnknown* const*, UINT, UINT, ID3D11Device**,
                                 ID3D11DeviceContext**, D3D_FEATURE_LEVEL*);
    const auto fn = reinterpret_cast<Fn>(GetProcAddress(module, "D3D11On12CreateDevice"));
    if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return fn(device, flags, feature_levels, feature_level_count, queues,
              queue_count, node_mask, d3d11_device, d3d11_immediate_context,
              obtained_feature_level);
}

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CoreCreateDevice() {
    return E_NOTIMPL;
}

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CoreCreateLayeredDevice() {
    return E_NOTIMPL;
}

extern "C" __declspec(dllexport) UINT WINAPI D3D11CoreGetLayeredDeviceSize() {
    return 0;
}

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CoreRegisterLayers() {
    return E_NOTIMPL;
}
