#include "app.hpp"
#include "handler.hpp"
#include "controller.hpp"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#include "include/cef_command_line.h"


#if defined(CEF_X11)
namespace {

int XErrorHandlerImpl(Display* display, XErrorEvent* event)
{
  LOG(WARNING) << "X error received: "
               << "type " << event->type << ", "
               << "serial " << event->serial << ", "
               << "error_code " << static_cast<int>(event->error_code) << ", "
               << "request_code " << static_cast<int>(event->request_code)
               << ", "
               << "minor_code " << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display* display)
{
  return 0;
}

}  // namespace
#endif  // defined(CEF_X11)

using cef_app_t      = CefRefPtr<SimpleApp>;
using cef_cmd_line_t = CefRefPtr<CefCommandLine>;
using ipc_msg_t      = kiq::ipc_message::u_ipc_msg_ptr;

//----------------------------------
CefSettings get_settings(int argc, char** argv)
{
  CefSettings    ret;
  cef_cmd_line_t command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromArgv(argc, argv);

  if (command_line->HasSwitch("enable-chrome-runtime"))
    ret.chrome_runtime = true;
  return ret;
}

//----------------------------------
//-------------MAIN-----------------
//----------------------------------
int main(int argc, char** argv)
{
#if defined(CEF_X11)
  XSetErrorHandler  (XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  kiq::set_log_fn([](const char* log) { LOG(INFO) << "KIQIPCLOG :: " << log; });

  CefMainArgs main_args(argc, argv);
  int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0)
    return exit_code;

  cef_app_t app = new SimpleApp;
  CefInitialize(main_args, get_settings(argc, argv), app.get(), nullptr);

  controller controller{static_cast<KCEFClient*>(app->GetDefaultClient().get())};

  while (controller.work() != controller::state::shutdown)
    CefDoMessageLoopWork();

  CefShutdown();

  return 0;
}
