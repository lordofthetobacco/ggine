#include "Engine.h"
#include <fstream>
#include <filesystem>
#include <assert.h>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "ObjLoader.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static ComPtr<IDXGIAdapter1> SelectHardwareAdapter(ComPtr<IDXGIFactory6> factory) {
    ComPtr<IDXGIAdapter1> bestAdapter; SIZE_T bestVideoMemory = 0; ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) { DXGI_ADAPTER_DESC1 d{}; adapter->GetDesc1(&d); if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue; if (d.DedicatedVideoMemory > bestVideoMemory) { bestVideoMemory = d.DedicatedVideoMemory; bestAdapter = adapter; } }
    return bestAdapter;
}

Engine::Engine(UINT width, UINT height) : clientWidth(width), clientHeight(height), hwnd(nullptr), rtvDescriptorSize(0), currentFrameIndex(0) {
    viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f}; scissorRect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
}

std::vector<uint8_t> Engine::readFileBytes(const std::wstring& path) { std::ifstream f(path, std::ios::binary); if (!f) return {}; f.seekg(0, std::ios::end); size_t size = static_cast<size_t>(f.tellg()); f.seekg(0, std::ios::beg); std::vector<uint8_t> data(size); f.read(reinterpret_cast<char*>(data.data()), size); return data; }

bool Engine::uploadVerticesToBuffer(const std::vector<XMFLOAT3>& vertices, ComPtr<ID3D12Resource>& outVB, D3D12_VERTEX_BUFFER_VIEW& outVBV) {
    D3D12_HEAP_PROPERTIES heapProps{}; heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; D3D12_RESOURCE_DESC resDesc{}; resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; resDesc.Width = UINT(vertices.size() * sizeof(XMFLOAT3)); resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1; resDesc.SampleDesc.Count = 1; resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; outVB.Reset(); if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&outVB)))) return false; void* mapped = nullptr; D3D12_RANGE readRange{0,0}; outVB->Map(0, &readRange, &mapped); memcpy(mapped, vertices.data(), vertices.size() * sizeof(XMFLOAT3)); outVB->Unmap(0, nullptr); outVBV.BufferLocation = outVB->GetGPUVirtualAddress(); outVBV.StrideInBytes = sizeof(XMFLOAT3); outVBV.SizeInBytes = UINT(vertices.size() * sizeof(XMFLOAT3)); return true;
}

bool Engine::createCubeObject(const std::wstring& name) {
    const float s = 0.5f; const XMFLOAT3 v[] = { {-s,-s,-s},{-s, s,-s},{ s, s,-s}, { s, s,-s},{ s,-s,-s},{-s,-s,-s}, {-s,-s, s},{ s,-s, s},{ s, s, s}, { s, s, s},{-s, s, s},{-s,-s, s}, {-s, s,-s},{-s, s, s},{-s,-s, s}, {-s,-s, s},{-s,-s,-s},{-s, s,-s}, { s, s,-s},{ s,-s,-s},{ s,-s, s}, { s,-s, s},{ s, s, s},{ s, s,-s}, {-s,-s,-s},{ s,-s,-s},{ s,-s, s}, { s,-s, s},{-s,-s, s},{-s,-s,-s}, {-s, s,-s},{-s, s, s},{ s, s, s}, { s, s, s},{ s, s,-s},{-s, s,-s}, };
    std::vector<XMFLOAT3> verts(std::begin(v), std::end(v)); MeshObject m; m.name = name; m.vertexCount = UINT(verts.size()); if (!uploadVerticesToBuffer(verts, m.vertexBuffer, m.vbv)) return false; scene.meshes.push_back(std::move(m)); if (scene.selectedMesh < 0) scene.selectedMesh = 0; return true;
}

bool Engine::createObjObject(const std::wstring& path, const std::wstring& name) {
    ObjMeshData mesh; if (!LoadObjPositionsAndIndices(path, mesh)) return false; if (mesh.indices.empty() || mesh.positions.empty()) return false; std::vector<XMFLOAT3> vertices; vertices.reserve(mesh.indices.size()); for (uint32_t idx : mesh.indices) vertices.push_back(mesh.positions[idx]); MeshObject m; m.name = name; m.vertexCount = UINT(vertices.size()); if (!uploadVerticesToBuffer(vertices, m.vertexBuffer, m.vbv)) return false; scene.meshes.push_back(std::move(m)); if (scene.selectedMesh < 0) scene.selectedMesh = 0; return true;
}

