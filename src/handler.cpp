// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "handler.hpp"

#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#if defined(CEF_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace {

SimpleHandler* g_instance = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

SimpleHandler::SimpleHandler(bool use_views)
    : use_views_(use_views), is_closing_(false) {
  DCHECK(!g_instance);
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  if (use_views_) {
    // Set the title of the window using the Views framework.
    CefRefPtr<CefBrowserView> browser_view =
        CefBrowserView::GetForBrowser(browser);
    if (browser_view) {
      CefRefPtr<CefWindow> window = browser_view->GetWindow();
      if (window) {
        window->SetTitle(title);
      }
    }
  } else if (!IsChromeRuntimeEnabled()) {
    // Set the title of the window using platform APIs.
    PlatformTitleChange(browser, title);
  }
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
  CEF_REQUIRE_UI_THREAD();
  browsers_.insert_or_assign(DEFAULT_KCEF_ID, browser);
}

bool SimpleHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD()
  ;

  if (browsers_.size() == 1)
    // Set a flag to indicate that the window close should be allowed.
    is_closing_ = true;

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove from the list of existing browsers.
  browsers_t::iterator pair = browsers_.begin();
  for (; pair != browsers_.end(); ++pair)
  {
    if ((pair->second)->IsSame(browser))
    {
      browsers_.erase(pair);
      break;
    }
  }

  if (browsers_.empty())
    CefQuitMessageLoop();
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Allow Chrome to show the error page.
  if (IsChromeRuntimeEnabled()) {
    return;
  }

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED) {
    return;
  }

  // Display a load error message using a data: URI.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

void SimpleHandler::CloseAllBrowsers(bool force_close)
{
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
                                       force_close));
    return;
  }

  if (browsers_.empty())
    return;

  for (auto it = browsers_.begin(); it != browsers_.end(); ++it)
    (*it).second->GetHost()->CloseBrowser(force_close);
}

// static
bool SimpleHandler::IsChromeRuntimeEnabled() {
  static int value = -1;
  if (value == -1) {
    CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::GetGlobalCommandLine();
    value = command_line->HasSwitch("enable-chrome-runtime") ? 1 : 0;
  }
  return value == 1;
}


void SimpleHandler::PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                                        const CefString& title) {
  std::string titleStr(title);

#if defined(CEF_X11)
  // Retrieve the X11 display shared with Chromium.
  ::Display* display = cef_get_xdisplay();
  DCHECK(display);

  // Retrieve the X11 window handle for the browser.
  ::Window window = browser->GetHost()->GetWindowHandle();
  if (window == kNullWindowHandle) {
    return;
  }

  // Retrieve the atoms required by the below XChangeProperty call.
  const char* kAtoms[] = {"_NET_WM_NAME", "UTF8_STRING"};
  Atom atoms[2];
  int result =
      XInternAtoms(display, const_cast<char**>(kAtoms), 2, false, atoms);
  if (!result) {
    NOTREACHED();
  }

  // Set the window title.
  XChangeProperty(display, window, atoms[0], atoms[1], 8, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(titleStr.c_str()),
                  titleStr.size());

  // TODO(erg): This is technically wrong. So XStoreName and friends expect
  // this in Host Portable Character Encoding instead of UTF-8, which I believe
  // is Compound Text. This shouldn't matter 90% of the time since this is the
  // fallback to the UTF8 property above.
  XStoreName(display, browser->GetHost()->GetWindowHandle(), titleStr.c_str());
#endif  // defined(CEF_X11)
}

void SimpleHandler::set_url(const std::string& url)
{
  LOG(INFO) << "Setting URL to " << url;
  browsers_[DEFAULT_KCEF_ID]->GetMainFrame()->LoadURL(url);
}