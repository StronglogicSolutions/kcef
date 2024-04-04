#pragma once

#include "include/cef_client.h"
#include "interface.hpp"
#include <list>

static const int32_t DEFAULT_KCEF_ID = 99;
class KCEFClient : public CefClient,
                   public CefDisplayHandler,
                   public CefLifeSpanHandler,
                   public CefLoadHandler,
                   public CefStringVisitor,
                   public kcef_interface
{
 public:
  explicit KCEFClient(bool use_views);
  ~KCEFClient() final;

  void set_url(const std::string&) const final;
  void query  (const std::string&)       final;
  void init   (src_cb_t);
  void scroll (uint32_t y = 8000)  const final;
  std::string get_url()            const final;
  void analyze()                         final;

  // Provide access to the single global instance of this object.
  static KCEFClient* GetInstance();

  // CefClient methods:
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
    return this;
  }
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // CefDisplayHandler methods:
  virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                             const CefString& title) override;

  // CefLifeSpanHandler methods:
  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           ErrorCode errorCode,
                           const CefString& errorText,
                           const CefString& failedUrl) override;

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame>   frame,
                         int                   code) final;

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  bool IsClosing() const { return is_closing_; }

  // Returns true if the Chrome runtime is enabled.
  static bool IsChromeRuntimeEnabled();

 private:
  // Platform-specific implementation.
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title);

  void Visit(const CefString& s) final;

  const bool use_views_;

  // List of existing browser windows. Only accessed on the CEF UI thread.
  using browsers_t = std::map<int32_t, CefRefPtr<CefBrowser>>;
  browsers_t  browsers_;
  bool        is_closing_;
  std::string current_source_;
  src_cb_t    cb_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(KCEFClient);
};
