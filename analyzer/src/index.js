const path = require('path')
const fs = require('fs')
const { JSDOM } = require('jsdom')
const { dockStart } = require('@nlpjs/basic')
const { spawn } = require('node:child_process')
const { DomainManager, NluNeural } = require('@nlpjs/nlu')
const { containerBootstrap } = require('@nlpjs/core')
const { LangEn } = require('@nlpjs/lang-en')
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
function get_name()
{
  const full = url.substring(url.indexOf("://") + 3)
  return full.substring(0, full.lastIndexOf('.'))
}
//--------------------------------------------
function get_input(doc)
{
  const input = []
  const list  = doc.querySelectorAll('[data-testid="tweetText"]')
  for (const item of list)
  {
    const parent = item.parentNode.previousElementSibling
    if (!parent)
      continue

    const user = parent.querySelector('[data-testid="User-Name"]')
    if (user)
      input.push({ text: item.textContent, username: user.firstElementChild.textContent.trim() })
  }

  return input
}
//--------------------------------------------
async function train_nlp(nlp)
{
  for (const text of ["Just letting you know", "Just letting everyone know", "Just to let you know"])
    nlp.addDocument('en', text, "implied.wisdom")
  await nlp.train()
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
  for (const item of items)
    data.push({ nlp: await nlp.process('en', item.text), username: item.username })

  async function find_candidates()
  {
    const result = []
    for (const item of data)
      if (has_watchword(item.nlp.entities))
        result.push({ ...item })
    return result
  }

  function identify_target(item)
  {
    const rank = { Person: 4, Organization: 3, Location: 2, Unknown: 1 }
    let ret
    for (const entity of item.context.entities)
      if (!entity.type in rank)
        console.warn(`${entity.type} is an unfamiliar entity`)
      else
      if (!ret || rank[entity.type] > rank[ret.type])
        ret = entity
    return ret
  }

  async function compute_resolutions()
  {
    select = await find_candidates()
    for (let i = 0; i < select.length; i++)
    {
      select[i].context   = await analyze(select[i].nlp.utterance, "context"  )
      select[i].emotion   = await analyze(select[i].nlp.utterance, "emotion"  )
      select[i].sentiment = await analyze(select[i].nlp.utterance, "sentiment")
      select[i].target    = identify_target(select[i])
      select[i].result    = "computed"
    }
  }

  await compute_resolutions()

  return { get: () => { return select } }
}
//--------------------------------------------
const handlers = {
  "twitter": async (doc) =>
  {
    const analysis = await create_analysis(get_input(doc))
    const result   = analysis.get()
    console.log(JSON.stringify(result))
  }
}
//--------------------------------------------
async function start()
{
  let temp = console.log   // silence bootstrap
  console.log = ()=>{}
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

  await train_nlp(nlp)

  console.log = temp       // restore logging

  try
  {
    const data = fs.readFileSync(file_path).toString()
    const doc  = new JSDOM(data).window.document
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
