#pragma once
#include <functional>
#include <string>

using src_cb_t = std::function<void(const std::string&)>;


class kcef_interface
{
 public:
  virtual ~kcef_interface() = default;
  virtual void          init    (src_cb_t)                 = 0;
  virtual void          set_url (const std::string&) const = 0;
  virtual void          query   (const std::string&)       = 0;
  virtual std::string   get_url ()                   const = 0;
  virtual void          scroll  (uint32_t)           const = 0;
  virtual void          analyze ()                         = 0;
  virtual void          focus   ()                   const = 0;
  virtual unsigned long get_window()                 const = 0;
  virtual void          run()                              = 0;
  virtual void          on_finish()                  const = 0;
};
