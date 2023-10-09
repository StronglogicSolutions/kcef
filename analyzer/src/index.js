const path = require('path')
const fs = require('fs')
const { JSDOM } = require('jsdom')
const { dockStart } = require('@nlpjs/basic');
const dockstart = dockStart
const file_path = process.argv[2]
const url       = process.argv[3]
let nlp
//--------------------------------------------
if (!file_path.length)
{
  console.error("Please provide the path to an HTML file and its originating URL as runtime arguments")
  process.exit(1)
}
//--------------------------------------------
function get_name()
{
  const full = url.substring(url.indexOf("://") + 3)
  return full.substring(0, full.lastIndexOf('.'))
}
//--------------------------------------------
async function create_analysis(items)
{
  const words  = Object.keys(JSON.parse(nlp.export(false)).ner.rules.en)
  const has_watchword = entities =>
  {
    for (const item of entities)
      if (words.find(word => { return word === item.entity }))
        return true
    return false
  }

  let   select = []
  const data   = []
  for (const text of items)
    data.push({ nlp: await nlp.process('en', text) })

  function find_candidates()
  {
    const result = []
    for (const item of data)
    {
      if (has_watchword(item.nlp.entities))
        result.push({ ...item, target: undefined })
    }
    return result
  }

  function identify_target(item)
  {
    return "noun"
  }

  function compute_resolutions()
  {
    select = find_candidates()
    for (let i = 0; i < select.length; i++)
    {
      select[i].target = identify_target(select[i])
      select[i].result = "computed"
    }
  }

  compute_resolutions()

  return { get: () => { return select } }
}
//--------------------------------------------
const handlers = {
  "twitter": async (doc) =>
  {
    const articles = [...doc.querySelectorAll("article")].map(e => { return e.textContent })
    const analysis = await create_analysis(articles)
    const result   = analysis.get()
    console.log(JSON.stringify(result))
  }
}
//--------------------------------------------
async function start()
{
  nlp = (await dockstart({
    settings: {
      nlp: {
        forceNER: true,
        languages: ['en'],
        corpora: [ path.join(__dirname, "corpus.json") ]
      }
    },
    use: ['Basic', 'LangEn'],
  })).get('nlp')

  await nlp.train()

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

  nlp.save(path.join(__dirname, "kcef_models.nlp"))
}

start()
