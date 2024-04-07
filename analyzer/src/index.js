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



// Create a logger instance
// const logger = winston.createLogger({
//   level: 'info', // Adjust the log level as needed
//   format: winston.format.combine(
//     winston.format.timestamp(),
//     winston.format.json()
//   ),
//   transports: [
//     // Log to console
//     new winston.transports.Console(),
//     // Log to a file
//     new winston.transports.File({ filename: 'app.log' })
//   ]
// });

// console.log = (...args) => {
//   logger.info(...args);
// }
// Define the file path for logging
const logFilePath = path.join(__dirname, 'console.log');

// Override console.log to capture and write logs to a file
const originalConsoleLog = console.log;
console.log = function(...args) {
  const logMessage = args.join(' ');
  // Write log message to file
  fs.appendFileSync(logFilePath, logMessage + '\n');
  // Call the original console.log function to print to console
  originalConsoleLog.apply(console, args);
};

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
    const result = JSON.stringify(await analyze_tweets(nlp, doc))
    await rotate_files()
    fs.writeFileSync('./analysis.json', result)
    console.log(result)
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
