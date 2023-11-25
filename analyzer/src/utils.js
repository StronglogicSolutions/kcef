const { spawn } = require('node:child_process')
const path      = require('path')
const wiki_url  = "https://en.wikipedia.org/w/api.php?action=query&utf8=&format=json&list=search&srsearch="

//--------------------------------------------
async function analyze(text, command)
{
  let r
  let p = new Promise(resolve => r = resolve)
  let ret
  const program  = path.join(__dirname, "../../", "third_party/knlp/out", "knlp_app")
  const args     = [`--description="${text}"`, command]
  const process  = spawn(program, args)

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
//--------------------------------------------
const fetch_wiki = async (query) =>
{
  let   ret        = ""
  const ex_pattern = /<([^</> ]+)[^<>]*?>[^<>]*?<\/\1> */gi
  const response   = await fetch(wiki_url + query)
  if (response.ok)
  {
    const data = await response.json()
    const result = data["query"]
    if (result && "search" in result)
      for (const item of result["search"])
        ret += item["snippet"].replaceAll(ex_pattern, '') + '\n'
  }

  if (ret.length)
    ret = ret.substring(0, ret.length - 1)
  return ret
}
module.exports.analyze = analyze
module.exports.get_name = get_name
module.exports.fetch_wiki = fetch_wiki

