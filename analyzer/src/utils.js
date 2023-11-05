const { spawn } = require('node:child_process')
const path      = require('path')

async function analyze(text, command)
{
  let r
  let p = new Promise(resolve => r = resolve)
  let ret

  const process  = spawn(path.join(__dirname, "../../", "third_party/knlp/out", "knlp_app"), [`--description="${text}"`, command])

  process.stdout.on('data', (data) =>
  {
    ret = data
    r()
  })

  process.stderr.on('data', (data) =>
  {
    console.error("Error forking process", data.toString())
    ret = data
    r()
  })

  await p

  return JSON.parse(ret.toString())
}
//--------------------------------------------
function get_name(url)
{
  const full = url.substring(url.indexOf("://") + 3)
  return full.substring(0, full.lastIndexOf('.'))
}

module.exports.analyze = analyze
module.exports.get_name = get_name

