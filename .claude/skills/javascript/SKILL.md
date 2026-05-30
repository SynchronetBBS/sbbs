---
name: javascript
description: Use for Synchronet's JavaScript language and host API — the SpiderMonkey 1.8.5 dialect constraints, the object/class model (MsgBase, FileBase, File, User, system, msg_area/file_area, Socket, MQTT, etc.), how those APIs actually behave (including sharp edges like MsgBase.get_all_msg_headers() lazy fields), writing tests in exec/tests/, and the stock exec/*.js module ecosystem as API references. Trigger when authoring or debugging Synchronet .js/.ssjs/.xjs code, asking "how does the MsgBase/User/FileBase API work", "why is h.to_ext undefined", "what JS can I use in Synchronet", or "is there a stock script that does X". For *running* JS (jsexec flags, invocation, debug builds) see the jsexec skill; for storage-layer SMB repair see smbutils.
---

# Synchronet JavaScript (the language + host API)

This skill is about **writing and reasoning about Synchronet JavaScript** — the
dialect, the host object model, and how the APIs behave. To actually *run* a
script or inline probe (jsexec flags, invocation modes, capturing output,
Windows debug builds), use the **`jsexec`** skill. For low-level SMB
file repair (smbutil/chksmb/fixsmb) use **`smbutils`**.

