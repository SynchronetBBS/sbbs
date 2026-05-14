# Boolean Text Search

Synchronet's text-search prompts (message scan, mail scan, file search, and the
less-style file pager) accept a small boolean query language: combine search
terms with AND / OR / NOT operators, group them with parentheses, and quote
terms to require a whole-word match. The language is compatible with the
syntax that PCBoard and Wildcat! used for the same purpose, with a few
deliberate extensions.

## Where it works

The boolean parser is used at every "Text to search for" prompt. In the
stock command shell (`exec/default.js`):

| Where                              | Trigger                                                              |
| ---------------------------------- | -------------------------------------------------------------------- |
| Message base scan (main menu)      | `F` — "Find Text in Messages" (`/F` to scan all sub-boards)          |
| Reading messages on a sub          | `F` — "Find text" re-prompt inside the read loop                     |
| Private mail (`Reading E-mail`)    | `/` (slash) at the mail-read prompt                                  |
| File listings (file menu)          | `F` — "Find Text in File Descriptions" (`/F` for all dirs)           |
| File pager (`P_SEEK`, less-style)  | `/` (slash) while viewing a file, bulletin, or log — `n` for next   |

Note: the file menu's `S` ("Search for Filename(s)") is a separate
wildcard-pattern filename match (`*.zip`, `WILD*.EXE`, etc.) and does not
use the boolean parser. Sysops with custom shells may bind these commands
differently — the parser engages whenever `SCAN_FIND` (messages) or `FL_FIND`
(files) reaches the underlying scan function.

Programmatic callers (e.g. `bbs.scan_posts(sub, mode, find)` in JavaScript)
inherit the same syntax — the `find` string they pass is parsed identically.

## Quick reference

