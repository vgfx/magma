#pragma once

#include "definitions.h"

#include <WinDef.h>

// GUI Window.
class Window
{
public:
   RULE_OF_FIVE_MOVE_ONLY(Window);

   // Creates a window; takes the client (drawable) area dimensions (in pixels) as input.
   Window(const uint16_t width, const uint16_t height);

   // Makes the window visible.
   void Show() const;

   // Makes the window invisible.
   void Hide() const;

   // Returns the handle of the application.
   HINSTANCE Instance() const;

   // Returns the handle of the window.
   HWND Handle() const;

   // Returns the client (drawable) area width (in pixels).
   uint16_t Width() const;

   // Returns the client (drawable) area height (in pixels).
   uint16_t Height() const;

   // Displays information (in milliseconds) in the title bar.
   // 'cpuFrameTime', 'gpuFrameTime': the frame times (in milliseconds) of CPU/GPU time lines. 
   void UpdateTitle(const float cpuFrameTime, const float gpuFrameTime) const;

private:

   uint16_t  m_width, m_height; // Client area dimensions
   HWND      m_hwnd;
   HINSTANCE m_hinst;
};