void Engine::createLightObject(const std::wstring& name) { LightObject l; l.name = name; l.color = {1,1,1}; l.intensity = 1.0f; l.transform.position = {0,1,0}; scene.lights.push_back(std::move(l)); if (scene.selectedLight < 0) scene.selectedLight = 0; }

bool Engine::initialize(HWND windowHandle) {
    hwnd = windowHandle;
#if defined(_DEBUG)
    { ComPtr<ID3D12Debug> dbg; if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer(); }
#endif
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory)))) return false; BOOL allowTearing = FALSE; if (SUCCEEDED(dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) tearingSupported = allowTearing == TRUE; ComPtr<IDXGIAdapter1> ad = SelectHardwareAdapter(dxgiFactory); if (ad) { if (FAILED(D3D12CreateDevice(ad.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return false; } else { ComPtr<IDXGIAdapter> warp; if (FAILED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))) return false; if (FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return false; }
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; if (FAILED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&commandQueue)))) return false; createSwapChain(); rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); D3D12_DESCRIPTOR_HEAP_DESC rtv{}; rtv.NumDescriptors = kFrameCount; rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; if (FAILED(device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&rtvDescriptorHeap)))) return false; D3D12_DESCRIPTOR_HEAP_DESC dsv{}; dsv.NumDescriptors = 1; dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; if (FAILED(device->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&dsvDescriptorHeap)))) return false; createRenderTargets(); createDepthResources(); for (UINT i=0;i<kFrameCount;++i) { if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i])))) return false; }
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[currentFrameIndex].Get(), nullptr, IID_PPV_ARGS(&commandList)))) return false; commandList->Close(); if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false; fenceValues[currentFrameIndex]=1; fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); if (!fenceEvent) return false; if (!createPipeline()) return false; if (!createCubeObject(L"Cube")) return false; createLightObject(L"Light"); for (UINT i=0;i<kFrameCount;++i) { D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD; D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = 256; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cameraCBs[i])))) return false; D3D12_RANGE rr{0,0}; cameraCBs[i]->Map(0, &rr, reinterpret_cast<void**>(&cameraCBMapped[i])); }
    if (!initImGui()) return false; frameTimeMs.assign(240,0.0f); QueryPerformanceFrequency(&perfFreq); QueryPerformanceCounter(&lastCounter); timingInitialized = true; scene.selectedMesh = scene.meshes.empty() ? -1 : 0; initAssetsDir(); scanAssets(); return true;
}

bool Engine::createPipeline() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{}; heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heapDesc.NumDescriptors = 1; heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&imguiSrvHeap)))) return false; D3D12_FEATURE_DATA_ROOT_SIGNATURE feat{}; feat.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1; if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feat, sizeof(feat)))) { feat.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0; }
    D3D12_ROOT_PARAMETER1 param{}; param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; param.Descriptor.ShaderRegister = 0; param.Descriptor.RegisterSpace = 0; D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs{}; rs.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; D3D12_ROOT_SIGNATURE_DESC1 rs1{}; rs1.NumParameters = 1; rs1.pParameters = &param; rs1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; rs.Desc_1_1 = rs1; ComPtr<ID3DBlob> s; ComPtr<ID3DBlob> e; if (FAILED(D3D12SerializeVersionedRootSignature(&rs, &s, &e))) return false; if (FAILED(device->CreateRootSignature(0, s->GetBufferPointer(), s->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) return false; std::filesystem::path exeDir; wchar_t modulePath[MAX_PATH]; GetModuleFileNameW(nullptr, modulePath, MAX_PATH); exeDir = std::filesystem::path(modulePath).parent_path(); auto vsBytes = readFileBytes((exeDir / L"shaders/triangle_vs.cso").wstring()); auto psBytes = readFileBytes((exeDir / L"shaders/triangle_ps.cso").wstring()); if (vsBytes.empty() || psBytes.empty()) return false; D3D12_SHADER_BYTECODE vs{}; vs.pShaderBytecode = vsBytes.data(); vs.BytecodeLength = vsBytes.size(); D3D12_SHADER_BYTECODE ps{}; ps.pShaderBytecode = psBytes.data(); ps.BytecodeLength = psBytes.size(); D3D12_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, }; D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{}; pso.pRootSignature = rootSignature.Get(); pso.VS = vs; pso.PS = ps; D3D12_BLEND_DESC blend{}; D3D12_RENDER_TARGET_BLEND_DESC rt{}; rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; blend.RenderTarget[0] = rt; pso.BlendState = blend; pso.SampleMask = UINT_MAX; D3D12_RASTERIZER_DESC rast{}; rast.FillMode = D3D12_FILL_MODE_SOLID; rast.CullMode = D3D12_CULL_MODE_NONE; rast.DepthClipEnable = TRUE; pso.RasterizerState = rast; D3D12_DEPTH_STENCIL_DESC ds{}; ds.DepthEnable = TRUE; ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; pso.DepthStencilState = ds; pso.InputLayout = { layout, _countof(layout) }; pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; pso.NumRenderTargets = 1; pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; pso.DSVFormat = DXGI_FORMAT_D32_FLOAT; pso.SampleDesc.Count = 1; if (FAILED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState)))) return false; return true;
}

