const { analyze, fetch_wiki } = require('../utils')
const { create_controller   } = require('../socket')
const { JSDOM               } = require('jsdom')
const fs = require('fs')
const text_target = '[data-testid="tweetText"]'
const name_target = '[data-testid="User-Name"]'
const user_target = '[data-testid="UserName"]'
const controller  = create_controller()

//--------------------------------------------
async function create_analysis(nlp, doc)
{
  const items         = get_input(doc)
  const words         = Object.keys(JSON.parse(nlp.export(false)).ner.rules.en)
  const user_url      = name => { return `https://twitter.com/${name}` }
  const has_watchword = entities =>
  {
    for (const item of entities)
      if (words.find(word => { return word === item.entity }))
        return true
    return false
  }

  let   select = []
  const data   = []

  await controller.start()

  for (const item of items)
    data.push({ nlp: await nlp.process('en', item.text), username: item.username })

  //--------------
  async function find_candidates()
  {
    const result = []
    for (const item of data)
      if (has_watchword(item.nlp.entities))
        result.push({ ...item })
    return result
  }
  //--------------
  async function read_user(data, name)
  {
    const result = {
      "agitator": false,
      "types": [],
      "score": 0,
      "description": ""
    }

    const doc           = new JSDOM(data).window.document
    const selector      = doc.querySelector(user_target)
    if (!selector)
      return result

    result.description  = selector.nextElementSibling.textContent
    const user_analysis = await nlp.process('en', result.description)
    const name_analysis = await nlp.process('en', name)
    let   imp_idx       = 0
    for (const entity of user_analysis.entities)
    {
      if (entity.entity.includes("agitator"))
      {
        result.types.push(entity.option)
        result.score++
        if (entity.entity === "agitator_exp" || ++imp_idx > 2)
          result.agitator = true
      }
    }
    for (const entity of name_analysis.entities)
    {
      if (entity.entity.includes("agitator"))
      {
        result.types.push(entity.option)
        result.score++
        if (entity.entity === "agitator_exp" || ++imp_idx > 2)
          result.agitator = true
      }
    }

    return result
  }
  //--------------
  function identify_target(item)
  {
    const rank = { Person: 4, Organization: 3, Location: 2, Unknown: 1, None: 0 }
    let ret = { value: "", type: "None" }
    if (!item.context || !item.context.entities || !item.context.entities.length)
      return ret

    for (const entity of item.context.entities)
      if (!entity.type in rank)
        console.warn(`${entity.type} is an unfamiliar entity`)
      else
      if (!ret || rank[entity.type] > rank[ret.type])
        ret = entity
    return ret
  }
  //--------------
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
  //--------------
  async function formulate_strategy(data)
  {

    const is_good_context = context =>
    {
      if (context.message.length > 10)
      {
        const words = context.message.split(' ')
        let hash_count = 0
        for (const word of words)
        {
          if (word.startsWith('#'))
            hash_count++
        }
        return (hash_count < (0.8 * words.length))
      }
      return false
    }

    const text          = data.nlp.utterance
    const user          = data.user
    const contexts      = data.context
    const phrases       = contexts.map(context => { return context.message } )
    const is_negative   = data.nlp.sentiment.score < 0
    let   target        = data.target.value
    if (!target)
    {
      for (const context of contexts)
      {
        for (const entity of context.entities)
        {
          if (entity.type != 'unknown')
          {
            target = entity.value
            break
          }
        }
        if (target)
          break
      }

      if (!target)
      {
        if (data.nlp.entities.length)
        {
          const entity = data.nlp.entities[0]
          target = entity.utteranceText
          data.target.value =  target
        }
      }

      if (target)
        data.target.value = target
    }

    for (const context of contexts)
    {
      if (context.objective.includes("assertion")     &&
          context.objective.includes("single phrase") &&
          is_good_context(context))
      {
        let final_target
        if (context.subjective)
          final_target = (context.subjective.includes(target)) ? target : context.subjective
        else
        if (target)
          final_target = target

        if (final_target)
        {
          const verb = (await analyze(context.message, "verb"       ))["value"]
          const prep = (await analyze(context.message, "preposition"))["value"]

          return {
            text,
            user,
            context,
            is_assertion: context.objective.includes("assertion"),
            is_question: context.objective.includes("question"),
            is_imperative: context.objective.includes("imperative"),
            is_negative,
            response: `Why should anyone believe you ${verb} ${prep} ${final_target}?`,
            target: final_target,
            wiki: await fetch_wiki(encodeURI(final_target))
          }
        }
      }
    }

    return { text, error: "Failed to compute strategy" }
  }
  //--------------
  async function fetch_users()
  {
    for (let i = 0; i < select.length; i++)
    {
      await controller.send(user_url(select[i].username))
      const received = await controller.recv()
      select[i].user = await read_user(received, select[i].username)
      if (select[i].user.agitator)
        select[i].strategy = await formulate_strategy(select[i])
    }
  }

  await compute_resolutions()
  await fetch_users()
  await controller.send(JSON.stringify(select), "analysis")
  return select
}
//--------------------------------------------
function get_input(doc)
{
  const parse_user = user =>
  {
    return user.firstElementChild.nextElementSibling.firstElementChild.firstElementChild
      .textContent.trim()
      .replace(/\s+/g, ' ')
      .replace(/\n+/g,  '')
      .replace('@',     '')
  }

  const input = []
  const list  = doc.querySelectorAll(text_target)
  for (const item of list)
  {
    const parent = item.parentNode.previousElementSibling
    if (!parent)
      continue

    const user = parent.querySelector(name_target)
    if (user)
      input.push({ text: item.textContent, username: parse_user(user) })
  }

  return input
}

module.exports.analyze_tweets = create_analysis
module.exports.get_input      = get_input
