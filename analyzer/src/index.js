const path                         = require('path')
const fs                           = require('fs')
const winston                      = require('winston')
const { JSDOM                    } = require('jsdom')
const { dockStart                } = require('@nlpjs/basic')
const { DomainManager, NluNeural } = require('@nlpjs/nlu')
const { containerBootstrap       } = require('@nlpjs/core')
const { LangEn                   } = require('@nlpjs/lang-en')
const { get_name, rotate_files   } = require('./utils')
const { analyze_tweets           } = require('./handlers/twitter')
const { stdout }                   = require('process');

const logger = winston.createLogger({
  level: 'trace',
  format: winston.format.combine(
    winston.format.timestamp(),
    winston.format.json()
  ),
  transports: [
    new winston.transports.Console(),
    new winston.transports.File({ filename: path.join(__dirname, '../../build', 'console.log') })
  ]
});

console.log   = (...args) => { logger.info (...args) }
console.error = (...args) => { logger.error(...args) }
console.warn  = (...args) => { logger.warn (...args) }
console.debug = (...args) => { logger.debug(...args) }
console.trace = (...args) => { logger.trace(...args) }

const dockstart   = dockStart
const file_path   = process.argv[2]
const url         = process.argv[3]
let   nlp
//--------------------------------------------
if (!file_path.length)
{
  console.error("Please provide the path to an HTML file and its originating URL as runtime arguments")
  process.exit(1)
}
//--------------------------------------------
async function train_nlp(nlp)
{
  for (const text of ["Just letting you know", "Just letting everyone know", "Just to let you know"])
    nlp.addDocument('en', text, "implied.wisdom")
  await nlp.train()
}
//--------------------------------------------
//--------------------------------------------
const handlers =
{
  "twitter": async (doc) =>
  {
    try
    {
      const result = JSON.stringify(await analyze_tweets(nlp, doc))
      await rotate_files()
      fs.writeFileSync('./analysis.json', result)
      console.log(result)
    }
    catch (error)
    {
      console.error('Failed to analyze.', error)
      process.exit(1)
    }
  }
}
//--------------------------------------------
//--------------------------------------------
//--------------------------------------------
;//////////////////MAIN///////////////////////
(async () =>
{
  let temp = console.log   // silence bootstrap
  console.log = ()=>{}
  nlp = (await dockstart({
    settings: {
      nlp:
      {
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

    handlers[get_name(url)](doc)
  }
  catch ({ message })
  {
    console.error("Exception caught:", message)
  }

  nlp.save(path.join(__dirname, "kcef_models.nlp"))
})()
