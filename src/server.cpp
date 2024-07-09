#include "server.hpp"
#include "include/base/cef_logging.h"

namespace kiq {
static const char*       RX_ADDR  {"tcp://0.0.0.0:xxxxx"};
static const char*       TX_ADDR  {"tcp://0.0.0.0:xxxxx"};
static const char*       AX_ADDR  {"tcp://0.0.0.0:xxxxx"};
static const std::string PEER_NAME{"sentinel"};
auto ipc_log = [](const auto* log)
{
  LOG(INFO) << "IPC - " << log;
};

server::server(ipc_dispatch_t dispatch)
: context_{1},
  rx_(context_, ZMQ_ROUTER),
  tx_(context_, ZMQ_DEALER),
  ax_(context_, ZMQ_DEALER),
  dispatch_(dispatch)
{
  kiq::set_log_fn(ipc_log);

  connect();
}
//----------------------------------
void server::process_message(kiq::ipc_message::u_ipc_msg_ptr msg)
{
  msgs_.push_back(std::move(msg));
}
//----------------------------------
void server::connect(bool reconnect)
{
  if (reconnect)
    disconnect();

  LOG(INFO) << "Connecting sockets";

  try
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

    LOG(INFO) << "Set socket options";

    rx_.bind   (RX_ADDR);
    tx_.connect(TX_ADDR);
    ax_.connect(AX_ADDR);

    LOG(INFO) << "Sockets connected";
  }
  catch(const std::exception& e)
  {
    LOG(ERROR) << "Exception caught connecting sockets: " << e.what();
  }

  enqueue_ipc(std::make_unique<status_check>());
}
//----------------------------------
void server::disconnect()
{
  LOG(INFO) << "Closing sockets";
  try
  {
    rx_ = zmq::socket_t(context_, ZMQ_ROUTER);
    ax_ = zmq::socket_t(context_, ZMQ_DEALER);
    tx_ = zmq::socket_t(context_, ZMQ_DEALER);
  }
  catch(const std::exception& e)
  {
    LOG(ERROR) << "Error closing sockets." << e.what();
  }

}
//----------------------------------
void server::run()
{
  if (!out_.empty())                         // TX
  {
    auto&&  msg  = out_.front();
    if (msg)
    {
      uint8_t type = msg->type();
      send_ipc_message(std::move(msg));

      if (type != constants::IPC_KEEPALIVE_TYPE)
        LOG(INFO) << "Server sent IPC of type: " << constants::IPC_MESSAGE_NAMES.at(type);
    }
    out_.pop_front();
  }

  if (auto msg = wait_and_pop())             // RX
    dispatch_.at(msg->type())(std::move(msg));
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
  if (msg->type() != 0x01)
  {
    const auto full_msg = msg->to_string();
    const auto preview  = (full_msg.size() > 500) ? full_msg.substr(0, 500) : full_msg;
    LOG(INFO) << "Popping message: " << preview;
  }

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
  if (pending == reply_)
    return;

  if (pending)
    LOG(INFO) << "Messages will be sent to Analyzer";
  else
    LOG(INFO) << "Messages will be sent to KIQ";

  reply_ = pending;
}
//----------------------------------
void server::on_done()
{
  set_reply_pending(false);
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
    buffer.push_back({static_cast<char*>(msg.data()), static_cast<char*>(msg.data()) + msg.size()});
    more_flag = sock.get(zmq::sockopt::rcvmore);
  }

  process_message(DeserializeIPCMessage(std::move(buffer)));
}
//----------------------------------
void server::enqueue_ipc(kiq::ipc_message::u_ipc_msg_ptr msg)
{
  const auto& input = msg->to_string();
  const auto& msg_s = input.size() > 500 ? input.substr(0, 500) : input;

  if (msg->type() != constants::IPC_KEEPALIVE_TYPE)
    LOG(INFO) << "enqueueing outgoing IPC message: " << msg_s;

  out_.emplace_back(std::move(msg));
}
//----------------------------------
void server::flush()
{
  LOG(INFO) << "Flushing incoming IPC messages";
  zmq::message_t message;
  while (true)
  {
    try
    {
      if (!rx_.recv(message, zmq::recv_flags::dontwait))
        break;
    }
    catch (const zmq::error_t& e)
    {
      if (e.num() == EAGAIN)
        break;
      else
      {
        LOG(ERROR) << "Error flushing socket: " << e.what();
        break;
      }
    }
  }
  LOG(INFO) << "Finished flushing RX socket";
}
} // ns kiq
