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
      XNextEvent(display_, &event);
    while (event.type != MapNotify);
  }

  ~xwindow() {
    if (display_)
      XCloseDisplay(display_);
  }

  void focus()
  {
    set_top();
    XRaiseWindow(display_, window_);
    XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
  }

  void set_top(bool top = true)
  {

    XEvent event;
    std::memset(&event, 0, sizeof(event));

    Atom wm_state = XInternAtom(display_, "_NET_WM_STATE", False);
    Atom wm_state_above = XInternAtom(display_, "_NET_WM_STATE_ABOVE", False);

    event.type = ClientMessage;
    event.xclient.window = window_;
    event.xclient.message_type = wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = (top) ? 1 : 0;
    event.xclient.data.l[1] = wm_state_above;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    event.xclient.data.l[4] = 0;

    XSendEvent(display_, DefaultRootWindow(display_), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display_);

    is_top_ = top;
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

bool is_top() const
{
  return is_top_;
}

private:
  Display* display_;
  Window   window_;
  Window   cefwin_{0};
  int      width_{960};
  int      height_{640};
  bool     is_top_{false};
};
