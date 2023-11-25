// const { JSDOM } = require('jsdom')
// const fs = require('fs')
// const
// const input = fs.readFileSync(process.argv[2]).toString()
// const doc = new JSDOM(input)

// const scripts = [...doc.window.document.scripts]

const fetch_wiki = async (query) =>
{
  let   ret        = ""
  const ex_pattern = /<([^</> ]+)[^<>]*?>[^<>]*?<\/\1> */gi
  const response   = await fetch("https://en.wikipedia.org/w/api.php?action=query&utf8=&format=json&list=search&srsearch=" + query)
  if (response.ok)
  {
    const data = await response.json()
    const result = data["query"]["search"]
    for (const item of result)
      ret += item["snippet"].replaceAll(ex_pattern, '') + '\n'
  }

  if (ret.length)
    ret = ret.substring(0, ret.length - 1)
  return ret
}

async function start()
{
  const text = await fetch_wiki("Harry%20Dunn")
  console.log(text)
}

start()