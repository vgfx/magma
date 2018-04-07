#pragma once

#include <minwindef.h>

// GUI Window.
struct Window
{
public:
   // Creates a window; takes the client (drawable) area dimensions (in pixels) as input.
   static void open(const uint16_t width, const uint16_t height);
   // Returns the handle of the application.
   static HINSTANCE instance();
   // Returns the handle of the window.
   static HWND handle();
   // Returns the client (drawable) area width (in pixels).
   static uint16_t width();
   // Returns the client (drawable) area height (in pixels).
   static uint16_t height();
   // Displays information (in milliseconds) in the title bar.
   // 'cpuFrameTime', 'gpuFrameTime': the frame times (in milliseconds) of CPU/GPU time lines. 
   static void updateTitleBar(const float cpuFrameTime, const float gpuFrameTime);
private:
   static uint16_t  m_width, m_height; // Client area dimensions
   static HWND      m_hwnd;
   static HINSTANCE m_hinst;
};
