// Clean up Windows header includes.
#define NOMINMAX
#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <cassert>
#include <cwchar>
#include <Windows.h>

#include "window.h"
#include "utility.h"

// Perform static member initialization.
uint16_t Window::m_width  = 0;
uint16_t Window::m_height = 0;
HWND     Window::m_hwnd   = nullptr;

// Main message handler.
LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message,
                            const WPARAM wParam, const LPARAM lParam)
{
    switch (message)
    {
        case WM_KEYDOWN:
            if (VK_ESCAPE == wParam)
            {
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void Window::open(const uint16_t width, const uint16_t height)
{
    m_width  = width;
    m_height = height;

    // Set up the rectangle position and dimensions.
    RECT rect = { 0, 0, static_cast<long>(width), static_cast<long>(height) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // Get the handle to the instance of the application.
    const HINSTANCE appHandle = GetModuleHandle(nullptr);

    // Register the window class.
    WNDCLASS wndClass      = {};
    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = WindowProc;
    wndClass.hInstance     = appHandle;
    wndClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wndClass.lpszClassName = L"ReDXWindowClass";
    ASSERT(RegisterClass(&wndClass), "RegisterClass failed.");

    // Create a window and store its handle.
    m_hwnd = CreateWindow(wndClass.lpszClassName, L"ReDX",
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, // Disable resizing
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          rect.right - rect.left,
                          rect.bottom - rect.top,
                          nullptr, nullptr,                        // No parent window, no menus
                          appHandle, nullptr);

    ASSERT(m_hwnd, "CreateWindow failed.");

    // Make the window visible.
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
}

HWND Window::handle()
{
    assert(m_hwnd && "Uninitialized window handle.");
    return m_hwnd;
}

uint16_t Window::width()
{
    return m_width;
}

uint16_t Window::height()
{
    return m_height;
}

void Window::updateTitleBar(const float cpuFrameTime, const float gpuFrameTime)
{
    static wchar_t title[] = L"Magma > CPU: 00.00 ms | GPU: 00.00 ms";
    swprintf(title, _countof(title) + 1, L"Magma > CPU: %5.2f ms | GPU: %5.2f ms",
             std::min(cpuFrameTime, 99.99f), std::min(gpuFrameTime, 99.99f));
    SetWindowText(m_hwnd, title);
}