**Start here:** the wiki's JavaScript hub — https://wiki.synchro.net/custom:javascript
— covers the engine, invocation contexts, the object model, `load()`/`require()`,
and the output/input method tables. The full reference list (object-model docs,
MDN, ECMA-262, libraries) is in the [References](#references) section below.

**Quick pointers:**
- Synchronet JS Object Model (classes/objects/methods/properties):
  https://synchro.net/docs/jsobjs.html (current) ·
  https://nix.synchro.net/jsobjs.html (preliminary, next release) ·
  https://nix.synchro.net/jsexecobjs.html (the subset bound under jsexec)
- Generated API docs: run `jsexec jsdocs.js` (reads in-tree JSDoc) or browse
  `src/sbbs3/js_*.cpp` — the C++ binding files are authoritative.
- Stock modules: `exec/*.js` in the install (grep for `new X(` / `X.method`).

## The dialect: SpiderMonkey 1.8.5 (ES3-ish + a few ES5 bits)

Synchronet embeds **SpiderMonkey 1.8.5** (JavaScript-C 1.8.5, 2011). Write to
roughly ES3 + named-function-expressions.

**Engine version by Synchronet release:** v3.14 → JS 1.5, v3.15 → JS 1.7,
v3.16 → JS 1.8.5. The current in-development release, **v3.22, still uses
SpiderMonkey 1.8.5.** A move to **SpiderMonkey 128** is slated for **v3.30** and
is being developed on the **`next-js` branch** of sbbs.

What this means for what you write *today*:
- The runtime is 1.8.5, so 1.8.5 is the floor — modern syntax that 1.8.5 can't
  parse will fail now.
- But the **stock/included scripts are expected to be forward-compatible with
  SM-128.** So write code that runs on 1.8.5 **and** won't break under SM-128:
  avoid 1.8.5-only quirks (e.g. E4X XML literals, the non-standard `for each…in`,
  expression closures) that SM-128 removes. Stay on the standard ES3/ES5 subset
  that both engines accept.
- Hold off on **large new C/C++ JS bindings** until SM-128 lands (the binding
  layer is what's churning on `next-js`); small script tweaks are fine.

**Not available** (parse/runtime errors in 1.8.5):
- `let` / `const` (use `var`), arrow functions, template literals
- `class`, destructuring, spread/rest, default params, generators
- `Array.prototype.includes/find/findIndex`, `Object.assign`, `String.prototype.repeat`

**Available** (1.8.5 has these): `JSON`, `Array.prototype.forEach/map/filter/
indexOf/reduce`, `Object.keys`, getters/setters, `try/catch/finally`, `e4x`
XML literals (avoid). When in doubt, match the idioms in nearby `exec/*.js`.

## The host object model

The full surface depends on context. Under `jsexec` (no user session) you get
the system/data objects but **not** the session-bound ones (`bbs`, `console`,
`client`) — that distinction is documented in the `jsexec` skill.
The classes themselves behave identically wherever they're bound.

| Group | Objects |
|-------|---------|
| Globals | `system`, `js`, `server`, `client` (session), `conio` (jsexec) |
| Session | `bbs`, `console`, `user` (only in a BBS/login context) |
| Areas | `msg_area`, `file_area`, `xtrn_area` |
| Data | `User`, `MsgBase`, `FileBase`, `File`, `Archive`, `Queue` |
| Network | `Socket`, `ConnectedSocket`, `ListeningSocket`, `MQTT`, `COM`, `HTTPRequest`/`HTTPReply` |
| Crypto | `CryptContext`, `CryptKeyset`, `CryptCert` |
| UI | `uifc` (jsexec full-screen), `console` (terminal, session) |

Host objects serialize correctly with `JSON.stringify` — including nested
sub-objects (`User.stats`, `User.security`, `User.limits`). Don't hand-roll a
`for (k in obj)` dump; `JSON.stringify(obj, null, 2)` is equivalent and shorter.

## Where Synchronet JavaScript runs (invocation contexts)

`.js` files normally live in `exec/`; `.ssjs` files live in the web hierarchy.
Scripts are **not compiled** — they're loaded at execution time, so editing a
`.js` takes effect on its next run (no server recycle needed). **Put modified
copies of stock scripts in `mods/`** so upgrades don't overwrite them (`mods/`
shadows `exec/`).

The same script can be driven from several hosts, and the available globals
differ by host:

| Context | How it's launched | Notes |
|---------|-------------------|-------|
| Terminal Server | timed event, external/door, login/logon/newuser module, sysop `EXEC`; on a command-line prefix the module name with `?` or `*` (e.g. `?newslink`) | full session: `bbs`, `console`, `user`, `client` |
| Web Server | `.ssjs` pages and `*.js` under `web/root/` | generates HTTP responses |
| Services | `exec/*service.js`, configured in `ctrl/services.ini` | all stock services are JS; static or dynamic |
| Mail Server | inbound mail processors, `exec/mailproc_example.js` + `ctrl/mailproc.ini` | |
| jsexec | standalone (CGI, daemon, one-off probe) | no session — see `jsexec` |

From Baja: `exec "?modname"` / `exec "*modname"` / `exec_bin "modname"`.

## load() and require()

`load()` (and the closely-related `require()`, added v3.17) compiles and runs an
external script from within a parent script — most often to pull in constants
and object libraries from `exec/load/` (e.g. `sbbsdefs.js`, `text.js`,
`graphic.js`). Three forms:

```javascript
var result = load('myscript.js', 1, 2, 3);     // run in current scope, args -> argv, returns last expr
var queue  = load(true, 'child.js', 1, 2, 3);   // run in a child thread; returns a bi-directional Queue
var lib    = load({}, 'mylib.js');              // run in a fresh object's scope; lib.foo / lib.CONST
lib.do_thing();
```

Library files meant to be `load({}, …)`-ed end with a bare `this;` statement so
their definitions land in the passed object — this isolates them from the parent
scope (no name collisions). `load()`'s search path is jsexec's `-i` (default
`load`).

### Constants: C vs JS namespaces (USER_UTF8 ≠ UTF8)

C/C++ headers and the JS-side constant files don't share names. Same values,
different prefixes:

| C/C++ (`sbbsdefs.h`, `scfgdefs.h`) | JS (`exec/load/*.js`)                       |
|------------------------------------|---------------------------------------------|
| `UTF8`, `NO_EXASCII`, `ANSI`       | `USER_UTF8`, `USER_NO_EXASCII`, `USER_ANSI` |
| `K_UTF8`, `K_NOCRLF`               | `K_UTF8`, `K_NOCRLF` (same)                 |
| `P_UTF8`, `P_NONE`                 | `P_UTF8`, `P_NONE` (same)                   |

User-related terminal/setting flags get a `USER_` prefix on the JS side to
avoid polluting the JS global namespace (`UTF8` as a bare word would collide
too easily). Input/print mode flags (`K_*`, `P_*`) keep their prefix on both
sides since the prefix is already a namespace.

Constants live in three places, and the load behavior differs:

1. **Host-injected globals** — defined in C++ (`js_global.cpp`), added to
   the global object at runtime init. Always available, no load:
   - `LOG_EMERG`, `LOG_ALERT`, `LOG_CRIT`, `LOG_ERR`/`LOG_ERROR`,
     `LOG_WARNING`, `LOG_NOTICE`, `LOG_INFO`, `LOG_DEBUG`
   - `INVALID_SOCKET`

2. **Class-static properties** — attached to the JS class itself via
   `JS_DefineObject` on the constructor. Always available wherever the
   class is:
   - `FileBase.DETAIL.*`, `FileBase.SORT.*` (`js_filebase.cpp`)
   - `CryptContext.ALGO_*` (and similar) (`js_cryptcon.c`)
   - Reach them like `FileBase.SORT.NAME_AZ`, no load needed

3. **JS-library constants** — defined in `exec/load/*.js` files. Must be
   pulled in with `load()` or `require()`:
   - **`sbbsdefs.js`** — `K_*`, `P_*`, `MSG_*`, `SS_*`, `NODE_*`, etc.
   - **`userdefs.js`** — `USER_*` flags (terminal caps, settings bitfield)
   - **`sockdefs.js`** — `SOCK_STREAM`, `AF_INET`, etc.

```javascript
require("sbbsdefs.js", "K_NOCRLF");        // canonical: name a sentinel
require("userdefs.js", "USER_UTF8");
// or
load("sbbsdefs.js"); load("userdefs.js");
```

Defensive idiom for code that might run without the require (seen throughout
the stock scripts):

```javascript
var supports_utf8 = (typeof(USER_UTF8) != "undefined")
    ? console.term_supports(USER_UTF8) : false;
```

Rule of thumb: if you can grep the JS-side constant in `exec/load/*.js`,
you need to load that file. If it's defined only in C/C++ headers, check
whether the host injected it (LOG_*, INVALID_SOCKET) or attached it to a
class (FileBase.DETAIL, CryptContext.ALGO) — those work without load.

### Terminal capability: runtime vs stored

Two different ways to ask "does this user have UTF-8":

- `console.term_supports(USER_UTF8)` — **runtime**: what the live connection
  actually negotiated. Authoritative at session time.
- `user.settings & USER_UTF8` — **stored**: the user's saved preference
  (set via `user_terminal.js` / `user_info_prompts.js`).

These can disagree (user toggled their pref but the connection still reports
old caps, or they connected from a different terminal). For "what can I
render right now" decisions, use `term_supports()`. For "what does this user
normally want" (e.g. when serializing a message that may be read later), use
`user.settings`.

`console` is only available in a real BBS session — not in `jsexec`, and not
always in early-startup contexts. Guard accordingly.

## MsgBase — the message-base API

`MsgBase` opens local mail (`"mail"`), a sub-board (by internal code, e.g.
`"syncdata.synchronet"`), or an arbitrary SMB by path. Open mail/sub by code, or
pass `is_path=true` to treat the string as a filesystem base path:

```javascript
var mb = new MsgBase("mail");                 // local e-mail
var mb = new MsgBase("general");              // a sub-board by code
var mb = new MsgBase(path, /* is_path: */true); // arbitrary SMB base path (v3.21+, commit 93b4d946c)
if (!mb.open())                               // ALWAYS check — failure is silent otherwise
    throw new Error("open: " + mb.last_error);
```

Core methods: `open`, `close`, `get_all_msg_headers`, `get_msg_header`,
`put_msg_header`, `get_msg_body`, `get_msg_index`, `save_msg`, `remove_msg`,
`vote_msg`, `how_user_voted`. `last_error` carries the reason after any failure.

### get_all_msg_headers([include_votes=false] [, expand_fields=true])

Returns a plain object **keyed by message number** (string keys), each value a
header object. Iterate with `for (var n in hdrs)`. `for..in` yields the keys in
ascending message-number order, so the newest mail is at the **end**.

```javascript
var hdrs = mb.get_all_msg_headers(false, false);
for (var n in hdrs) {
    var h = hdrs[n];
    if (typeof h != "object" || h == null) continue;  // skip any stray props
    // ... use h.number, h.to, h.from, h.subject, h.to_ext, ...
}
```

- `include_votes=true` also returns vote/poll messages (default skips them).
- `expand_fields` controls expansion of a few derived fields (e.g.
  `reverse_path` from the SMTP reverse-path hfield). It does **not** govern the
  lazy-field bug below.

### ⚠️ The lazy-field gotcha (read this before filtering on to_ext/from_ext/etc.)

Header objects from `get_all_msg_headers()` resolve most string fields
**lazily** (a SpiderMonkey resolve hook, `LAZY_STRING_TRUNCSP_NULL` in
`js_msgbase.cpp`). The `*_NULL`-defaulted fields —

> `to_ext`, `from_ext`, `to_list`, `cc_list`, `summary`, `tags`, `from_org`,
> `replyto`, `replyto_ext`, `replyto_list`, `reverse_path`, `forward_path`,
> `editor`, `mime_version`, `content_type`, `ftn_msgid`/`ftn_*`, etc.

— return **`undefined` on the FIRST property access of a header object** if that
`*_NULL` field is the first thing you touch. Touching **any** non-NULL field
first (`number`, `attr`, `to`, `from`, `subject`, `when_imported`, …) "primes"
the object and then every field — including the `*_NULL` ones — reads correctly.

**Always read a non-NULL field before any `*_NULL` field.** Filtering a mailbox
by recipient is the classic trap, because `to_ext` is naturally the first thing
you reach for:

```javascript
// WRONG — to_ext is the first access; returns undefined on real bases:
for (var n in hdrs) { if (hdrs[n].to_ext == 1) count++; }      // counts ~0

// RIGHT — touch a non-NULL field first, then to_ext resolves:
for (var n in hdrs) {
    var h = hdrs[n];
    var to = h.to;                 // prime (non-NULL field)
    if (h.to_ext == 1) count++;    // now correct
}
```

Notes from hard experience:
- The failure is **silent**: you get `undefined` (or, in older builds, a value
  from another header), not an error. A recipient filter just quietly matches
  nothing.
- It is **data-dependent, not just scale-dependent**. On synthetic bases it
  doesn't reproduce (verified clean: uniform, varying, NULL-interleaved, net-type
  and header-rich messages, up to **15000** msgs — the same scale as the live
  base — with/without forced GC). On a real
  mail base it bites almost every header: verified on a live ~14.4k-message base,
  cold first-access of `to_ext` returned `undefined` for ~14.26k of 14.43k
  headers, while priming with `.to` first returned the correct value for all
  ~9.1k messages actually addressed to the user. The breakage starts at a clear
  **transition point** — cold reads are correct until the first header with a
  NULL/empty `to_ext` (a non-net/local message amid net mail) appears, after
  which *every* subsequent header reads `undefined` cold for the rest of the
  enumeration. A minimal self-contained reproducer hasn't been pinned — so you
  cannot rely on a quick interactive test to reveal it.
