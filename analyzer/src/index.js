const path = require('path')
const fs = require('fs')
const { JSDOM } = require('jsdom')
const { NlpManager } = require('node-nlp');
const nlp = new NlpManager({ languages: ['en'], forceNER: true })
const file_path = process.argv[2]
const url       = process.argv[3]

if (!file_path.length)
{
  console.error("Please provide the path to an HTML file and its originating URL as runtime arguments")
  process.exit(1)
}

function get_name()
{
  const full = url.substring(url.indexOf("://") + 3)
  return full.substring(0, full.lastIndexOf('.'))
}

async function create_analysis(items)
{
  const data = []

  for (const item of items)
    data.push({ nlp: await nlp.process('en', item.textContent) })

  function find_candidates()
  {
    return data
  }

  function identify_target(item)
  {
    return "noun"
  }

  function compute_resolutions()
  {
    for (let i = 0; data.length; i++)
    {
      data[i].target = identify_target(data[i])
      data[i].result = "computed"
    }
  }

  return {
    get: function()
    {
      return data
    }
  }
}

const handlers = {
  "twitter": async (doc) =>
  {
    const articles = doc.querySelectorAll("article")
    const analysis = await create_analysis(articles)
    const result   = analysis.get()
    for (const detail of result)
      console.log(detail)
  }
}

async function start()
{
  try
  {
    const data = fs.readFileSync(file_path).toString()
    const doc = new JSDOM(data).window.document
    const name = get_name()
    handlers[name](doc)
  }
  catch ({ message })
  {
    console.error("Exception caught:", message)
  }
}

start()
