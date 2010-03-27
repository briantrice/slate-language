-- Copyright 2010 Alex Suraci
-- Slate LPeg Lexer

module(..., package.seeall)
local P, R, S = lpeg.P, lpeg.R, lpeg.S

local ws = token('whitespace', space^1)

-- comments
local comment = token('comment', delimited_range('"', '\\'))

-- strings
local string = token('string', delimited_range("'", '\\'))

-- chars
local char = token('char', '$' * (('\\' * P(1)) + P(1)))

-- numbers
local number = token('number', float + integer)

-- operators
local op = punct - S('()"\'') + S('\\[]]')
local operator = token('operator', op)

-- identifiers
local word = (alnum + S("._#"))^1
local identifier = token('identifier', word + (':' * word))

-- symbols
local symbol = token('symbol', '#' * word)

-- keywords
local keyword = token('slate_keyword', word * ':')

-- optional keywords
local optional_keyword = token('slate_optional_keyword', '&' * word * ':')

-- constructors
local constructor = token('type', upper * word)

function LoadTokens()
  local slate = slate
  add_token(slate, 'whitespace', ws)
  add_token(slate, 'symbol', symbol)
  add_token(slate, 'slate_keyword', keyword)
  add_token(slate, 'slate_optional_keyword', optional_keyword)
  add_token(slate, 'type', constructor)
  add_token(slate, 'identifier', identifier)
  add_token(slate, 'string', string)
  add_token(slate, 'char', char)
  add_token(slate, 'comment', comment)
  add_token(slate, 'number', number)
  add_token(slate, 'operator', operator)
  add_token(slate, 'any_char', any_char)
end

function LoadStyles()
  add_style('symbol', style_symbol)
  add_style('slate_keyword', style_slate_keyword)
  add_style('slate_optional_keyword', style_slate_keyword)
end
