#include "utility.h"
#include "window.h"

#include <algorithm>
#include <cassert>
#include <cwchar>
#include <Windows.h>

// Main message handler.
static LRESULT CALLBACK WindowProc(const HWND hWnd, const UINT message,
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

Window::Window(const uint16_t width, const uint16_t height)
{
    m_width = width;
    m_height = height;

    // Set up the rectangle position and dimensions.
    RECT rect = { 0, 0, static_cast<long>(width), static_cast<long>(height) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // Get the handle of the application.
    m_hinst = GetModuleHandle(nullptr);

    // Register the window class.
    WNDCLASS wndClass      = {};
    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = WindowProc;
    wndClass.hInstance     = m_hinst;
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
                          m_hinst, nullptr);

    ASSERT(m_hwnd, "CreateWindow failed.");
}

Window::Window(Window&& other) noexcept
{
    TrivialMoveConstruct<Window>(this, &other);
}

Window& Window::operator=(Window&& other) noexcept
{
    return TrivialMoveAssign<Window>(this, &other);
}

Window::~Window()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
    }
}

void Window::Show() const
{
    assert(m_hwnd && "Uninitialized window handle.");
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
}

void Window::Hide() const
{
    assert(m_hwnd && "Uninitialized window handle.");
    ShowWindow(m_hwnd, SW_HIDE);
}

HINSTANCE Window::Instance() const
{
    assert(m_hinst && "Uninitialized application handle.");
    return m_hinst;
}

HWND Window::Handle() const
{
    assert(m_hwnd && "Uninitialized window handle.");
    return m_hwnd;
}

uint16_t Window::Width() const
{
    return m_width;
}

uint16_t Window::Height() const
{
    return m_height;
}

void Window::UpdateTitle(const float cpuFrameTime, const float gpuFrameTime) const
{
    static wchar_t title[] = L"Magma > CPU: 00.00 ms | GPU: 00.00 ms";
    swprintf(title, _countof(title) + 1, L"Magma > CPU: %5.2f ms | GPU: %5.2f ms",
             std::min(cpuFrameTime, 99.99f), std::min(gpuFrameTime, 99.99f));
    SetWindowText(m_hwnd, title);
}
