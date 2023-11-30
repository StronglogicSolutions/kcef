#include "controller.hpp"
#include <nlohmann/json.hpp>
#include "include/base/cef_logging.h"
#include <process.hpp>

using json_t    = nlohmann::json;
using kiq_msg_t = kiq::kiq_message;

std::string
escape_s(const std::string& s)
{
  std::string out;
  for (const char& c : s)
  {
    if (c == ',')
      out += "%2C";
    else
      out += c;
  }
  return out;
}

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
    }
    },
    { "sentinel:analysis",  [this](auto args)                                     // LOAD URL
    {
      LOG(INFO) << "handling analysis results. Printing args";
      for (const auto& arg : args)
        LOG(INFO) << "Arg: " << arg;
      kiq_.enqueue_ipc(std::make_unique<kiq::platform_info>("", args.at(0), "agitation analysis"));
    }
    }
  })
{
  kcef_->init([this](const std::string& s)                                       // QUERY CALLBACK
  {
    const auto url      = kcef_->get_url();
    const auto filename = kutils::get_unix_tstring() + ".html";
    kutils::SaveToFile(s, filename);

    LOG(INFO) << "Saved " << url << " to " << filename;

    if (!app_waiting_ && !app_active_)
      return;

    if (app_waiting_)
    {
      LOG(INFO) << "app was waiting";
      app_waiting_ = false;
      kiq_.enqueue_ipc(std::make_unique<kiq::platform_info>("sentinel", escape_s(s), "new_url"));
      return;
    }

    const auto process = kiq::process({"./app.sh", filename, url}, 0);          // ANALYZE
    if (process.has_error())
      LOG(ERROR) << "NodeJS app failed: " << process.get_error();
    else
      LOG(INFO)  << "NodeJS app stdout:\n" << process.get().output;

    kiq_.enqueue_ipc(std::make_unique<kiq::platform_info>("", s, "source"));
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
  if (!url.empty())
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