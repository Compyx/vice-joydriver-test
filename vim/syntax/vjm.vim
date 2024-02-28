" VIM syntax file for VICE joymap 2.0 files
" Author:   Bas Wassink <b.wassink@ziggo.nl>

if exists("b:current_syntax")
    finish
endif

syn keyword vjmTodo         TODO FIXME XXX BUG

syn match vjmKeyword        "\<vjm-version\>"
syn match vjmKeyword        "\<device-\(name\|product\|vendor\|version\)\>"
syn keyword vjmKeyword      map
syn keyword vjmInputType    action axis button hat key pin pot
syn keyword vjmDirection    north northeast east southeast south southwest west northwest negative positive

syn match vjmComment        "[#;].*$" contains=vjmTodo

syn match vjmNumber         '\d\+'
syn match vjmNumber         '\(0[bB]\|[%]\)[01]\+'
syn match vjmNumber         '\(0[xX]\|[$]\)\x\+'

syn region vjmString        oneline start='"' end='"'

let b:current_syntax = "vjm"

hi link vjmTodo         Todo
hi link vjmComment      Comment
hi link vjmKeyword      Keyword
hi link vjmInputType    Type
hi link vjmNumber       Constant
hi link vjmString       Constant
hi link vjmDirection    Special