bool Engine::initImGui() { IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; if (!ImGui_ImplWin32_Init(hwnd)) return false; if (!ImGui_ImplDX12_Init(device.Get(), kFrameCount, DXGI_FORMAT_R8G8B8A8_UNORM, imguiSrvHeap.Get(), imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(), imguiSrvHeap->GetGPUDescriptorHandleForHeapStart())) return false; return true; }

void Engine::createSwapChain() { DXGI_SWAP_CHAIN_DESC1 d{}; d.BufferCount = kFrameCount; d.Width = clientWidth; d.Height = clientHeight; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; d.SampleDesc.Count = 1; d.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; ComPtr<IDXGISwapChain1> t; dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &d, nullptr, nullptr, &t); t.As(&swapChain); swapChain->SetMaximumFrameLatency(kFrameCount); frameLatencyWaitableObject = swapChain->GetFrameLatencyWaitableObject(); dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER); currentFrameIndex = swapChain->GetCurrentBackBufferIndex(); }

void Engine::createRenderTargets() { D3D12_CPU_DESCRIPTOR_HANDLE h = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(); for (UINT i=0;i<kFrameCount;++i) { swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])); device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, h); h.ptr += rtvDescriptorSize; } }

void Engine::createDepthResources() { D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; d.Width = clientWidth; d.Height = clientHeight; d.DepthOrArraySize = 1; d.MipLevels = 1; d.Format = DXGI_FORMAT_D32_FLOAT; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; d.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT; D3D12_CLEAR_VALUE cv{}; cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 1.0f; if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&depthStencil)))) return; D3D12_DEPTH_STENCIL_VIEW_DESC v{}; v.Format = DXGI_FORMAT_D32_FLOAT; v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; device->CreateDepthStencilView(depthStencil.Get(), &v, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()); }

void Engine::update(double dt) {
    ImGuiIO& io = ImGui::GetIO(); bool usingMouse = (GetKeyState(VK_RBUTTON) & 0x8000) && !io.WantCaptureMouse; POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p); static bool first = true; if (first) { lastMouse = p; first = false; } float dx = float(p.x - lastMouse.x); float dy = float(p.y - lastMouse.y); lastMouse = p; if (usingMouse) { const float sensitivity = 0.005f; cameraYaw += dx * sensitivity; cameraPitch -= dy * sensitivity; const float limit = XM_PIDIV2 - 0.01f; if (cameraPitch > limit) cameraPitch = limit; if (cameraPitch < -limit) cameraPitch = -limit; } XMVECTOR forward = XMVectorSet(cosf(cameraPitch) * sinf(cameraYaw), sinf(cameraPitch), cosf(cameraPitch) * cosf(cameraYaw), 0.0f); XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0), forward)); XMVECTOR up = XMVectorSet(0,1,0,0); float move = 3.0f * static_cast<float>(dt); if (!io.WantCaptureKeyboard) { if (GetAsyncKeyState('W') & 0x8000) { XMVECTOR p0 = XMLoadFloat3(&cameraPosition); XMStoreFloat3(&cameraPosition, XMVectorAdd(p0, XMVectorScale(forward, move))); } if (GetAsyncKeyState('S') & 0x8000) { XMVECTOR p0 = XMLoadFloat3(&cameraPosition); XMStoreFloat3(&cameraPosition, XMVectorSubtract(p0, XMVectorScale(forward, move))); } if (GetAsyncKeyState('A') & 0x8000) { XMVECTOR p0 = XMLoadFloat3(&cameraPosition); XMStoreFloat3(&cameraPosition, XMVectorSubtract(p0, XMVectorScale(right, move))); } if (GetAsyncKeyState('D') & 0x8000) { XMVECTOR p0 = XMLoadFloat3(&cameraPosition); XMStoreFloat3(&cameraPosition, XMVectorAdd(p0, XMVectorScale(right, move))); } if (GetAsyncKeyState('Q') & 0x8000) { XMVECTOR p0 = XMLoadFloat3(&cameraPosition); XMStoreFloat3(&cameraPosition, XMVectorSubtract(p0, XMVectorScale(up, move))); } if (GetAsyncKeyState('E') & 0x8000) { XMVECTOR p0 = XMLoadFloat3(&cameraPosition); XMStoreFloat3(&cameraPosition, XMVectorAdd(p0, XMVectorScale(up, move))); } }
    assetsRescanAccum += dt; if (assetsRescanAccum > 1.0) { assetsRescanAccum = 0.0; scanAssets(); }
}

