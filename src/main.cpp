#include "app.hpp"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#include "include/base/cef_logging.h"
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

int main(int argc, char* argv[])
{
  CefMainArgs main_args(argc, argv);

  int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0)
    return exit_code;


#if defined(CEF_X11)
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromArgv(argc, argv);

  CefSettings settings;

  if (command_line->HasSwitch("enable-chrome-runtime"))
    settings.chrome_runtime = true;

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  bool                 shutdown = false;
  CefRefPtr<SimpleApp> app(new SimpleApp);

  CefInitialize(main_args, settings, app.get(), nullptr);

  // CefRunMessageLoop();
  while (!shutdown)
    CefDoMessageLoopWork();

  CefShutdown();

  return 0;
}
