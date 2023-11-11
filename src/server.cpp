#include "server.hpp"
#include "include/base/cef_logging.h"

namespace kiq {
static const char* RX_ADDR{"tcp://0.0.0.0:28479"};
static const char* TX_ADDR{"tcp://0.0.0.0:28474"};
static const char* AX_ADDR{"tcp://0.0.0.0:28480"};

auto ipc_log = [](const auto* log)
{
  LOG(INFO) << "IPC - " << log;
};

server::server()
: context_{1},
  rx_(context_, ZMQ_ROUTER),
  tx_(context_, ZMQ_DEALER),
  ax_(context_, ZMQ_DEALER)
{
  rx_.set(zmq::sockopt::linger, 0);
  tx_.set(zmq::sockopt::linger, 0);
  ax_.set(zmq::sockopt::linger, 0);
  rx_.set(zmq::sockopt::routing_id, "sentinel_daemon");
  tx_.set(zmq::sockopt::routing_id, "sentinel");
  ax_.set(zmq::sockopt::routing_id, "sentinel_app");
  rx_.set(zmq::sockopt::tcp_keepalive, 1);
  tx_.set(zmq::sockopt::tcp_keepalive, 1);
  ax_.set(zmq::sockopt::tcp_keepalive, 1);
  rx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  tx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  ax_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  rx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  tx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  ax_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  rx_.bind   (RX_ADDR);
  tx_.connect(TX_ADDR);
  ax_.connect(AX_ADDR);
  kiq::set_log_fn(ipc_log);
  run();
}
//----------------------------------
server::~server()
{
  active_ = false;
  if (fut_.valid())
    fut_.wait();
}
//----------------------------------
void server::process_message(kiq::ipc_message::u_ipc_msg_ptr msg)
{
  msgs_.push_back(std::move(msg));
}
//----------------------------------
void server::run()
{
  fut_ = std::async(std::launch::async,
  [this]
  {
    while (active_)
    {
      if (!out_.empty())
      {
        LOG(INFO) << "Server has outgoing message";
        auto&& msg = out_.front();
        send_ipc_message(std::move(msg));
        LOG(INFO) << "Message sent";
        out_.pop_front();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  });
}
//----------------------------------
ipc_msg_t server::wait_and_pop()
{
  static bool from_tx = true;
  uint8_t     mask    = poll();

  if (mask & 0x01 << 1)
    recv();
  if (mask & 0x01 << 0)
    recv(from_tx);

  if (msgs_.empty())
    return nullptr;

  ipc_msg_t msg = std::move(msgs_.front());
  msgs_.pop_front();
  if (!msg)
    LOG(ERROR) << "Popped null message";
  else
    LOG(INFO) << "Popping message: " << msg->to_string();
  return msg;
}
//----------------------------------
uint8_t
server::poll()
{
  static const auto timeout   = std::chrono::milliseconds(30);
  uint8_t           poll_mask = {0x00};
  zmq::pollitem_t   items[]   = { { tx_, 0, ZMQ_POLLIN, 0},
                                  { rx_, 1, ZMQ_POLLIN, 0} };

  zmq::poll(&items[0], 2, timeout);

  if (items[0].revents & ZMQ_POLLIN)
    poll_mask |= (0x01 << 0);
  if (items[1].revents & ZMQ_POLLIN)
    poll_mask |= (0x01 << 1);
  return poll_mask;
}
//----------------------------------
zmq::socket_t& server::socket()
{
  return (reply_) ? ax_ : tx_;
}
//----------------------------------
void server::set_reply_pending(bool pending)
{
  reply_ = pending;
}
//----------------------------------
void server::on_done()
{
  reply_ = false;
}
//----------------------------------
void server::recv(bool tx)
{
  using buffers_t = std::vector<ipc_message::byte_buffer>;

  zmq::message_t  identity;
  zmq::socket_t&  sock = (tx) ? tx_ : rx_;
  if (!sock.recv(identity) || identity.empty())
  {
    LOG(ERROR) << "Failed to receive IPC: No identity";
    return;
  }

  buffers_t      buffer;
  zmq::message_t msg;
  int            more_flag{1};

  while (more_flag && sock.recv(msg))
  {
    LOG(INFO) << "Received frame: " << msg.to_string_view();
    buffer.push_back({static_cast<char*>(msg.data()), static_cast<char*>(msg.data()) + msg.size()});
    more_flag = sock.get(zmq::sockopt::rcvmore);
  }

  LOG(INFO) << "Received IPC message";
  process_message(DeserializeIPCMessage(std::move(buffer)));
}
//----------------------------------
void server::enqueue_ipc(kiq::ipc_message::u_ipc_msg_ptr msg)
{
  LOG(INFO) << "enqueueing outgoing IPC message";
  out_.emplace_back(std::move(msg));
}
} // ns kiq
