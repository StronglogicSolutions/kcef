#pragma once

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstdlib>
#include <cstring>
#include "include/base/cef_logging.h"

class xwindow {
public:
  xwindow(Display* display)
  : display_(display)
  {
    int screen = DefaultScreen(display_);
    Window root = RootWindow(display_, screen);

    // Create a simple window
    window_ = XCreateSimpleWindow(display_, root, 0, 0, 600, 400, 0,
                                  BlackPixel(display_, screen), WhitePixel(display_, screen));

    // Set window title
    XStoreName(display_, window_, "KCEF Sentinel");

    // Select events to listen for
    XSelectInput(display_, window_, StructureNotifyMask);

    // Display the window
    XMapWindow(display_, window_);

    // Wait for MapNotify event
    XEvent event;
    do
    {
      XNextEvent(display_, &event);
    }
    while (event.type != MapNotify);
  }

  ~xwindow() {
    if (display_)
    {
      XCloseDisplay(display_);
    }
  }

  void focus() {
    XRaiseWindow(display_, window_);
    XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
  }

  void run() {
    while (XPending(display_) > 0)
    {
      XEvent event;
      XNextEvent(display_, &event);
    }
  }

Window value() const
{
  return window_;
}

private:
  Display* display_;
  Window   window_;
};