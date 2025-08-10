#include <windows.h>
#include <stdint.h>
#include "Engine.h"
#include "imgui.h"
#include "imgui_impl_win32.h"

LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;
    Engine* app = reinterpret_cast<Engine*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_SIZE:
        if (app) {
            if (wParam == SIZE_MINIMIZED) return 0;
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            if (w > 0 && h > 0) app->resize(w, h);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow) {
    const wchar_t* className = L"ggineWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);
    RECT rc{0, 0, 1280, 720};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, className, L"ggine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return -1;
    Engine app(1280, 720);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));
    if (!app.initialize(hwnd)) return -1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    LARGE_INTEGER prev; QueryPerformanceCounter(&prev);
    double accumulator = 0.0;
    const double dt = 1.0 / 60.0;
    MSG msg{};
    for (;;) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double frameDt = double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart);
        prev = now;
        if (frameDt > 0.25) frameDt = 0.25;
        accumulator += frameDt;
        while (accumulator >= dt) {
            app.update(dt);
            accumulator -= dt;
        }
        app.render();
    }
}
