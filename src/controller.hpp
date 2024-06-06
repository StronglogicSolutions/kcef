#pragma once

#include <kutils.hpp>
#include <ksys.hpp>
#include "server.hpp"
#include "interface.hpp"
#include <process.hpp>

class controller
{
using payload_t      = std::vector<std::string>;
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
  using message_queue_t = std::deque<std::pair<std::string, std::vector<std::string>>>;

  void                  handle_queue();
  void                  enqueue(const std::string& url);
  kcef_interface*       kcef_;
  kiq::server           kiq_;
  kiq_handler_t         kiq_handler;
  message_queue_t       msg_queue_;
  browse_queue_t        queue_;
  kutils::bucket<1, 5>  bucket_; // REQ/N sec
  kutils::timer<900000> timer_;  // 300 seconds / 15 minutes
  bool                  app_waiting_{false};
  bool                  app_active_{false};
  bool                  was_sleeping_{false};
  std::future<void>     proc_future_;
  kiq::ksys             ksys_;
  kutils::timer<5000>   wake_timer_;
};
