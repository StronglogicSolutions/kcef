#include "server.hpp"
#include "include/base/cef_logging.h"

namespace kiq {
static const char* RX_ADDR{"tcp://0.0.0.0:28479"};
static const char* TX_ADDR{"tcp://0.0.0.0:28474"};

server::server()
: context_{1},
  rx_(context_, ZMQ_ROUTER),
  tx_(context_, ZMQ_DEALER)
{
  rx_.set(zmq::sockopt::linger, 0);
  tx_.set(zmq::sockopt::linger, 0);
  rx_.set(zmq::sockopt::routing_id, "sentinel_daemon");
  tx_.set(zmq::sockopt::routing_id, "sentinel");
  rx_.set(zmq::sockopt::tcp_keepalive, 1);
  tx_.set(zmq::sockopt::tcp_keepalive, 1);
  rx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  tx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  rx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  tx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);

  rx_.bind   (RX_ADDR);
  tx_.connect(TX_ADDR);

  future_ = std::async(std::launch::async, [this] { run(); });
}
//----------------------------------
void server::process_message(kiq::ipc_message::u_ipc_msg_ptr msg)
{
  msgs_.push_back(std::move(msg));
}
//----------------------------------
ipc_msg_t server::wait_and_pop()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  if (msgs_.empty())
    return nullptr;
  ipc_msg_t msg = std::move(msgs_.front());
  msgs_.pop_front();

  LOG(INFO) << "Popping message: " << msg->to_string();
  return msg;
}

//----------------------------------
server::~server()
{
  active_ = false;
  if (future_.valid())
    future_.wait();
}
//----------------------------------
bool server::is_active() const
{
  return active_;
}
//----------------------------------
zmq::socket_t& server::socket()
{
  return tx_;
}
//----------------------------------
void server::run()
{
  while (active_)
    recv();
}
//----------------------------------
void server::recv()
{
  using buffers_t = std::vector<ipc_message::byte_buffer>;

  zmq::message_t identity;
  if (!rx_.recv(identity) || identity.empty())
  {
    LOG(ERROR) << "Failed to receive IPC: No identity";
    return;
  }

  buffers_t      buffer;
  zmq::message_t msg;
  int            more_flag{1};

  while (more_flag && rx_.recv(msg))
  {
    buffer.push_back({static_cast<char*>(msg.data()), static_cast<char*>(msg.data()) + msg.size()});
    more_flag = rx_.get(zmq::sockopt::rcvmore);
  }

  LOG(INFO) << "Received IPC message";
  process_message(DeserializeIPCMessage(std::move(buffer)));
}
} // ns kiq
