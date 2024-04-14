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

    window_ = XCreateSimpleWindow(display_, root, 0, 0, width_, height_, 0,
                                  BlackPixel(display_, screen), WhitePixel(display_, screen));

    XStoreName(display_, window_, "KCEF Sentinel");
    XSelectInput(display_, window_, StructureNotifyMask);
    XMapWindow(display_, window_);

    XEvent event;
    do
    {
      XNextEvent(display_, &event);
    }
    while (event.type != MapNotify);
  }

  ~xwindow() {
    if (display_)
      XCloseDisplay(display_);
  }

  void focus()
  {
    XRaiseWindow(display_, window_);
    XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
  }

  void run()
  {
    XEvent event;
    while (XPending(display_))
    {
      XNextEvent(display_, &event);
      switch (event.type)
      {
        case ConfigureNotify:
          if (event.xconfigure.width != width_ || event.xconfigure.height != height_)
          {
            width_ = event.xconfigure.width;
            height_ = event.xconfigure.height;
            XResizeWindow(display_, cefwin_, width_, height_);
          }
          break;
      }
    }

  }

Window value() const
{
  return window_;
}

void set_cef(Window cefwin)
{
  cefwin_ = cefwin;
}

private:
  Display* display_;
  Window   window_;
  Window   cefwin_{0};
  int      width_{960};
  int      height_{640};
};
