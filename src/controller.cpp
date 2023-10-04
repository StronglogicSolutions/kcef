#include "controller.hpp"
#include <nlohmann/json.hpp>
#include "include/base/cef_logging.h"

using json_t    = nlohmann::json;
using kiq_msg_t = kiq::kiq_message;

controller::controller(kcef_interface* kcef)
: kcef_(kcef),
  dispatch_({
    {kiq::constants::IPC_KIQ_MESSAGE, [this](auto msg)
    {
      json_t     data = json_t::parse(static_cast<kiq_msg_t*>(msg.get())->payload(), nullptr, false);
      const auto args = data["args"].get<payload_t>();
      const auto type = args.at(0);

      LOG(INFO) << "Type: " << type;

      kiq_handler.at(type)(args);
    }}}),
  kiq_handler({
    { "sentinel:messages", [this](auto args) { kcef_->set_url(args.at(1)); }},
    { "sentinel:query",    [this](auto args) { kcef_->query  (args.at(1)); }}
  })
{
  kcef_->init([this](const std::string& s)
  {
    LOG(INFO) << "Sending this source:\n" << s;
    kiq_.send_ipc_message(std::make_unique<kiq::platform_info>("", s, "source"));
  });
}
//-----------------------------------
controller::state controller::work()
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