void Engine::setWindowClientSize(UINT width, UINT height) { RECT wr{0,0,(LONG)width,(LONG)height}; AdjustWindowRectEx(&wr, GetWindowLong(hwnd, GWL_STYLE), FALSE, GetWindowLong(hwnd, GWL_EXSTYLE)); SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); }

void Engine::setFullscreen(bool enable) { if (isFullscreen == enable) return; isFullscreen = enable; if (enable) { windowStyle = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE); GetWindowRect(hwnd, &windowRect); SetWindowLongPtr(hwnd, GWL_STYLE, windowStyle & ~WS_OVERLAPPEDWINDOW); HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST); MONITORINFO mi{sizeof(mi)}; GetMonitorInfo(hMon, &mi); SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED); } else { SetWindowLongPtr(hwnd, GWL_STYLE, windowStyle); SetWindowPos(hwnd, nullptr, windowRect.left, windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED); } }

void Engine::render() {
    LARGE_INTEGER now; if (timingInitialized) { QueryPerformanceCounter(&now); double ms = double(now.QuadPart - lastCounter.QuadPart) * 1000.0 / double(perfFreq.QuadPart); lastCounter = now; if (!frameTimeMs.empty()) { frameTimeMs[frameTimeWriteIdx] = static_cast<float>(ms); frameTimeWriteIdx = (frameTimeWriteIdx + 1) % frameTimeMs.size(); }}
    if (frameLatencyWaitableObject) WaitForSingleObject(frameLatencyWaitableObject, 1000);
    commandAllocators[currentFrameIndex]->Reset();
    if (selectionKind==SelectionKind::Mesh && (selectedIndex < 0 || selectedIndex >= (int)scene.meshes.size())) { selectionKind = SelectionKind::None; selectedIndex = -1; }
    if (selectionKind==SelectionKind::Light && (selectedIndex < 0 || selectedIndex >= (int)scene.lights.size())) { selectionKind = SelectionKind::None; selectedIndex = -1; }
    commandList->Reset(commandAllocators[currentFrameIndex].Get(), nullptr);
    ID3D12DescriptorHeap* heaps[] = { imguiSrvHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Tools");
    if (tearingSupported) ImGui::Checkbox("Enable Tearing", &enableTearing);
    ImGui::Checkbox("VSync", &enableVsync);
    if (!frameTimeMs.empty()) ImGui::PlotLines("Frame ms", frameTimeMs.data(), static_cast<int>(frameTimeMs.size()), static_cast<int>(frameTimeWriteIdx), nullptr, 0.0f, 40.0f, ImVec2(0, 80));
    ImGui::Text("GPU Waitable: %s", frameLatencyWaitableObject ? "on" : "off");
    ImGui::End();

    ImGui::Begin("Hierarchy");
    if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | (selectionKind==SelectionKind::Camera?ImGuiTreeNodeFlags_Selected:0))) {
        if (ImGui::IsItemClicked()) { selectionKind = SelectionKind::Camera; selectedIndex = 0; }
        if (ImGui::BeginPopupContextItem("CamCtx")) {
            static char camRename[128] = {};
            if (ImGui::IsWindowAppearing()) {
                std::string utf8(cameraName.begin(), cameraName.end());
                strncpy_s(camRename, utf8.c_str(), sizeof(camRename)-1);
            }
            ImGui::InputText("Rename", camRename, sizeof(camRename));
            if (ImGui::MenuItem("Apply Rename")) {
                std::string s(camRename); std::wstring ws(s.begin(), s.end()); cameraName = ws; ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Add New Cube")) { createCubeObject(L"Cube"); }
            if (ImGui::MenuItem("Add New Light")) { createLightObject(L"Light"); }
            ImGui::EndPopup();
        }
    }
    if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen)) {
        int meshToDelete = -1;
        int meshToDuplicate = -1;
        for (int i=0;i<(int)scene.meshes.size();++i) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ((selectionKind==SelectionKind::Mesh && selectedIndex==i)?ImGuiTreeNodeFlags_Selected:0);
            std::string utf8(scene.meshes[i].name.begin(), scene.meshes[i].name.end());
            ImGui::TreeNodeEx((void*)(intptr_t)(1000+i), flags, "%s", utf8.c_str());
            if (ImGui::IsItemClicked()) { selectionKind = SelectionKind::Mesh; selectedIndex = i; }
            if (ImGui::BeginPopupContextItem((std::string("MeshCtx")+std::to_string(i)).c_str())) {
                if (ImGui::MenuItem("Duplicate")) { meshToDuplicate = i; ImGui::CloseCurrentPopup(); }
                if (ImGui::MenuItem("Delete")) { meshToDelete = i; ImGui::CloseCurrentPopup(); }
                static char renameBuf[128] = {};
                if (ImGui::IsWindowAppearing()) {
                    std::string n(scene.meshes[i].name.begin(), scene.meshes[i].name.end());
                    strncpy_s(renameBuf, n.c_str(), sizeof(renameBuf)-1);
                }
                ImGui::InputText("Rename", renameBuf, sizeof(renameBuf));
                if (ImGui::MenuItem("Apply Rename##mesh")) {
                    std::string s(renameBuf); std::wstring ws(s.begin(), s.end()); scene.meshes[i].name = ws; ImGui::CloseCurrentPopup();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Add New Cube")) { createCubeObject(L"Cube"); }
                if (ImGui::MenuItem("Add New Light")) { createLightObject(L"Light"); }
                ImGui::EndPopup();
            }
        }
        if (meshToDuplicate >= 0 && meshToDuplicate < (int)scene.meshes.size()) {
            MeshObject copy = scene.meshes[meshToDuplicate];
            copy.name += L" (copy)";
            scene.meshes.push_back(copy);
            selectionKind = SelectionKind::Mesh; selectedIndex = (int)scene.meshes.size()-1;
        }
        if (meshToDelete >= 0 && meshToDelete < (int)scene.meshes.size()) {
            scene.meshes.erase(scene.meshes.begin()+meshToDelete);
            selectionKind = SelectionKind::None; selectedIndex = -1;
        }
    }
    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        int lightToDelete = -1;
        int lightToDuplicate = -1;
        for (int i=0;i<(int)scene.lights.size();++i) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ((selectionKind==SelectionKind::Light && selectedIndex==i)?ImGuiTreeNodeFlags_Selected:0);
            std::string utf8(scene.lights[i].name.begin(), scene.lights[i].name.end());
            ImGui::TreeNodeEx((void*)(intptr_t)(2000+i), flags, "%s", utf8.c_str());
            if (ImGui::IsItemClicked()) { selectionKind = SelectionKind::Light; selectedIndex = i; }
            if (ImGui::BeginPopupContextItem((std::string("LightCtx")+std::to_string(i)).c_str())) {
                if (ImGui::MenuItem("Duplicate")) { lightToDuplicate = i; ImGui::CloseCurrentPopup(); }
                if (ImGui::MenuItem("Delete")) { lightToDelete = i; ImGui::CloseCurrentPopup(); }
                static char lrename[128] = {};
                if (ImGui::IsWindowAppearing()) {
                    std::string n(scene.lights[i].name.begin(), scene.lights[i].name.end());
                    strncpy_s(lrename, n.c_str(), sizeof(lrename)-1);
                }
                ImGui::InputText("Rename", lrename, sizeof(lrename));
                if (ImGui::MenuItem("Apply Rename##light")) {
                    std::string s(lrename); std::wstring ws(s.begin(), s.end()); scene.lights[i].name = ws; ImGui::CloseCurrentPopup();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Add New Cube")) { createCubeObject(L"Cube"); }
                if (ImGui::MenuItem("Add New Light")) { createLightObject(L"Light"); }
                ImGui::EndPopup();
            }
        }
        if (lightToDuplicate >= 0 && lightToDuplicate < (int)scene.lights.size()) {
            LightObject copy = scene.lights[lightToDuplicate];
            copy.name += L" (copy)";
            scene.lights.push_back(copy);
            selectionKind = SelectionKind::Light; selectedIndex = (int)scene.lights.size()-1;
        }
        if (lightToDelete >= 0 && lightToDelete < (int)scene.lights.size()) {
            scene.lights.erase(scene.lights.begin()+lightToDelete);
            selectionKind = SelectionKind::None; selectedIndex = -1;
        }
    }
    ImGui::End();

    ImGui::Begin("Inspector");
    if (selectionKind==SelectionKind::Camera) {
        float pos[3] = { cameraPosition.x, cameraPosition.y, cameraPosition.z };
        if (ImGui::DragFloat3("Position", pos, 0.01f)) {
            cameraPosition = {pos[0],pos[1],pos[2]};
        }
        float yawPitch[2] = { cameraYaw, cameraPitch };
        if (ImGui::DragFloat2("Yaw/Pitch", yawPitch, 0.01f)) {
            cameraYaw = yawPitch[0];
            cameraPitch = yawPitch[1];
        }
    } else if (selectionKind==SelectionKind::Mesh && selectedIndex>=0 && selectedIndex<(int)scene.meshes.size()) {
        auto& t = scene.meshes[selectedIndex].transform;
        float pos[3] = { t.position.x, t.position.y, t.position.z };
        float rot[3] = { t.rotationEuler.x, t.rotationEuler.y, t.rotationEuler.z };
        float scl[3] = { t.scale.x, t.scale.y, t.scale.z };
        if (ImGui::DragFloat3("Position", pos, 0.01f)) t.position = {pos[0],pos[1],pos[2]};
        if (ImGui::DragFloat3("Rotation", rot, 0.5f)) t.rotationEuler = {rot[0],rot[1],rot[2]};
        if (ImGui::DragFloat3("Scale", scl, 0.01f)) t.scale = {scl[0],scl[1],scl[2]};
    } else if (selectionKind==SelectionKind::Light && selectedIndex>=0 && selectedIndex<(int)scene.lights.size()) {
        auto& l = scene.lights[selectedIndex];
        float pos[3] = { l.transform.position.x, l.transform.position.y, l.transform.position.z };
        float col[3] = { l.color.x, l.color.y, l.color.z };
        if (ImGui::DragFloat3("Position", pos, 0.01f)) l.transform.position = {pos[0],pos[1],pos[2]};
        if (ImGui::ColorEdit3("Color", col)) l.color = {col[0],col[1],col[2]};
        ImGui::DragFloat("Intensity", &l.intensity, 0.05f, 0.0f, 32.0f);
    }
    ImGui::End();

    ImGui::Begin("Assets");
    ImGui::Text("Folder: %ls", assetsDirW.c_str());
    static int selectedAsset = -1;
    for (int i=0;i<(int)assetObjFiles.size();++i) {
        std::string name(assetObjFiles[i].begin(), assetObjFiles[i].end());
        bool sel = (selectedAsset==i);
        if (ImGui::Selectable(name.c_str(), sel)) selectedAsset = i;
    }
    if (selectedAsset>=0 && selectedAsset<(int)assetObjFiles.size()) {
        if (ImGui::Button("Add Selected OBJ")) {
            createObjObject(assetObjFiles[selectedAsset], L"OBJ");
        }
    }
    ImGui::End();

    ImGui::Render();

    D3D12_RESOURCE_BARRIER toRT{};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = renderTargets[currentFrameIndex].Get();
    toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &toRT);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += currentFrameIndex * rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    float clear[4] = {0.05f, 0.05f, 0.07f, 1.0f};
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearRenderTargetView(rtv, clear, 0, nullptr);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    XMVECTOR eye = XMLoadFloat3(&cameraPosition);
    XMVECTOR forward = XMVectorSet(cosf(cameraPitch) * sinf(cameraYaw), sinf(cameraPitch), cosf(cameraPitch) * cosf(cameraYaw), 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eye, XMVectorAdd(eye, forward), XMVectorSet(0,1,0,0));
    float aspect = clientWidth > 0 ? float(clientWidth) / float(clientHeight ? clientHeight : 1) : 1.0f;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.9f, aspect, 0.1f, 100.0f);

    for (size_t i=0; i<scene.meshes.size(); ++i) {
        const auto& obj = scene.meshes[i];
        if (!obj.vertexBuffer) continue;
        XMMATRIX S = XMMatrixScaling(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(obj.transform.rotationEuler.x), XMConvertToRadians(obj.transform.rotationEuler.y), XMConvertToRadians(obj.transform.rotationEuler.z));
        XMMATRIX T = XMMatrixTranslation(obj.transform.position.x, obj.transform.position.y, obj.transform.position.z);
        XMMATRIX mvp = S * R * T * view * proj;
        CameraCB cb{}; XMStoreFloat4x4(&cb.mvp, mvp);
        memcpy(cameraCBMapped[currentFrameIndex], &cb, sizeof(CameraCB));
        commandList->IASetVertexBuffers(0, 1, &obj.vbv);
        commandList->SetGraphicsRootConstantBufferView(0, cameraCBs[currentFrameIndex]->GetGPUVirtualAddress());
        commandList->DrawInstanced(obj.vertexCount, 1, 0, 0);
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

    D3D12_RESOURCE_BARRIER toPresent{};
    toPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toPresent.Transition.pResource = renderTargets[currentFrameIndex].Get();
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &toPresent);
    commandList->Close();
    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, lists);
    UINT presentFlags = 0; if (tearingSupported && enableTearing && !enableVsync) presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
    UINT syncInterval = enableVsync ? 1 : 0;
    swapChain->Present(syncInterval, presentFlags);
    moveToNextFrame();
}

