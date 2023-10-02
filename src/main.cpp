#include "app.hpp"
#include "handler.hpp"
#include <nlohmann/json.hpp>

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

using cef_app_t      = CefRefPtr<SimpleApp>;
using cef_cmd_line_t = CefRefPtr<CefCommandLine>;
using ipc_msg_t      = kiq::ipc_message::u_ipc_msg_ptr;

namespace kiq {
static const char* RX_ADDR{"tcp://0.0.0.0:28479"};
static const char* TX_ADDR{"tcp://0.0.0.0:28473"};
class server : public kiq::IPCHandlerInterface
{
 public:
  server()
  : context_{1},
    rx_(context_, ZMQ_ROUTER),
    tx_(context_, ZMQ_DEALER)
  {
    rx_.set(zmq::sockopt::linger, 0);
    tx_.set(zmq::sockopt::linger, 0);
    rx_.set(zmq::sockopt::routing_id, "katrix_daemon");
    tx_.set(zmq::sockopt::routing_id, "katrix_daemon_tx");
    rx_.set(zmq::sockopt::tcp_keepalive, 1);
    tx_.set(zmq::sockopt::tcp_keepalive, 1);
    rx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
    tx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
    rx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
    tx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);

    rx_.bind   (RX_ADDR);
    tx_.connect(TX_ADDR);

    future_ = std::async(std::launch::async, [this] { run(); });
  }

  void process_message(kiq::ipc_message::u_ipc_msg_ptr msg) final
  {
    msgs_.push_back(std::move(msg));
  }

  ipc_msg_t wait_and_pop()
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    if (msgs_.empty())
     return nullptr;
    ipc_msg_t msg = std::move(msgs_.front());
    msgs_.pop_front();

    LOG(INFO) << "Popping message: " << msg->to_string();
    return msg;
  }

  //----------------------------------
  ~server()
  {
    active_ = false;
    if (future_.valid())
      future_.wait();
  }
  //----------------------------------
  bool is_active() const
  {
    return active_;
  }

 protected:
  zmq::socket_t& socket() final
  {
    return tx_;
  }

 private:
  void run()
  {
    while (active_)
      recv();
  }
  //----------------------------------
  void recv()
  {
    using buffers_t = std::vector<ipc_message::byte_buffer>;

    zmq::message_t identity;
    if (!rx_.recv(identity) || identity.empty())
    {
      LOG(ERROR) << "Failed to receive IPC: No identity";
      return;
    }

    buffers_t      buffer;
    zmq::message_t msg;
    int            more_flag{1};

    while (more_flag && rx_.recv(msg))
    {
      buffer.push_back({static_cast<char*>(msg.data()), static_cast<char*>(msg.data()) + msg.size()});
      more_flag = rx_.get(zmq::sockopt::rcvmore);
    }

    LOG(INFO) << "Received IPC message";
    process_message(DeserializeIPCMessage(std::move(buffer)));
  }
  //----------------------------------
  //----------------------------------
  using messages_t = std::deque<ipc_msg_t>;
  //----------------------------------
  zmq::context_t        context_;
  zmq::socket_t         rx_;
  zmq::socket_t         tx_;
  std::future<void>     future_;
  bool                  active_{true};
  messages_t            msgs_;
};
} // ns kiq
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
//-----------------------------------
class controller
{
using json_t         = nlohmann::json;
using payload_t      = std::vector<std::string>;
using ipc_dispatch_t = std::map<uint8_t, std::function<void(ipc_msg_t)>>;
using kiq_handler_t  = std::map<std::string_view, std::function<void(payload_t)>>;

 public:
  controller(KCEFClient* kcef)
  : kcef_(kcef)
  {}
//-----------------------------------
//-----------------------------------
  enum class state
  {
    work,
    shutdown
  };
  //-----------------------------------
  state work()
  {
    try
    {
      if (auto msg = kiq_.wait_and_pop())
        dispatch_.at(msg->type())(std::move(msg));
    }
    catch (const std::exception& e)
    {
      LOG(ERROR) << "Exception caught in controller: " << e.what();
      return state::shutdown;
    }

    return state::work;
  }

 private:
  KCEFClient*    kcef_;
  kiq::server    kiq_;

  ipc_dispatch_t dispatch_
  {
    {kiq::constants::IPC_KIQ_MESSAGE, [this](auto msg)
    {
      json_t     data = json_t::parse(static_cast<kiq::kiq_message*>(msg.get())->payload(), nullptr, false);
      const auto args = data["args"].get<payload_t>();
      const auto type = args.at(0);

      LOG(INFO) << "Type: " << type;

      kiq_handler.at(type)(args);
    }}
  };
  //-----------------------------------
  kiq_handler_t kiq_handler{
  {
    "sentinel:messages", [this](auto args) { kcef_->set_url(args.at(1)); }}
  };
};

//----------------------------------
//-------------MAIN-----------------
//----------------------------------
int main(int argc, char** argv)
{
#if defined(CEF_X11)
  XSetErrorHandler  (XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

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
