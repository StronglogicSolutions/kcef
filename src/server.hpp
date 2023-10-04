#pragma once

#include <deque>
#include <kproto/ipc.hpp>

using ipc_msg_t      = kiq::ipc_message::u_ipc_msg_ptr;

namespace kiq
{
class server : public kiq::IPCHandlerInterface
{
 public:
  server();
  ~server() final = default;
  void      process_message(kiq::ipc_message::u_ipc_msg_ptr msg) final;
  ipc_msg_t wait_and_pop();

 protected:
  zmq::socket_t& socket() final;

 private:
  void    run ();
  uint8_t poll();
  void    recv(bool tx = false);
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