- The underlying cause is a **SpiderMonkey 1.8.5 engine bug** sitting between
  the JS engine and the `*_NULL`-defaulted lazy-resolve hook in
  `js_msgbase.cpp`. An updated root-cause diagnosis is being tracked in
  **GitLab issue #1143**; treat the precise mechanism as unsettled and the
  named "fix" commits in that issue as **not yet definitive**. The
  touch-non-NULL-first rule is the dependable workaround regardless of which
  build/commit you're on — **treat it as mandatory**.
- **Element-access escape hatch**: empirically, `hdr["to_ext"]` (the bracket /
  element-access form) reads correctly even on a cold header where
  `hdr.to_ext` (dot / property-access form) returns `undefined`. If you can't
  reorder accesses (e.g. you're filtering inside a library you don't own), the
  `hdr["FIELD"]` form is an option. Still prefer touch-non-NULL-first when
  you control the code — it's the more obvious idiom.
- `get_msg_header()` (single fetch) doesn't exhibit it in practice, because
  callers organically touch a non-NULL field next. The bug is specific to
  bulk-fetched headers whose first touched property is a `*_NULL` field.

### Field types & values

- `to_ext`/`from_ext` are recipient/sender **user numbers as strings** — the
  sysop is `"1"`. They are clean numeric strings (`"1"` is one char, code 0x31);
  `h.to_ext == 1` is a valid loose comparison **once the field is primed** (see
  above). If a comparison against `1` mysteriously fails, suspect the lazy-field
  gotcha, not a stray byte in the data.
