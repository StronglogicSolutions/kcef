#pragma once

#include <kutils.hpp>
#include "server.hpp"
#include "interface.hpp"

class controller
{
using payload_t      = std::vector<std::string>;
using ipc_dispatch_t = std::map<uint8_t, std::function<void(ipc_msg_t)>>;
using kiq_handler_t  = std::map<std::string_view, std::function<void(payload_t)>>;

 public:
  controller(kcef_interface* kcef);
//-----------------------------------
//-----------------------------------
  enum class state
  {
    work,
    shutdown
  };
  //-----------------------------------
  state work();

 private:
  using browse_queue_t = std::deque<std::string>;

  void                  handle_queue();
  void                  enqueue(const std::string& url);
  kcef_interface*       kcef_;
  kiq::server           kiq_;
  ipc_dispatch_t        dispatch_;
  kiq_handler_t         kiq_handler;
  browse_queue_t        queue_;
  kutils::bucket<1, 5>  bucket_; // REQ/N sec
  bool                  app_waiting_{false};
};
