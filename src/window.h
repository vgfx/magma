#pragma once

#include <minwindef.h>

// GUI Window.
struct Window
{
public:
   // Creates a window; takes the client (drawable) area dimensions (in pixels) as input.
   static void Create(const uint16_t width, const uint16_t height);
   // Makes the window visible.
   static void Show();
   // Makes the window invisible.
   static void Hide();
   // Returns the handle of the application.
   static HINSTANCE Instance();
   // Returns the handle of the window.
   static HWND Handle();
   // Returns the client (drawable) area width (in pixels).
   static uint16_t Width();
   // Returns the client (drawable) area height (in pixels).
   static uint16_t Height();
   // Displays information (in milliseconds) in the title bar.
   // 'cpuFrameTime', 'gpuFrameTime': the frame times (in milliseconds) of CPU/GPU time lines. 
   static void UpdateTitle(const float cpuFrameTime, const float gpuFrameTime);
private:
   static uint16_t  m_width, m_height; // Client area dimensions
   static HWND      m_hwnd;
   static HINSTANCE m_hinst;
};
