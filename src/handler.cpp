#include "handler.hpp"

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include <kutils.hpp>

#if defined(CEF_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

static const char*  g_result_recipient = "https://x.com/messages/1399461905148809216-1623001010623983627";

namespace {

KCEFClient* g_instance = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

static
std::string get_scroll_command(uint32_t y)
{
  return "window.scrollBy({ top: " + std::to_string(y) + ", left: 0, behavior: 'smooth' })";
}
//---------------------------------------------------------------------------
KCEFClient::KCEFClient()
: is_closing_(false),
  window_(cef_get_xdisplay())
{
  DCHECK(!g_instance);
  g_instance = this;
}
//---------------------------------------------------------------------------
KCEFClient::~KCEFClient()
{
  g_instance = nullptr;
}
//---------------------------------------------------------------------------
void KCEFClient::init(src_cb_t cb)
{
  cb_ = cb;
}

// static
//---------------------------------------------------------------------------
KCEFClient* KCEFClient::GetInstance()
{
  return g_instance;
}
//---------------------------------------------------------------------------
void KCEFClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                               const CefString&      title)
{
  CEF_REQUIRE_UI_THREAD();
    PlatformTitleChange(browser, title);
}
//---------------------------------------------------------------------------
void KCEFClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
  CEF_REQUIRE_UI_THREAD();
  browsers_.insert_or_assign(DEFAULT_KCEF_ID, browser);
  window_.set_cef(browsers_.at(DEFAULT_KCEF_ID)->GetHost()->GetWindowHandle());
}
//---------------------------------------------------------------------------
bool KCEFClient::DoClose(CefRefPtr<CefBrowser> browser)
{
  CEF_REQUIRE_UI_THREAD();

  if (browsers_.size() == 1)
    is_closing_ = true;

  return false;
}
//---------------------------------------------------------------------------
void KCEFClient::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
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
//---------------------------------------------------------------------------
void KCEFClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl)
{
  CEF_REQUIRE_UI_THREAD();

  if (IsChromeRuntimeEnabled()) // Allow Chrome to show the error page.
    return;


  if (errorCode == ERR_ABORTED) // Don't display an error for downloaded files.
    return;

  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}
//---------------------------------------------------------------------------
void KCEFClient::CloseAllBrowsers(bool force_close)
{
  if (!CefCurrentlyOn(TID_UI))
  {
    CefPostTask(TID_UI, base::BindOnce(&KCEFClient::CloseAllBrowsers, this, force_close));
    return;
  }

  if (browsers_.empty())
    return;

  for (auto it = browsers_.begin(); it != browsers_.end(); ++it)
    (*it).second->GetHost()->CloseBrowser(force_close);
}
//---------------------------------------------------------------------------
bool KCEFClient::IsChromeRuntimeEnabled()
{
  static int value = -1;
  if (value == -1)
  {
    CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::GetGlobalCommandLine();
    value = command_line->HasSwitch("enable-chrome-runtime") ? 1 : 0;
  }
  return value == 1;
}
//---------------------------------------------------------------------------
void KCEFClient::PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                                        const CefString& title)
{
  std::string titleStr(title);

#if defined(CEF_X11)
  ::Display* display = cef_get_xdisplay();
  DCHECK(display);

  ::Window window = browser->GetHost()->GetWindowHandle();
  if (window == kNullWindowHandle)
    return;

  const char* kAtoms[] = {"_NET_WM_NAME", "UTF8_STRING"};
  Atom atoms[2];
  int result = XInternAtoms(display, const_cast<char**>(kAtoms), 2, false, atoms);
  if (!result)
    NOTREACHED();

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
//---------------------------------------------------------------------------
void KCEFClient::analyze()
{
  scroll();
  query("get");
}
//---------------------------------------------------------------------------
void KCEFClient::scroll(uint32_t y) const
{
  browsers_.at(DEFAULT_KCEF_ID)->GetMainFrame()->ExecuteJavaScript(
    get_scroll_command(y), "", 1
  );
}
//---------------------------------------------------------------------------
void KCEFClient::set_url(const std::string& url) const
{
  LOG(INFO) << "Setting URL to " << url;
  browsers_.at(DEFAULT_KCEF_ID)->GetMainFrame()->LoadURL(url);
}
//---------------------------------------------------------------------------
void KCEFClient::query(const std::string& q)
{
  kutils::make_event([this]{ browsers_[DEFAULT_KCEF_ID]->GetMainFrame()->GetSource(this); });
}
//---------------------------------------------------------------------------
std::string KCEFClient::get_url() const
{
  return browsers_.at(DEFAULT_KCEF_ID)->GetMainFrame()->GetURL().ToString();
}
//---------------------------------------------------------------------------
void KCEFClient::focus()
{
  LOG(INFO) << "Bringing window to front and focusing";

  window_.focus();
}
//---------------------------------------------------------------------------
bool KCEFClient::has_focus() const
{
  return window_.is_top();
}
//---------------------------------------------------------------------------
void KCEFClient::on_finish()
{
  LOG(INFO) << "Setting window to normal";

  window_.set_top(false);
}
//---------------------------------------------------------------------------
void KCEFClient::Visit(const CefString& s)
{
  current_source_ = s.ToString();
  LOG(INFO) << "Visit()";
  cb_(current_source_);
}
//---------------------------------------------------------------------------
void KCEFClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame>   frame,
                           int                   code)
{
  LOG(INFO) << "OnLoadEnd code: " << code;
  if (code == 200)
    query("get");
  scroll(0);

  load_time_ = std::time(nullptr);

}
//---------------------------------------------------------------------------
void
KCEFClient::run ()
{
  window_.run();
}
//---------------------------------------------------------------------------
unsigned long
KCEFClient::get_window() const
{
  return window_.value();
}

double
KCEFClient::idle_time () const
{
  if (!load_time_)
  {
    LOG(WARNING) << "Cannot give idle time: no page has yet loaded";
    return kcef_interface::never_loaded();
  }

  return std::difftime(std::time(nullptr), load_time_);
}