void Engine::moveToNextFrame() { const UINT64 fv = fenceValues[currentFrameIndex]; commandQueue->Signal(fence.Get(), fv); currentFrameIndex = swapChain->GetCurrentBackBufferIndex(); if (fence->GetCompletedValue() < fenceValues[currentFrameIndex]) { fence->SetEventOnCompletion(fenceValues[currentFrameIndex], fenceEvent); WaitForSingleObject(fenceEvent, INFINITE);} fenceValues[currentFrameIndex] = fv + 1; }

void Engine::waitForGpu() { commandQueue->Signal(fence.Get(), fenceValues[currentFrameIndex]); fence->SetEventOnCompletion(fenceValues[currentFrameIndex], fenceEvent); WaitForSingleObject(fenceEvent, INFINITE); fenceValues[currentFrameIndex]++; }

void Engine::resize(UINT width, UINT height) { if (!swapChain) { clientWidth = width; clientHeight = height; return; } if (width==0 || height==0) return; waitForGpu(); for (UINT i=0;i<kFrameCount;++i) renderTargets[i].Reset(); depthStencil.Reset(); DXGI_SWAP_CHAIN_DESC desc{}; swapChain->GetDesc(&desc); if (FAILED(swapChain->ResizeBuffers(kFrameCount, width, height, desc.BufferDesc.Format, desc.Flags))) return; swapChain->SetMaximumFrameLatency(kFrameCount); frameLatencyWaitableObject = swapChain->GetFrameLatencyWaitableObject(); currentFrameIndex = swapChain->GetCurrentBackBufferIndex(); createRenderTargets(); clientWidth=width; clientHeight=height; viewport.Width = (float)width; viewport.Height = (float)height; scissorRect.right = (LONG)width; scissorRect.bottom = (LONG)height; createDepthResources(); }

Engine::~Engine() { if (commandQueue && fence && fenceEvent) waitForGpu(); ImGui_ImplDX12_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); if (fenceEvent) CloseHandle(fenceEvent); }

void Engine::initAssetsDir() {
    std::filesystem::path base = std::filesystem::path(L".");
    std::filesystem::path assets = base / L"assets";
    if (!std::filesystem::exists(assets)) {
        std::error_code ec; std::filesystem::create_directories(assets, ec);
    }
    assetsDirW = assets.wstring();
}

void Engine::scanAssets() {
    assetObjFiles.clear();
    if (assetsDirW.empty()) return;
    std::filesystem::path root(assetsDirW);
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.has_extension()) {
            auto ext = p.extension().wstring();
            for (auto& c : ext) c = (wchar_t)towlower(c);
            if (ext == L".obj") {
                assetObjFiles.push_back(p.wstring());
            }
        }
    }
}
