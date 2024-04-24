const zmq  = require("zeromq")
const { kproto, deserialize } = require('../../third_party/kproto/js/kproto')
const fs   = require('fs')
const net  = require('net')
const command = "loadurl"
const appname = "sentinel"
const tx_addr = "tcp://localhost:28479"
const rx_addr = "tcp://0.0.0.0:28480"

function port_available(port)
{
  return new Promise((resolve) =>
  {
      const tester = net.createServer()
          .once('error', (err) =>
          {
            if (err.code === 'EADDRINUSE')
              resolve(false)
             else
              resolve(true)
          })
          .once('listening', () =>
          {
            tester.once('close', () =>
            {
              resolve(true);
            }).close()
          })
          .listen(port)
  })
}

function create_controller()
{
  const context = new zmq.Context({blocky: false})

  const controller =
  {
    tx_: new zmq.Dealer({ context }),
    rx_: new zmq.Router({ context }),

    start: async function()
    {
      if (!await port_available(28480))
        throw new Error('Port is unavailable')

      console.log('connecting to socket')
      this.tx_.connect(tx_addr)
      await this.rx_.bind(rx_addr);
    },

    stop: function()
    {
      this.tx_.close()
      this.rx_.close()
    },

    send: async function(data, cmd = command)
    {
      await this.tx_.send(kproto(cmd, data, appname))
    },

    recv: async function()
    {
      const buffer = await (await this.rx_.receive())
      const frames = buffer.toString().split(',')
      const sender = frames.shift()                // Should be sentinel
      return deserialize(frames)
    }
  }

  return controller
}

module.exports.create_controller = create_controller
