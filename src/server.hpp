#pragma once

#include <deque>
#include <kproto/ipc.hpp>

using ipc_msg_t      = kiq::ipc_message::u_ipc_msg_ptr;
using ipc_dispatch_t = std::map<uint8_t, std::function<void(ipc_msg_t)>>;

namespace kiq
{
class server : public kiq::IPCHandlerInterface
{
 public:
  server(ipc_dispatch_t);
  ~server() final = default;

  void      process_message  (kiq::ipc_message::u_ipc_msg_ptr msg) final;
  ipc_msg_t wait_and_pop     ();
  void      set_reply_pending(bool pending = true);
  void      enqueue_ipc      (kiq::ipc_message::u_ipc_msg_ptr msg);
  void      run              ();
  void      connect          (bool reconnect = false);
  void      disconnect       ();
  void      flush            ();
  void      send             (kiq::ipc_message::u_ipc_msg_ptr msg);

 protected:
  zmq::socket_t& socket () final;
  void           on_done() final;

 private:
  uint8_t poll();
  void    recv(bool tx = false);
  //----------------------------------
  //----------------------------------
  using messages_t = std::deque<ipc_msg_t>;
  //----------------------------------
  zmq::context_t        context_;
  zmq::socket_t         rx_;
  zmq::socket_t         tx_;
  zmq::socket_t         ax_;
  messages_t            msgs_;
  messages_t            out_;
  bool                  reply_ {false};
  ipc_dispatch_t        dispatch_;
};
} // ns kiq