const { spawn } = require('node:child_process')
const path      = require('path')
const fs        = require('fs').promises
const make_url  = query => `https://en.wikipedia.org/w/api.php?action=query&utf8\=&format=json&generator=search&prop=extracts&exintro=true&explaintext=true&list=search&gsrsearch=${query}&srsearch=${query}`
//--------------------------------------------
async function analyze(text, command)
{
  const def = { }
  def.p = new Promise(resolve => def.r = resolve)
  let ret

  const program  = path.join(__dirname, "../../", "third_party/knlp/out", "knlp_app")
  const args     = [`--description="${text}"`, command]
  const process  = spawn(program, args)

  console.log('Running: ', program, args)
  process.stdout.on('data', (data) =>
  {
    console.log('received stdout: ', data.toString())
    ret = data
    def.r()
  })

  process.stderr.on('data', (data) =>
  {
    console.log("Error forking process", data.toString())
    ret = data
    def.r
  })

  await def.p
  console.log('forked process returned', ret.toString())
  try
  {
    return JSON.parse(ret.toString())
  }
  catch (e)
  {
    return { value: "" }
  }
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
  if (!query.length)
    return ret

  const response   = await fetch(make_url(query)).catch(e => console.error("Fetch error", e))
  if (response && response.ok)
  {
    const data   = await response.json()
    const result = data["query"]
    if (result && "search" in result && result["search"].length)
      ret = result["pages"][result["search"][0].pageid]["extract"]
  }

  return ret
}
//---------------------------------------------
async function rotate_files()
{
  const directory = './'
  const filename = 'analysis.json';

  const filepath = path.join(directory, filename);

  try
  {
    await fs.access(filepath, fs.constants.F_OK);
    console.log(`${filename} exists in the directory.`);

    const stats         = await fs.stat(filepath);
    const last_modified = stats.mtime.getTime();
    const new_filename  = `analysis${last_modified}.json`;
    const new_filepath  = path.join(directory, new_filename);

    await fs.copyFile(filepath, new_filepath);
    console.log(`${filename} has been copied as ${new_filename}`);
  }
  catch (err)
  {
    if (err.code === 'ENOENT')
      console.error(`${filename} does not exist in the directory.`);
    else
      console.error('An error occurred:', err);
  }
}

module.exports.analyze = analyze
module.exports.get_name = get_name
module.exports.fetch_wiki = fetch_wiki
module.exports.rotate_files = rotate_files