| You type                       | Matches when…                                                              |
| ------------------------------ | -------------------------------------------------------------------------- |
| `monitor`                      | the haystack contains `monitor` (case-insensitive substring)               |
| `VGA monitor`                  | the haystack contains the literal phrase `VGA monitor`                     |
| `text & edit`                  | both `text` *and* `edit` appear (in any order)                             |
| `text and edit`                | same as above (`AND` keyword form)                                         |
| `hard disk \| hard drive`      | either `hard disk` *or* `hard drive` appears                               |
| `hard disk or hard drive`      | same as above (`OR` keyword form)                                          |
| `! 320x200`                    | the haystack does **not** contain `320x200`                                |
| `not 320x200`                  | same as above (`NOT` keyword form)                                         |
| `1024x768 &! swim`             | contains `1024x768` and not `swim`                                         |
| `(windows \| dos) & modem !os/2` | (`windows` or `dos`) and `modem`, but not `os/2`                         |
| `"TEST"`                       | the **word** `TEST` (won't match `TESTING` or `BACKTEST`)                  |
| `"SMITH & JONES"`              | the literal phrase `SMITH & JONES`, including the `&`                      |

## Syntax

A query is a boolean expression of search terms, operators, and groups.

### Search terms

A bare term is any run of characters that doesn't include an operator
character (`&`, `|`, `!`, `(`, `)`, `"`). Embedded whitespace is part of the
term — it does **not** mean implicit AND. So `VGA monitor` is one phrase that
matches whenever `VGA` appears immediately followed by ` monitor`.

```
TEST            -> contains 'TEST' anywhere (substring, case-insensitive)
VGA monitor     -> contains the phrase 'VGA monitor'
no-such-string  -> contains the literal 'no-such-string' (hyphens are not special)
```

### Operators

| Symbol | Keyword | Meaning                                       |
| ------ | ------- | --------------------------------------------- |
| `&`    | `AND`   | both operands present                         |
| `\|`   | `OR`    | at least one operand present                  |
| `!`    | `NOT`   | operand absent (unary, binds tightest)        |

Keyword forms are case-insensitive and only recognized as operators when
surrounded by whitespace, parens, or operator characters — so `BANDIT` is one
phrase, while `BAND AND IT` is `BAND` AND `IT`.

Precedence: **NOT > AND > OR**. So `A | B & C` means `A OR (B AND C)`. Use
parentheses if you want a different grouping.

You can omit the `&` when it would precede a `!` or `NOT` — the AND is
implied. So `dog !cat` is the same as `dog & !cat` or `dog AND NOT cat`.

### Grouping

Parentheses control evaluation order:

```
(windows | dos) & modem      -- (windows or dos) and modem
windows | dos & modem        -- windows or (dos and modem)
```

### Quoting (whole-word match)

Wrapping a term in double quotes turns on **word-boundary matching**: the
match must be preceded and followed by a non-word character (or the start /
end of the haystack). This is useful when a short search term would otherwise
match inside a longer word.

```
TEST       -> matches  TEST, TESTING, BACKTEST, ...   (substring)
"TEST"     -> matches  TEST, (TEST), 'TEST'.          but NOT TESTING, BACKTEST
```

A "word character" is any letter, digit, or underscore. Everything else
(spaces, punctuation, brackets, etc.) is a boundary.

Quoted phrases preserve any internal whitespace and operator characters as
literal content, with word-boundary checks applied to the outer ends of the
phrase. This is what makes `"SMITH & JONES"` searchable as a literal phrase
including the `&`.

```
"hard disk"   -> matches 'the hard disk here', not 'hard disks' or 'hard diskette'
"new york"    -> matches 'New York City'
"smith & jones" -> matches 'Smith & Jones report'  (the & is literal, not AND)
```

#### Whitespace inside quotes opts out of the boundary check on that side

A leading space inside the quotes disables the word-boundary check on the
left edge; a trailing space disables it on the right edge. Padding both sides
turns a quoted term into a pure substring search.

| You type     | Behavior                                                |
| ------------ | ------------------------------------------------------- |
| `"WORD"`     | whole-word match — both edges bounded                   |
| `" WORD"`    | trailing edge bounded only                              |
| `"WORD "`    | leading edge bounded only                               |
| `" WORD "`   | no boundary check — pure substring match                |

This is what lets you, for instance, search for `"co"` to match the abbreviation
`co.` but not `cocoa` or `coffee`, while `" co "` still matches `co` anywhere
including inside `cocoa`.

## Common idioms

```
hello & world                 -- both present
hello | hi | greetings        -- any of these
"PCBoard" !pirate             -- whole word PCBoard, exclude messages with 'pirate'
( spam | scam ) & ! "yahoo"   -- spam or scam, but not the word 'yahoo'
"SMITH & JONES"               -- literal phrase with an ampersand
foo bar                       -- the phrase 'foo bar' (NOT implicit AND)
foo & bar                     -- the words 'foo' and 'bar' (in any order)
```

## Error handling

If the parser can't understand your query, the BBS prints
`Invalid search expression: <reason>` and returns you to the prompt. Common
reasons:

- `unterminated quoted string` — a `"` opened a quoted phrase but no closing `"` was found
- `expected ')'` — a `(` opened a group but the matching `)` is missing
- `expected search term` — an operator was followed by another operator or end of input
- `unexpected '<char>' at offset N` — stray `)`, or some other syntactically out-of-place character

## Compatibility notes

The dialect is intentionally compatible with PCBoard and Wildcat! conventions
where they agree, and a strict superset elsewhere:

- All published PCBoard and Wildcat! example queries parse and evaluate the
  same way they did in those systems.
- Operator **keywords** (`AND` / `OR` / `NOT`) are accepted in addition to the
  symbolic forms — PCBoard only used the symbols; Wildcat! supported both.
- **Standard precedence** (NOT > AND > OR) is used instead of PCBoard's
  documented strict left-to-right, but the two only differ on hand-crafted
  examples that no published manual used. Add parentheses if you depend on a
  specific grouping.
- An implicit AND is inserted before `!` / `NOT` (as in Wildcat!'s `(windows |
  DOS) & (modem | comm) !OS/2` example). PCBoard required the `&` to be
  explicit; the implicit form is more permissive but never changes the meaning
  of an already-valid PCBoard query.
- PCBoard treated `(text)` (parens around a single word with no operator
  inside) as a search for the literal string `(text)` — including the parens.
  Synchronet always treats `(...)` as grouping, so `(text)` is equivalent to
  `text`. If you want to search for parens literally, quote them: `"(text)"`.

## Notes for module authors

The boolean parser is exposed in C/C++ at `src/sbbs3/boolsrch.h`:

```c
bool_expr_t* bool_expr_compile(const char* query, char** errmsg);
bool_expr_t* bool_expr_compile_ex(const char* query, unsigned flags, char** errmsg);
bool         bool_expr_match(const bool_expr_t*, const char* haystack);
bool         bool_expr_match_fields(const bool_expr_t*,
                                    const char* const* fields, size_t nfields);
void         bool_expr_free(bool_expr_t*);
```

Compile once before iterating documents; reuse the compiled expression across
the full scan, then free it. `bool_expr_match_fields()` ORs each term across
the supplied fields — a term matches the document if it appears in *any*
field. This is how message scans match `from`-or-subject-or-body and file
scans match name-or-description-or-author with a single query.

For a callsite that wants raw substring semantics (no operator interpretation
at all), pass `BOOL_EXPR_LITERAL` as a flag to `bool_expr_compile_ex()` — the
entire input string becomes one phrase term.
