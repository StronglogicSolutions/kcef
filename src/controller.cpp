#include "controller.hpp"
#include <nlohmann/json.hpp>
#include "include/base/cef_logging.h"
#include <process.hpp>

using json_t    = nlohmann::json;
using kiq_msg_t = kiq::kiq_message;

controller::controller(kcef_interface* kcef)
: kcef_(kcef),
  dispatch_({
    {kiq::constants::IPC_KIQ_MESSAGE, [this](auto msg) // IPC MSG HANDLER
    {
      json_t     data = json_t::parse(static_cast<kiq_msg_t*>(msg.get())->payload(), nullptr, false);
      const auto args = data["args"].get<payload_t>();
      const auto type = args.at(0);

      LOG(INFO) << "Type: " << type;

      kiq_handler.at(type)(args);
    }},
    {kiq::constants::IPC_PLATFORM_INFO, [this](auto msg) // IPC INFO
    {
      const auto type = static_cast<kiq::platform_info*>(msg.get())->type();
      const auto data = static_cast<kiq::platform_info*>(msg.get())->info();

      LOG(INFO) << "Info command: " << type;
      LOG(INFO) << "Info payload: " << data;
      try
      {
        kiq_handler.at(type)({ data });
      }
      catch(const std::exception& e)
      {
        LOG(ERROR) << "Failed to handle command. Exception: " << e.what();
      }
    }},
    {kiq::constants::IPC_OK_TYPE, [this](auto msg) { LOG(INFO) << "Received OK: " << msg->to_string(); }}}), // REPLY OK
  kiq_handler({
    { "sentinel:messages", [this](auto args) { enqueue(args.at(1));        }},   // KIQ REQUESTS
    { "sentinel:query",    [this](auto args)                                     // FIND SOMETHING TO ANALYZE
    {
      app_active_ = true;
      kcef_->query  (args.at(1));
    }
    },
    { "sentinel:loadurl",  [this](auto args)                                     // LOAD URL
    {
      LOG(INFO) << "handling loadurl";
      app_waiting_ = true;
      kiq_.set_reply_pending();
      enqueue(args.at(0));
    }}
  })
{
  kcef_->init([this](const std::string& s)                                       // query callback
  {
    const auto url      = kcef_->get_url();
    const auto filename = kutils::get_unix_tstring() + ".html";
    kutils::SaveToFile(s, filename);

    LOG(INFO) << "Saved " << url;

    if (!app_waiting_ && !app_active_)
      return;

    if (app_waiting_) // TODO: this is messy
    {
      LOG(INFO) << "app was waiting";
      app_waiting_ = false;
      kiq_.send_ipc_message(std::make_unique<kiq::platform_info>("sentinel", "new_url", s));
      return;
    }

    kutils::make_event([&filename, &url, this]
    {
      const auto result = kiq::qx({"./app.sh", filename, url});                    // ANALYZE
      if (result.error)
        LOG(ERROR) << "NodeJS app failed: " << result.output;
      else
        LOG(INFO)  << "NodeJS app stdout:\n" << result.output;
      kiq_.send_ipc_message(std::make_unique<kiq::platform_info>("", result.output, "agitation analysis"));
    }, 0);
    kiq_.send_ipc_message(std::make_unique<kiq::platform_info>("", s,             "source"));
  });
}
//-----------------------------------
controller::state controller::work()
{
  try
  {
    if (auto msg = kiq_.wait_and_pop())
      dispatch_.at(msg->type())(std::move(msg));

    handle_queue();
  }
  catch (const std::exception& e)
  {
    LOG(ERROR) << "Exception caught in controller: " << e.what();
    return state::shutdown;
  }

  return state::work;
}
//----------------------------------
void controller::enqueue(const std::string& url)
{
  queue_.push_back(url);
}
//----------------------------------
void controller::handle_queue()
{
  if (queue_.empty() || !bucket_.request(1))
    return;

  LOG(INFO) << "Taking item from front of queue";

  kcef_->set_url(queue_.front());
  queue_.pop_front();
}