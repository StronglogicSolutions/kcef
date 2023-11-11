const zmq  = require("zeromq")
const { kproto, deserialize } = require('../../third_party/kproto/js/kproto')

const command = "loadurl"
const appname = "sentinel"
const tx_addr = "tcp://localhost:28479"
const rx_addr = "tcp://0.0.0.0:28480"

function create_controller()
{
  const controller =
  {
    tx_: new zmq.Dealer(),
    rx_: new zmq.Router(),

    start: async function()
    {
      this.tx_.connect(tx_addr)
      await this.rx_.bind(rx_addr);
    },

    send: async function(url)
    {
      await this.tx_.send(kproto(command, url, appname))
    },

    recv: async function()
    {
      const buffer = await this.rx_.receive()
      const frames = buffer.toString().split(',')
      const sender = frames.shift()                // Should be sentinel
      return deserialize(frames)
    }
  }

  return controller
}

module.exports.create_controller = create_controller
