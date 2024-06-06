#include "controller.hpp"
#include <nlohmann/json.hpp>
#include "include/base/cef_logging.h"

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
  kiq_({
    {kiq::constants::IPC_STATUS,         [this](auto msg) { LOG(INFO) << "Received IPC status"; kiq_.connect(); }},
    {kiq::constants::IPC_KEEPALIVE_TYPE, [this](auto msg) { (void)("NOOP"); }},
    {kiq::constants::IPC_KIQ_MESSAGE,    [this](auto msg) // IPC MSG HANDLER
    {
      json_t     data = json_t::parse(static_cast<kiq_msg_t*>(msg.get())->payload(), nullptr, false);
      const auto args = data["args"].get<payload_t>();
      const auto type = args.at(0);

      LOG(INFO) << "Type: " << type;

      try
      {
        kiq_handler.at(type)(args);
      }
      catch(const std::exception& e)
      {
        LOG(ERROR) << "Error handling message: " << e.what();
      }
    }},
    {kiq::constants::IPC_PLATFORM_INFO, [this](auto msg) // IPC INFO
    {
      const auto type = static_cast<kiq::platform_info*>(msg.get())->type();
      const auto data = static_cast<kiq::platform_info*>(msg.get())->info();

      LOG(INFO) << "Info command: " << type;
      LOG(INFO) << "Info payload: " << std::string{data.begin(), (data.size() > 500) ?
                                                        data.begin() + 500 : data.end()};
      try
      {

        if (was_sleeping_)
          msg_queue_.push_back({type, { type, data } });
        else
          kiq_handler.at(type)({ data, type });
      }
      catch(const std::exception& e)
      {
        LOG(ERROR) << "Failed to handle command. Exception: " << e.what();
      }
    }},
    {kiq::constants::IPC_OK_TYPE, [this](auto msg) { LOG(INFO) << "Received OK: " << msg->to_string(); }}}), // REPLY OK
  kiq_handler({
    { "message", [this](auto args)                                     // KIQ REQUESTS
    {
      if (app_active_)
        throw std::runtime_error{"App already active. Not changing URL"};

      kcef_->focus();
      enqueue(args.at(1));
    }
    },
    { "query",   [this](auto args)                                     // FIND SOMETHING TO ANALYZE
    {
      if (!timer_.check_and_update())
        throw std::runtime_error{"Query already in progress"};

      if (!kcef_->has_focus())
        throw std::runtime_error{"Won't run query until KCEF in focus"};

      LOG(INFO) << "Received query. Setting app to active";
      app_active_  = true;
      app_waiting_ = false;
      kcef_->query(args.at(1));
    }
    },
    { "loadurl", [this](auto args)                                     // LOAD URL
    {
      if (!app_active_)
        LOG(WARNING) << "Received loadurl request but app isn't active";

      LOG(INFO) << "handling loadurl request from analyzer. App will wait for source and then send back";
      app_waiting_ = true;
      enqueue(args.at(0));
    }
    },
    { "analysis", [this](auto args)                                    // ANALYSIS RESULTS
    {
      LOG(INFO) << "Handling analysis results";
      kcef_->on_finish();
      app_active_ = false;
      kiq_.set_reply_pending(false);
      kiq_.enqueue_ipc(std::make_unique<kiq::platform_info>("", args.at(0), "agitation analysis", ""));
      timer_.stop();
    },
    },
    { "info",  [this](auto args)                                       // ANALYZE REQUEST
    {
      LOG(INFO) << "Handling scroll test";
      kcef_->analyze();
    },
    },
    { "generate", [this](const std::vector<std::string>& args)
    {
      kiq_.enqueue_ipc(std::make_unique<kiq::platform_request>("kai", "0", "DEFAULT_USER", "generate", args.at(0)));
    },
    },
    { "generated", [this](const std::vector<std::string>& args)
    {
      kiq_.set_reply_pending();
      kiq_.enqueue_ipc(std::make_unique<kiq::platform_info>("sentinel", args.front(), "generated", ""));
    },
    }
  }),
  ksys_([this](kiq::monitor_state state)
  {
    if (state.suspend)
      was_sleeping_ = true;
    else
      wake_timer_ = kutils::timer<5000>{};
  })
{
  timer_.stop();

  kcef_->init([this](const std::string& s)                             // QUERY CALLBACK
  {
    if (!app_waiting_ && !app_active_)
    {
      LOG(INFO) << "App not waiting and not active";
      return;
    }

    const auto url = kcef_->get_url();

    if (app_waiting_) // Analyzer requests additional sources before returning result
    {
      const auto filename = kutils::get_unix_tstring() + "_user.html";
      kutils::SaveToFile(s, filename);

      LOG(INFO) << "Saved " << url << " to " << filename;
      LOG(INFO) << "App was waiting for source from new URL. Sending back to analyzer.";
      kiq_.set_reply_pending();
      kiq_.enqueue_ipc(std::make_unique<kiq::platform_info>("sentinel", escape_s(s), "new_url", ""));
      return;
    }

    LOG(INFO) << "App is active. Will process";

    const auto filename = kutils::get_unix_tstring() + ".html";
    kutils::SaveToFile(s, filename);

    LOG(INFO) << "Saved " << url << " to " << filename;

    proc_future_ = std::async(std::launch::async, [this, url, filename]
    {
      auto process = kiq::process({"./app.sh", filename, url}, 240);  // ANALYZE
      while (process.has_work())
      {
        LOG(INFO) << "process has work";
        process.do_work();
      }

      if (process.has_error())
        LOG(ERROR) << "NodeJS app error: "  << process.get_error();
      LOG(INFO)  << "NodeJS app stdout: " << process.preview();
    });
  });
}
//-----------------------------------
controller::state controller::work()
{
  try
  {
    kiq_.run();
    handle_queue();

    if (!app_active_ && proc_future_.valid())
    {
      LOG(INFO)  << "Joining analyzer thread";
      proc_future_.wait();
      proc_future_.get();
      app_waiting_ = false;
    }
    else
    if (app_active_ && timer_.check_and_update())
    {
      LOG(ERROR) << "Analyzer taking long: calling std::future::wait() on next run()";
      app_active_ = false;
      kcef_->on_finish();
    }

    kcef_->run();
  }
  catch (const std::exception& e)
  {
    LOG(ERROR) << "Exception caught in controller: " << e.what();
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
  if (!msg_queue_.empty() && wake_timer_.check_and_update())
  {
    auto&& msg = msg_queue_.front();
    kiq_handler.at(msg.first)(msg.second);
    msg_queue_.pop_front();
    was_sleeping_ = false;
  }

  if (queue_.empty() || !bucket_.request(1))
    return;

  LOG(INFO) << "Taking item from front of queue";

  kcef_->set_url(queue_.front());
  queue_.pop_front();

}
