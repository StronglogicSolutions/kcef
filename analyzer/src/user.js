const user_target = '[data-testid="UserName"]'
const { JSDOM } = require('jsdom')
const fs = require('fs')

async function read_user(data, name)
  {
    const result = {
      "agitator": false,
      "types": [],
      "score": 0,
      "description": ""
    }
    const dom           = new JSDOM(data)
    console.log(dom)
    const doc           = new JSDOM(data).window.document
    const selector      = doc.querySelector(user_target)
    console.log('selector', selector)
    if (!selector)
      return result

    result.description  = selector.nextElementSibling.textContent

    return result
  }

  async function main()
  {
    const data = fs.readFileSync('./userpage.html')
    console.log(data.toString())
    const user = await read_user(data, 'Jairo_I_Funez')
    console.log(user)
  }

  main()