- `to`/`from`/`subject` are always present (non-NULL lazy fields), so they're
  safe to read first and make ideal "primer" accesses.
- Timestamps (`when_written_time`, `when_imported_time`) are Unix time; format
  with `system.timestr(t)`.

### Deleting messages

`remove_msg([by_offset=false,] number_or_offset | id_string)` sets the
`MSG_DELETE` attribute and updates the header — it **flags**, it does not
physically purge. Flagged messages are **recoverable with `fixsmb -undelete`**
until the base is next *packed* (the mail base auto-packs during nightly
maintenance, which makes deletions permanent).

```javascript
// Flag every message matching some predicate (selective delete):
for (var n in hdrs) {
    var h = hdrs[n];
    var to = h.to;                                  // prime before to_ext
    if (h.to_ext == 1 && /ERROR occurred/.test(h.subject || ""))
        mb.remove_msg(false, Number(n));            // by message number
}
```

Note `remove_msg` works off the header/index — you do **not** need to load
message bodies to delete. Read bodies (`get_msg_body`) only when your delete
criterion depends on body content (e.g. distinguishing two errors that share a
subject).

`smbutil`'s `d`/`D` flag/delete **all** messages — for *selective* deletion a
MsgBase script is the right tool (see `smbutils` for the storage
layer and when to prefer each).

