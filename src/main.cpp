#include "app.hpp"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#include "include/base/cef_logging.h"
#include "include/cef_command_line.h"

#include <deque>
#include <kproto/ipc.hpp>
#include <zmq.hpp>

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

class server : public kiq::IPCHandlerInterface
{
 public:
  bool get_message() const
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    return false;
  }

  void process_message(kiq::ipc_message::u_ipc_msg_ptr msg) final
  {
    messages_.push_back(std::move(msg));
  }

 protected:
  zmq::socket_t& socket() final
  {
    return socket_;
  }

 private:
  using messages_t = std::deque<kiq::ipc_message::u_ipc_msg_ptr>;

  zmq::socket_t socket_;
  messages_t    messages_;
};

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

  server               kipc_server;
  bool                 shutdown = false;
  CefRefPtr<SimpleApp> app(new SimpleApp);

  CefInitialize(main_args, settings, app.get(), nullptr);

  // CefRunMessageLoop();
  while (!shutdown)
  {
    if (kipc_server.get_message())
      (void)("Do something good");

    CefDoMessageLoopWork();
  }

  CefShutdown();

  return 0;
}
