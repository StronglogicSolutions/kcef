#pragma once

#include "include/cef_client.h"
#include <include/cef_request_context_handler.h>
#include "interface.hpp"
#include "window.hpp"
#include <ctime>

static const int32_t DEFAULT_KCEF_ID = 99;
class KCEFClient : public CefClient,
                   public CefDisplayHandler,
                   public CefLifeSpanHandler,
                   public CefLoadHandler,
                   public CefStringVisitor,
                   public kcef_interface
{
 public:
  explicit KCEFClient();
  ~KCEFClient() final;

  void          set_url(const std::string&) const final;
  void          query  (const std::string&)       final;
  void          init   (src_cb_t);
  void          scroll (uint32_t y = 24000) const final;
  std::string   get_url()                   const final;
  void          analyze()                         final;
  void          focus()                           final;
  bool          has_focus()                 const final;
  void          on_finish()                       final;
  void          run ()                            final;
  unsigned long get_window()                const final;
  double        idle_time ()                const final;

  static KCEFClient* GetInstance();
                                                                          // CefClient methods
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler  () override { return this; }
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler        () override { return this; }

  virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,               // CefDisplayHandler methods
                             const CefString&      title) override;

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;    // CefLifeSpanHandler methods
  virtual bool DoClose       (CefRefPtr<CefBrowser> browser) override;
  virtual void OnBeforeClose (CefRefPtr<CefBrowser> browser) override;

  virtual void OnLoadError(CefRefPtr<CefBrowser> browser,                 // CefLoadHandler methods
                           CefRefPtr<CefFrame>   frame,
                           ErrorCode             errorCode,
                           const CefString&      errorText,
                           const CefString&      failedUrl) override;

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame>   frame,
                         int                   code) final;

  void CloseAllBrowsers(bool force_close);

  bool IsClosing() const { return is_closing_; }

  static bool IsChromeRuntimeEnabled();

 private:
  using browsers_t = std::map<int32_t, CefRefPtr<CefBrowser>>;

  void PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title);

  void Visit(const CefString& s) final;

  browsers_t  browsers_;
  bool        is_closing_;
  std::string current_source_;
  src_cb_t    cb_;
  xwindow     window_;
  std::time_t load_time_{0};


  IMPLEMENT_REFCOUNTING(KCEFClient);
};