## Reading INI files (and the trailing-comment trap)

`File.iniGetValue(section, key, default)` and `File.iniGetObject(section)` read
Synchronet `.ini` config (`js_file.cpp` → `xpdev/ini_file.c`). The parser
recognizes `;` as the comment char (`INI_COMMENT_CHAR`), but **whether a trailing
`; comment` on a value line is stripped depends on the value's TYPE** — this is
the single most common config-corruption surprise:

| Value type | Trailing `key = value ; comment` | Why |
|------------|----------------------------------|-----|
| **string** | **NOT stripped** — the comment becomes part of the value | `read_value`/`get_value` take everything after `=`, apply only `truncsp` (trim trailing whitespace); a string can legitimately contain `;` and spaces, so nothing is cut |
| **boolean** | stripped/ignored | `isTrue()` truncates the value at the first `;`, space, or tab before matching `true`/`yes`/`on` |
| **enum** | stripped/ignored | `parseEnum()` keeps only the first whitespace-delimited word |
| **integer / float / datetime** | stripped/ignored | the numeric parse stops at the first non-numeric char |

The asymmetry is deliberate. A single-token value (bool/enum/number) can't
contain spaces, so anything after the token is safely a comment — support was
added on purpose: `isTrue` (`cceb1fbb8`, after FozzTexx reported the wiki's
`sexpots.ini` had `true ; comment` values parsing as false) and `parseEnum`
(`7346893d6`, "Enum values followed by comments are now supported"). A **string**
value has no such delimiter, so the parser can't guess where the value ends and
a comment begins — it keeps the whole thing.

Consequences for JS callers:

