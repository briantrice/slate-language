" Slate syntax file
" Language:	Slate

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

" some Smalltalk keywords and standard methods
syn keyword	slateOddballs	True False Nil
syn match	slateControl	"\<resend\>"
syn match	slateControl	"\^"
syn match	slateControl	":="
syn match	slateControl	"=:="
syn match	slateControl	"@"
syn match	slateIgnore	"_"
syn match	slateArgument	":"
syn match	slateStatement	"\."

" the block of local variables of a method
syn region slateLocalVariables	start="|" end="|" contains=slateInputVariable

" the Smalltalk comment
syn region slateComment	start="\"" skip="\\\"" end="\"" contains=slateEscape

" the Smalltalk strings and single characters
syn region slateString	start='\'' skip="\\'" end='\'' contains=slateEscape
syn match  slateEscape     contained "\\."
syn match  slateCharacter	"$."
syn match  slateCharacter  "$\\." contains=slateEscape

syn case ignore

syn match slateInputVariable       contained "\(:\<[a-z_][a-z0-9_]*\>\)"

" the symbols prefixed by a '#'
" syn match  slateSymbol	"\(#\<[a-z_][a-z0-9_:]*\>\)" contains=slateArgument
syn match  slateSymbol	"\(#\<[^'	 ]*\>\)" contains=slateArgument
syn match  slateSymbol	"\(#'[^']*'\)" contains=slateArgument

" some representations of numbers
syn match  slateNumber	"\<\d\+\(u\=l\=\|lu\|f\)\>"
syn match  slateFloat	"\<\d\+\.\d*\(e[-+]\=\d\+\)\=[fl]\=\>"
syn match  slateFloat	"\<\d\+e[-+]\=\d\+[fl]\=\>"

syn case match

" a try to highlight paren mismatches
syn region slateParen	transparent matchgroup=slateParenChars start='(' matchgroup=slateParenChars end=')' matchgroup=NONE contains=ALLBUT,slateParenError
syn match  slateParenError	")"
syn region slateBlock	transparent matchgroup=slateBlockChars start='\[' matchgroup=slateBlockChars end='\]' matchgroup=NONE contains=ALLBUT,slateBlockError
syn match  slateBlockError	"\]"
syn region slateSet	transparent matchgroup=slateSetChars start='{' matchgroup=slateSetChars end='}' matchgroup=NONE contains=ALLBUT,slateSetError
syn match  slateSetError	"}"

hi link slateParenError slateError
hi link slateSetError slateError
hi link slateBlockError slateError

" synchronization for syntax analysis
syn sync minlines=50

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_slate_syntax_inits")
  if version < 508
    let did_slate_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink slateStatement		PreProc
  HiLink slateIgnore		PreProc
  HiLink slateOddballs		Type
  HiLink slateControl		Label
  HiLink slateArgument		Label
  HiLink slateComment		Comment
  HiLink slateCharacter		Constant
  HiLink slateString		Constant
  HiLink slateEscape		Special
  HiLink slateSymbol		Constant
  HiLink slateSetChars		Constant
  HiLink slateParenChars	Delimiter
  HiLink slateBlockChars	Delimiter
  HiLink slateNumber		Type
  HiLink slateFloat		Type
  HiLink slateError		Error
  HiLink slateLocalVariables	PreProc
  HiLink slateInputVariable	Label

  delcommand HiLink
endif

let b:current_syntax = "slate"