- `File.iniGetObject()` returns **every** value as a raw string, so it **never**
  strips inline comments. `iniGetValue(s, k, dflt)` strips only when `dflt`'s
  type routes it through the bool/number/enum path (`iniGetBool`/`iniGetInteger`/
  etc.); a string default goes through `iniGetString` and keeps the comment.
- **Don't put an inline `; comment` after a string-valued key** (URLs, paths,
  filenames, names) — put the comment on its own line above the key. A line like
  `endpoint = http://host:11434/api/chat   ; my note` yields the literal value
  `http://host:11434/api/chat   ; my note`, which then fails wherever it's used.
- Inline comments after bool/enum/numeric keys are fine (and appear throughout
  stock `.ini` files) — that's exactly what the type-specific stripping is for.

### The root (unnamed) section = global defaults

Keys that appear **before any `[section]` tag** live in the **root (unnamed)
section**. `File.iniGetObject(null)` reads it (and `iniSetObject(null, obj)`
writes it — see `exec/upgrade_to_v320.js`). The standard Synchronet idiom is to
put **global/default key-values in the root**, then let each named `[section]`
**override** them — read the root for the defaults, read the section, merge the
section on top:

```javascript
var f = new File(system.ctrl_dir + 'foo.ini'); f.open('r');
var defaults = f.iniGetObject(null)       || {};   // root: shared defaults
var section  = f.iniGetObject('guru:irc') || {};   // per-thing overrides
f.close();
var cfg = {}; for (var k in defaults) cfg[k] = defaults[k];
for (k in section) cfg[k] = section[k];            // section wins
```

Prefer the root over a *named* `[default]` section for defaults: it's what the
docs describe (https://wiki.synchro.net/config:ini_files#root_section) and it
avoids overloading a section name — a literal `[default]` section collides with
any value that's *also* used as a section key elsewhere (e.g. a sysop-configured
code that happens to be "default"). Section names are case-insensitive, so a
value used as a section key should be case-folded before it lands in a filename
to avoid case-variant dupes on case-insensitive filesystems.

## Common API recipes

```javascript
// Inspect a user (JSON dump of the whole object, nested sub-objects included):
print(JSON.stringify(new User(1), null, 2));

// Field names only:
var u = new User(1); for (var k in u) print(k);

// Load a definitions module, then use its constants:
load("sbbsdefs.js"); print(USER_DELETED.toString(16));
```

## Output & input: choosing the right function

Two families, and the difference matters:
- **Global** `write()`, `writeln()` (aka `print()`), `printf()`, `write_raw()`,
  `alert()`, `log()` — work under **both** the Terminal Server and **jsexec**.
  Most translate Ctrl-A codes (`write_raw()`/`putbyte` don't). When no user is
  online, `write()`→`log(LOG_INFO)` and `alert()`→`log(LOG_WARNING)`.
- **`console.*`** (`console.print`, `console.putmsg`, `console.center`,
  `console.getstr`, `console.getkey`, `console.yesno`, …) — Terminal Server
  **only** (a user session); **not** bound under jsexec. `console.print`/`putmsg`
  also expand **@-codes** (and `putmsg` honors menu attributes); `getstr`/etc.
  do line-counting and prompting a bare `read()`/`readln()` won't.

So in a jsexec probe or service test, use `print()`/`writeln()`/`log()` — never
`console.*` (it throws `ReferenceError` off-session). In a Terminal Server
module that displays menu/text files, use `console.putmsg()`.

**Carriage returns / PETSCII:** don't emit a raw `\r` to move the cursor to
column 0 — use `console.creturn()` or the Ctrl-A `[` sequence. On PETSCII
terminals an ASCII 13 performs a full newline (`\r\n`), so a raw `\r` breaks
cursor positioning.

## The stock exec/*.js ecosystem (API by example)

~100 stock `.js` modules ship in `exec/`; reading them is usually the fastest
way to learn an API, and many double as ready-made tools. Grep for `new X(` or
`X.method` to find a usage example before writing your own.

**MsgBase / FidoNet:** `msgutil.js` (swiss-army MsgBase tool), `postmsg.js`,
`sendmsg.js`, `scrubmsgs.js`, `delmsgs.js`, `dupefind.js`, `ftnmsgdump.js`,
`binkit.js` (also a `Socket` reference), `echoareas.js`.
**FileBase:** `addfiles.js`, `delfiles.js`, `filelist.js`, `fileman.js`,
`purgefiles.js`, `rehashfiles.js`, `hashfile.js`.
**Users:** `allusers.js` (user-walk template), `badpasswords.js`,
`inactive_user_email.js`, `last10logins.js`, `makeuser.js`.
**Config:** `exportcfg.js` / `importcfg.js` (SCFG ↔ JSON), `make_areas_ini.js`.
**Net/mail/IRC:** `wget.js` (HTTPRequest reference), `letsyncrypt.js`,
`certtool.js`, `sendmail.js`, `ircbot.js`, `mqtt_pub.js`/`mqtt_sub.js`.
**LLM chat / Guru:** `chat_llm.js` (LLM-backed Guru chat engine; entry
`chat_session(input, ctx)` / `open_session(ctx)`, driven by a Guru's SCFG
`Module` field), `chat_llm_irc.js` (IRC bot adapter), `llm_tools.js` +
`llm_tools/*.js` (drop-in function-calling tools), `llm_index.js` +
`llm_index/*.js` (BM25 RAG indexer). See the wiki links below.
**Inspection:** `dumpini.js`, `hexdump.js`, `sauce.js`, `jsdocs.js`,
`syncjslint.js`, `sockinfo.js`.
**Services** (also runnable standalone for testing): `nntpservice.js`,
`imapservice.js`, `gopherservice.js`, `websocketservice.js`, `json-service.js`.

```bash
# All stock modules that have no bbs./console. deps (jsexec-friendly):
cd "<sbbs>/exec" && for f in *.js; do grep -qE '\b(bbs|console)\.' "$f" || echo "$f"; done
```

## Writing tests in exec/tests/

Synchronet ships a tiny harness at `exec/tests/test.js` (jsexec-only; no
separate runner). Layout: `exec/tests/<category>/<name>.js`; `test.js` walks the
tree depth-first and runs every `.js`.

- **Pass** = runs to completion without throwing. **Fail** = throws,
  `exit(nonzero)`, or returns an `instanceof Error`. No `assert()` framework —
  just `throw new Error("descriptive message")`.
- **`skipif`** file in a category dir is `load()`-ed; truthy result skips the
  **whole category**. E.g. `exec/tests/msgbase/skipif` → `typeof MsgBase ===
  'undefined'`.
- No setup/teardown hooks — each file does its own; wrap scratch state in
  try/finally.

Temp-msgbase test pattern (so you never touch the install's `mail`):

```javascript
var path = system.temp_dir + "test_foo_" + Date.now();
var EXT = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];
function cleanup() { EXT.forEach(function (e) { try { file_remove(path + e); } catch (x) {} }); }
try {
    var mb = new MsgBase(path, true);
    if (!mb.open()) throw new Error("open: " + mb.last_error);
    if (!mb.save_msg({ to:"x", to_ext:"1", from:"y", subject:"z" }, "body"))
        throw new Error("save_msg: " + mb.last_error);
    // ... verify ...
    mb.close();
} finally { cleanup(); }
```

> **Caveat for regression tests of the lazy-field gotcha:** a single-message (or
> all-uniform, or purely synthetic) base does **not** reproduce it — that's
> exactly why `ca448cb8b`'s test passes while the bug persists on real bases. A
> meaningful regression test must reproduce the real-base conditions (it remains
> an open item to pin a minimal synthetic reproducer); until then, treat the
> "touch a non-NULL field first" rule as the reliable mitigation.

### Syntax-checking / testing a module that acts at load time

Some modules *do work* the moment they're loaded — start a server loop,
connect a socket, emit a greeting (e.g. `chat_llm_irc.js` connects to IRC
and runs its main loop at the bottom of the file). You can't just
`load()` such a file to syntax-check it or exercise its helpers, because
`load()` runs it — connecting a second bot / launching the loop.

The idiom: gate the entry point behind a sentinel the loader can set, then
`load()` with the sentinel set. `load()` compiles the **whole** file (so a
real syntax error throws) but skips the side effect:

```javascript
/* bottom of the module: */
if (typeof MYMOD_NO_MAIN == 'undefined') {
    main_loop();              // skipped when loaded for testing
}

/* from a jsexec probe: */
var MYMOD_NO_MAIN = true;
load('mymod.js');             // compiles + defines functions; no main loop
print(typeof some_helper);    // unit-test the helpers directly
```

Stock examples: `chat_llm.js`'s `CHAT_LLM_NO_STANDALONE`, `chat_llm_irc.js`'s
`CHAT_LLM_IRC_NO_MAIN`. This guard-and-`load()` is the reliable way to
syntax-check a side-effecting module without running it.

`exec/syncjslint.js` is a **style** linter (jslint), **not** a syntax gate:
it emits false positives on valid SpiderMonkey constructs (e.g. a `-` inside
a regex character class → "Unexpected '-'") and halts on them. Use it for
style review, not as a compile / pass-fail check.

`system.temp_dir + name + Date.now()` is unique per run; the seven extensions
above are the full set an SMB can produce.

## References

The authoritative resources, as gathered on and linked from the wiki's
JavaScript hub (https://wiki.synchro.net/custom:javascript):

**Synchronet JavaScript:**
- JavaScript hub / overview — https://wiki.synchro.net/custom:javascript
- Synchronet JS Object Model (current release) — https://synchro.net/docs/jsobjs.html
- Synchronet JS Object Model (preliminary, next release) — https://nix.synchro.net/jsobjs.html
- JS objects bound under jsexec — https://nix.synchro.net/jsexecobjs.html
- JavaScript Libraries index — https://wiki.synchro.net/custom:javascript:lib:index
- Customization index — https://wiki.synchro.net/custom:index
- jsexec utility — https://wiki.synchro.net/util:jsexec
- Baja (legacy scripting language) — https://synchro.net/docs/baja.html · https://wiki.synchro.net/util:baja
- Ctrl-A codes — https://wiki.synchro.net/custom:ctrl-a_codes · @-codes — https://wiki.synchro.net/custom:atcodes · colors/attributes — https://wiki.synchro.net/custom:colors
- Mail processor example/config — https://wiki.synchro.net/module:mailproc_example · https://wiki.synchro.net/config:mailproc.ini · services — https://wiki.synchro.net/config:services.ini
- LLM-backed Guru — engine https://wiki.synchro.net/module:chat_llm · config https://wiki.synchro.net/config:chat_llm.ini · tools https://wiki.synchro.net/module:llm_tools · RAG index https://wiki.synchro.net/module:llm_index · IRC adapter https://wiki.synchro.net/module:chat_llm_irc · how-to https://wiki.synchro.net/howto:llm-guru

**Core JavaScript language (the engine speaks this):**
- MDN JavaScript Guide — https://developer.mozilla.org/en-US/docs/Web/JavaScript
- MDN JavaScript Reference — https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference
- MDN SpiderMonkey — https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey
- ECMA-262 5.1 (June 2011, the ed. matching SM 1.8.5) — https://ecma-international.org/wp-content/uploads/ECMA-262_5.1_edition_june_2011.pdf
- Historic Core JavaScript 1.5 Guide/Reference — http://developer.mozilla.org/docs/Core_JavaScript_1.5_Guide · .../Core_JavaScript_1.5_Reference

To update any of these wiki pages, see the `synchronet-wiki` skill.

## Cross-references

- **Run JS / jsexec flags / Windows debug builds** → `jsexec`.
- **SMB storage-layer repair (smbutil/chksmb/fixsmb)** → `smbutils`.
- **Build sbbs.dll / a debug binary** → `synchronet-build`.
- **Display/menu files, @-codes, Ctrl-A** → `menus`.
