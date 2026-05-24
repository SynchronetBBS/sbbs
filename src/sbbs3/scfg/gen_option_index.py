#!/usr/bin/env python3
"""
gen_option_index.py - Generate scfgindex.h from src/sbbs3/scfg/*.c

Walks the SCFG source files and extracts every menu-option label that the
sysop sees, along with the FULL menu path the sysop must navigate to reach
that option (e.g.  "Configure > External Programs > Online Programs > Online
Program > Internal Code"). Used by SCFG's "Search Settings" feature so a
sysop can type a substring and find the menu path containing the option.

Indexing strategy (in two passes over the source files):

Pass 1 - identify every function body delimited by `<rettype> name(...) {`
through the matching `}` at column 0. Inside each body, collect:
    * The titles of every `uifc.list(..., "Title", opt)` call, in order.
    * Each `name(args)` call to another function (so we can build a
      caller -> callee graph).
    * Every `snprintf(opt[...], ..., "Label", ...)` /
      `strcpy(opt[...], "Label")` literal label.

Pass 2 - resolve menu paths. For each option, the path is the chain of
menu titles from the root "Configure" menu (the title of the uifc.list
inside main()) down through the function the option lives in, ending with
the option label. The chain is computed by walking the callee->caller
graph backward.

The generated header is checked into the source tree so end-users do not
need Python to build SCFG. Re-run this script whenever option labels,
menu titles, or function call relationships change.
"""
from __future__ import annotations

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.join(HERE, "scfgindex.h")

SCFG_SOURCES = sorted(
    fn for fn in os.listdir(HERE)
    if fn.endswith(".c") and fn.startswith("scfg")
    and fn not in ("scfgsrch.c",)
)

STRLIT = r'"(?:[^"\\]|\\.)*"'

# ---------------------------------------------------------------------------
# Low-level lexing helpers
# ---------------------------------------------------------------------------

def _decode(s: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(s):
        c = s[i]
        if c != "\\":
            out.append(c); i += 1; continue
        if i + 1 >= len(s):
            out.append(c); i += 1; continue
        nxt = s[i + 1]
        if nxt == "n":   out.append("\n"); i += 2
        elif nxt == "t": out.append("\t"); i += 2
        elif nxt == "r": out.append("\r"); i += 2
        elif nxt == '"': out.append('"');  i += 2
        elif nxt == "\\":out.append("\\"); i += 2
        else:            out.append(c); out.append(nxt); i += 2
    return "".join(out)


def _skip_strings_comments(text: str, i: int):
    """If text[i] starts a string/char literal or comment, return the index
    just past it. Otherwise return None."""
    n = len(text)
    if i + 1 < n and text[i] == "/" and text[i + 1] == "*":
        j = text.find("*/", i + 2)
        return n if j < 0 else j + 2
    if i + 1 < n and text[i] == "/" and text[i + 1] == "/":
        j = text.find("\n", i + 2)
        return n if j < 0 else j + 1
    if text[i] == '"':
        j = i + 1
        while j < n:
            if text[j] == "\\" and j + 1 < n:
                j += 2; continue
            if text[j] == '"':
                return j + 1
            j += 1
        return n
    if text[i] == "'":
        j = i + 1
        while j < n:
            if text[j] == "\\" and j + 1 < n:
                j += 2; continue
            if text[j] == "'":
                return j + 1
            j += 1
        return n
    return None


def _split_top_level_args(s: str) -> list[str]:
    args = []
    depth = 0
    cur = []
    i = 0
    n = len(s)
    while i < n:
        skip = _skip_strings_comments(s, i)
        if skip is not None:
            cur.append(s[i:skip])
            i = skip
            continue
        c = s[i]
        if c in "([{":
            depth += 1; cur.append(c); i += 1; continue
        if c in ")]}":
            depth -= 1; cur.append(c); i += 1; continue
        if c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c); i += 1
    if cur:
        args.append("".join(cur).strip())
    return args


def _balanced_call(text: str, start: int):
    """text[start] is '(' (or whatever the opening bracket is). Return
    (end_index, inner_text)."""
    if text[start] != "(":
        return None
    depth = 0
    i = start
    n = len(text)
    while i < n:
        skip = _skip_strings_comments(text, i)
        if skip is not None:
            i = skip
            continue
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i, text[start + 1:i]
        i += 1
    return None


def _find_brace_end(text: str, start: int) -> int:
    """text[start] is '{'. Return offset of matching '}'."""
    depth = 0
    i = start
    n = len(text)
    while i < n:
        skip = _skip_strings_comments(text, i)
        if skip is not None:
            i = skip
            continue
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n


# ---------------------------------------------------------------------------
# Function body discovery
# ---------------------------------------------------------------------------

PAT_FUNC_DEF = re.compile(
    r'^(?:static\s+)?'
    r'(?:void|bool|int|uint(?:8|16|32|64)?_t|size_t|char\s*\*)\s+'
    r'([a-z_][A-Za-z0-9_]*)\s*\(',
    re.MULTILINE
)


def find_functions(text: str):
    """Yield (name, body_start, body_end) for each function definition that
    has its body in this file."""
    n = len(text)
    for m in PAT_FUNC_DEF.finditer(text):
        # Find the call's opening paren end.
        i = text.find("(", m.start(1))
        if i < 0:
            continue
        bal = _balanced_call(text, i)
        if not bal:
            continue
        j = bal[0] + 1
        # Skip whitespace, comments, attributes until '{' or ';'.
        while j < n:
            skip = _skip_strings_comments(text, j)
            if skip is not None:
                j = skip; continue
            c = text[j]
            if c in " \t\r\n":
                j += 1; continue
            break
        if j >= n or text[j] != "{":
            continue  # forward declaration or function-pointer init
        body_end = _find_brace_end(text, j)
        yield m.group(1), j + 1, body_end


# ---------------------------------------------------------------------------
# Per-function inspection
# ---------------------------------------------------------------------------

PAT_SNPRINTF_OPT = re.compile(r'\b(?:snprintf|sprintf|SAFEPRINTF\d?)\s*\(\s*opt\s*\[')
PAT_STRCPY_OPT   = re.compile(r'\b(?:strcpy|SAFECOPY)\s*\(\s*opt\s*\[')
PAT_UIFC_LIST    = re.compile(r'\buifc\.list\s*\(')
PAT_VAR_FMT      = re.compile(r'\b(?:snprintf|sprintf|SAFEPRINTF\d?)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*[,)]')
PAT_VAR_CPY      = re.compile(r'\b(?:strcpy|SAFECOPY)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,')
PAT_CALL         = re.compile(r'\b([a-z_][A-Za-z0-9_]*)\s*\(')

# Static option array declared inside a function body, e.g.
#   char* mopt[] = { "Nodes", "System", ..., NULL };
# Used by main()'s top-level uifc.list call where the options are not built
# via snprintf(opt[..]) but supplied as a literal array.
PAT_STATIC_OPT_ARR = re.compile(
    r'\bchar\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*\]\s*=\s*\{')

# `case N:` switch label. Used to map a function call inside a switch to the
# index in the parent's option array - i.e. the option label the user clicked
# to trigger the call.
PAT_CASE_NUM = re.compile(r'\bcase\s+(\d+)\s*:')

# C keywords that look like function calls but aren't. Don't advance past
# their parenthesized expression; let the parser see the calls inside.
C_KEYWORDS_LIKE_CALL = {
    "if", "while", "for", "switch", "return", "sizeof", "do",
    "case", "static", "void", "int", "char", "long", "short", "unsigned",
    "signed", "const", "volatile", "register", "auto", "extern",
    "typedef", "struct", "union", "enum", "goto", "break", "continue",
    "typeof",
}

FMT_SPEC = re.compile(r'%[-+ 0#]?[0-9*]*(?:\.[0-9*]+)?[hljztL]*[diouxXeEfgGaAcspn%]')

def stable_portion(fmt: str) -> str:
    s = FMT_SPEC.sub("", fmt)
    return re.sub(r"\s+", " ", s).strip()


def normalize_title(s: str) -> str:
    """Clean a menu title for display: strip trailing column-header padding
    (everything past the first run of 3+ spaces), collapse whitespace, and
    drop empty trailing parens left over by `"%s Foo"` formats with empty
    runtime values."""
    if s is None:
        return s
    parts = re.split(r"\s{3,}", s)
    out = parts[0].strip()
    out = re.sub(r"\s*\(\s*\)\s*$", "", out)
    return out


# Pattern to locate `uifc.helpbuf = "..."` and capture the helpbuf's first
# line (between backticks), used as a fallback menu name when uifc.list has
# a dynamic title.
PAT_HELPBUF = re.compile(r'\buifc\.helpbuf\s*=\s*("(?:[^"\\]|\\.)*")')

def helpbuf_title(literal: str) -> str | None:
    """Decode a helpbuf C string literal and pull out its title - the text
    between the leading backticks of its first line. Returns None if no
    title pattern is found."""
    if not literal or not literal.startswith('"'):
        return None
    body = _decode(literal[1:-1])
    first = body.split("\n", 1)[0].strip()
    m = re.match(r"`([^`]+):?`", first)
    if not m:
        return None
    title = m.group(1).strip().rstrip(":").strip()
    # Strip trailing words that are noise for navigation.
    title = re.sub(r"\s+(Configuration|Settings|Options)$", "", title,
                   flags=re.IGNORECASE)
    return title or None


SKIP_LABELS_LOWER = {
    "yes", "no", "ok", "cancel", "any", "all", "none", "auto", "default",
    "true", "false", "edit", "create", "load", "next", "previous",
    "monthly", "weekly", "daily", "hourly", "minute", "second",
    "back", "help", "exit",
}

def _looks_like_format(s: str) -> bool:
    """True if the string contains a printf format specifier - meaning it's
    really a format string, not a sysop-facing label."""
    return bool(re.search(r"%[-+ 0#]?[*0-9.]*[hljztL]*[diouxXeEfgGaAcspn]", s))


def label_is_skippable(label: str) -> bool:
    norm = label.strip().lower()
    if not norm:           return True
    if norm in SKIP_LABELS_LOWER: return True
    if len(norm) <= 2:     return True
    # Picker dialogs typically present column-padded value choices like
    # `areas.bbs       SBBSecho Area File`. Real menu option labels never
    # contain runs of multiple consecutive spaces.
    if "  " in label:
        return True
    return False


def line_at(text: str, off: int) -> int:
    return text.count("\n", 0, off) + 1


def brace_depth_at(text: str, body_start: int, off: int) -> int:
    """Count `{` minus `}` between body_start and off, ignoring chars in
    string/character literals and comments. body_start is assumed to be
    just past the function's opening brace (i.e. depth starts at 0)."""
    depth = 0
    i = body_start
    while i < off:
        skip = _skip_strings_comments(text, i)
        if skip is not None:
            i = min(skip, off)
            continue
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    return depth


def _match_brace_close(text: str, brace_open_off: int, body_end: int) -> int | None:
    """Given offset of a `{`, return offset of the matching `}`."""
    depth = 1
    k = brace_open_off + 1
    while k < body_end and depth > 0:
        skip = _skip_strings_comments(text, k)
        if skip is not None:
            k = skip
            continue
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return k
        k += 1
    return None


def _enclosing_loop_brace_close(text: str, off: int, body_start: int,
                                body_end: int) -> int | None:
    """Find the closing `}` of the most recent `while (...) {` or
    `for (...) {` whose body brace-encloses `off` and is OPEN at `off`.
    Used to bound the scope of a list whose dispatch is via MSK_* on a
    variable (no enclosing switch)."""
    # Walk back through prior `{` chars at successively shallower depths
    # until we find a matching loop keyword.
    depth = 0
    i = off - 1
    while i > body_start:
        skip_b = _skip_strings_comments(text, i)
        if skip_b is not None and skip_b > i:
            # Walked into a string/comment region while scanning backward;
            # _skip_strings_comments only knows how to skip forward, so
            # just step back one and try again.
            i -= 1
            continue
        c = text[i]
        if c == "}":
            depth += 1
        elif c == "{":
            if depth == 0:
                # Found enclosing `{`. Look at the keyword preceding `(`.
                j = i - 1
                while j > body_start and text[j].isspace():
                    j -= 1
                if j > body_start and text[j] == ")":
                    # find matching `(`
                    pd = 1
                    k = j - 1
                    while k > body_start and pd > 0:
                        if text[k] == ")":
                            pd += 1
                        elif text[k] == "(":
                            pd -= 1
                            if pd == 0:
                                break
                        k -= 1
                    if pd == 0:
                        # k is offset of `(`; check keyword before
                        m = k - 1
                        while m > body_start and text[m].isspace():
                            m -= 1
                        kw_end = m + 1
                        while m > body_start and (text[m].isalnum() or text[m] == "_"):
                            m -= 1
                        kw = text[m + 1:kw_end]
                        if kw in ("while", "for"):
                            return _match_brace_close(text, i, body_end)
                # Not a loop - keep walking outward.
            depth -= 1
        i -= 1
    return None


def _end_of_call(text: str, call_start: int, body_end: int) -> int | None:
    """Return the offset just past the closing `)` of the call beginning at
    `call_start` (where `call_start` points at the start of the function
    name)."""
    # Find the opening `(`.
    i = text.find("(", call_start, body_end)
    if i < 0:
        return None
    bal = _balanced_call(text, i)
    if not bal:
        return None
    return bal[0] + 1


def find_scope_range(text: str, list_off: int, body_end: int,
                     body_start: int) -> tuple | None:
    """Return (brace_open, scope_end) for the menu's case-handler scope:
    the offset of the switch's opening `{` and the matching `}`. For the
    MSK_* dispatch form the loop body has no clean brace_open, so we
    return (None, scope_end). Returns None entirely if no scope at all."""
    after_call = _end_of_call(text, list_off, body_end)
    if after_call is None:
        return None
    # Direct `switch (uifc.list(...)) { ... }` form.
    j = after_call
    while j < body_end:
        skip = _skip_strings_comments(text, j)
        if skip is not None:
            j = skip
            continue
        c = text[j]
        if c.isspace() or c == ")":
            j += 1
            continue
        break
    if j < body_end and text[j] == "{":
        end = _match_brace_close(text, j, body_end)
        return (j, end) if end is not None else None
    # Assigned-var form: look for `switch (var) {` after the call.
    var = _assigned_var(text, list_off)
    if var:
        sw_pat = re.compile(
            r'\bswitch\s*\(\s*' + re.escape(var) + r'\s*\)\s*\{')
        m = sw_pat.search(text, after_call, body_end)
        if m:
            brace_off = m.end() - 1
            end = _match_brace_close(text, brace_off, body_end)
            return (brace_off, end) if end is not None else None
        # MSK_* dispatch: scope is the enclosing while/for body, no
        # straightforward switch brace_open.
        msk_pat = re.compile(
            r'\b' + re.escape(var) + r'\s*&\s*MSK_(ON|OFF)\b')
        if msk_pat.search(text, after_call, body_end):
            end = _enclosing_loop_brace_close(
                text, list_off, body_start, body_end)
            return (None, end) if end is not None else None
    return None


def find_scope_end(text: str, list_off: int, body_end: int,
                   body_start: int) -> int | None:
    """Backward-compatible wrapper returning just the scope's closing `}`."""
    r = find_scope_range(text, list_off, body_end, body_start)
    return r[1] if r else None


def _enclosing_switch(text: str, off: int) -> bool:
    """True if the call at offset `off` is the controlling expression of a
    `switch ( ... )` statement."""
    i = off - 1
    parens = 0
    while i > 0:
        c = text[i]
        if c.isspace():
            i -= 1; continue
        if c == ')':
            parens += 1; i -= 1; continue
        if c == '(':
            if parens > 0:
                parens -= 1; i -= 1; continue
            j = i - 1
            while j >= 0 and text[j].isspace():
                j -= 1
            kw_end = j + 1
            while j >= 0 and (text[j].isalnum() or text[j] == "_"):
                j -= 1
            kw = text[j + 1:kw_end]
            return kw == "switch"
        i -= 1
    return False


def _assigned_var(text: str, off: int) -> str | None:
    """If the call at offset `off` is the RHS of an assignment like
    `var = uifc.list(...)`, return `var`. Otherwise None."""
    i = off - 1
    while i > 0 and text[i].isspace():
        i -= 1
    if i <= 0 or text[i] != "=":
        return None
    if i > 0 and text[i - 1] in "<>=!+-*/%&|^":
        return None  # comparison or compound, not a plain assignment
    j = i - 1
    while j >= 0 and text[j].isspace():
        j -= 1
    end = j + 1
    while j >= 0 and (text[j].isalnum() or text[j] == "_"):
        j -= 1
    if end == j + 1:
        return None
    return text[j + 1:end]


_DRILL_PAT = re.compile(
    r'\b(?:uifc\.list|uifc\.input|uifc\.showbuf|getar|toggle_flag|'
    r'choose_io_method|whichlogic|whichcond)\s*\(')


def _find_first_at_depth(text: str, off: int, body_end: int, body_start: int,
                          patterns: list) -> str | None:
    """Linearly scan from off+1 forward, tracking brace depth (skipping over
    string/char/comment regions). At positions whose depth equals the depth
    at `off`, try each pattern in order; return the kind tag of the first
    match, or None if no pattern matches before body_end."""
    target_depth = brace_depth_at(text, body_start, off)
    depth = target_depth
    i = off + 1
    while i < body_end:
        skip = _skip_strings_comments(text, i)
        if skip is not None:
            i = skip
            continue
        c = text[i]
        if c == '{':
            depth += 1
            i += 1
            continue
        if c == '}':
            depth -= 1
            i += 1
            continue
        if depth == target_depth:
            for kind, pat in patterns:
                if pat.match(text, i):
                    return kind
        i += 1
    return None


def is_navigable_list(text: str, off: int, body_start: int, body_end: int,
                      known_funcs: set[str]) -> bool:
    """A SCFG `uifc.list` call is navigable (a real menu, not a one-shot
    picker) if its dispatch path contains FURTHER drill-down calls
    (uifc.list/uifc.input/getar/toggle_flag/etc.) - i.e. selecting an
    option does more than just set a value.

    Pickers (e.g. "Allowed Characters in Uploaded Filenames") look like
    real menus structurally (they're in a switch) but their case handlers
    just assign flags. Filtering those out keeps them out of the search
    index AND prevents them from being treated as ancestors of other
    menus that happen to live in the same outer switch.
    """
    has_dispatch = False
    end_search = body_end
    if _enclosing_switch(text, off):
        has_dispatch = True
        end_search = find_scope_end(text, off, body_end, body_start) or body_end
    else:
        var = _assigned_var(text, off)
        if var:
            # Bound the dispatch lookup at the SAME brace depth as the
            # assignment. A `switch (var)` at the same depth is the canonical
            # dispatch; a `var = ...` reassignment at the same depth means
            # the result of this uifc.list flows into the next picker
            # instead. Reassignments at DEEPER depth (inside an `if (...) {
            # ... break; }` early-exit block, for instance) are conditional
            # and don't disrupt the flow to a same-depth switch.
            # Accept direct dispatch `switch (var)` and indirection-table
            # dispatch `switch (table[var])` (used when options are conditionally
            # hidden and a parallel action[] array maps the menu index to a
            # stable action code - see rate_limit_cfg).
            sw_pat  = re.compile(r'\bswitch\s*\(\s*(?:[A-Za-z_][A-Za-z0-9_]*\s*\[\s*)?'
                                 + re.escape(var) + r'\s*\]?\s*\)')
            msk_pat = re.compile(r'\b' + re.escape(var) + r'\s*&\s*MSK_(?:ON|OFF)\b')
            ra_pat  = re.compile(r'\b' + re.escape(var) + r'\s*=\s*[^=]')
            kind = _find_first_at_depth(text, off, body_end, body_start,
                                        [('sw', sw_pat),
                                         ('msk', msk_pat),
                                         ('ra', ra_pat)])
            if kind in ('sw', 'msk'):
                has_dispatch = True
                end_search = find_scope_end(text, off, body_end, body_start) or body_end
    if not has_dispatch:
        return False
    # Now require that the dispatched scope contains at least one drill-in
    # call (other than the nav itself, which is at `off`).
    scan_start = off + 1
    if scan_start >= end_search:
        return False
    for m in _DRILL_PAT.finditer(text, scan_start, end_search):
        # Skip the original uifc.list call itself if it falls within the
        # search range (shouldn't, since we start at off+1).
        # Skip matches inside string/char literals or comments.
        if _skip_strings_comments(text, m.start()) is not None:
            continue
        return True
    # Also navigable if the case bodies delegate to another known SCFG
    # function (e.g. server_cfg's case handlers call ftpsrvr_cfg etc. -
    # the dispatch IS to other menus, just one indirection away). Without
    # this, top-level "umbrella" menus that consist of nothing but helper
    # calls get misclassified as pickers and break the ancestor chain.
    for m in PAT_CALL.finditer(text, scan_start, end_search):
        callee = m.group(1)
        if callee in C_KEYWORDS_LIKE_CALL:
            continue
        if callee not in known_funcs:
            continue
        if _skip_strings_comments(text, m.start()) is not None:
            continue
        return True
    return False


# A "FuncInfo" tracks everything we extracted from one function body.
class FuncInfo:
    __slots__ = ("name", "file", "first_title", "options", "calls")
    def __init__(self, name, file_):
        self.name = name
        self.file = file_
        self.first_title: str | None = None
        # Each option: (label, line, [menu_chain - outer first, immediate last])
        self.options: list[tuple[str, int, list[str]]] = []
        # Each call: (callee_name, line, current_title_at_call_site)
        self.calls: list[tuple[str, int, str | None]] = []


def parse_function(text: str, body_start: int, body_end: int,
                   func_name: str, file_name: str,
                   known_funcs: set[str]) -> FuncInfo:
    info = FuncInfo(func_name, file_name)

    # Pre-pass: collect any `char* foo[] = { "...", "...", NULL };` arrays
    # declared in this function body. Used as the options list when a
    # uifc.list call passes a named array (e.g. main()'s `mopt`) rather
    # than the global snprintf'd `opt[]`.
    static_arrays: dict[str, list[str]] = {}
    for sa in PAT_STATIC_OPT_ARR.finditer(text, body_start, body_end):
        arr_name = sa.group(1)
        brace_off = sa.end() - 1  # position of '{'
        close_off = _match_brace_close(text, brace_off, body_end)
        if close_off is None:
            continue
        labels: list[str] = []
        for sm in re.finditer(STRLIT, text[brace_off + 1:close_off]):
            d = _decode(sm.group(0)[1:-1])
            if d:  # skip NULL/empty entries
                labels.append(d)
        if labels:
            static_arrays[arr_name] = labels

    # Walk the body linearly tracking events in source order.
    events = []  # (offset, kind, payload)
    n = body_end
    i = body_start
    var_titles: dict[str, str] = {}
    last_helpbuf_title: str | None = None

    def first_string_literal(args):
        for a in args:
            a = a.strip()
            if re.fullmatch(STRLIT, a):
                return _decode(a[1:-1])
        return None

    while i < n:
        skip = _skip_strings_comments(text, i)
        if skip is not None:
            i = skip
            continue
        # Try patterns in priority order. Find the next match for each
        # within the rest of body.
        # We just search starting from i.
        m_sp = PAT_SNPRINTF_OPT.search(text, i, n)
        m_st = PAT_STRCPY_OPT.search(text, i, n)
        m_ul = PAT_UIFC_LIST.search(text, i, n)
        m_vf = PAT_VAR_FMT.search(text, i, n)
        m_vs = PAT_VAR_CPY.search(text, i, n)
        m_call = PAT_CALL.search(text, i, n)
        # Skip C-keyword "calls" (switch/if/while/for/...) so they don't
        # eat the parenthesized expression that may contain real calls.
        while m_call and m_call.group(1) in C_KEYWORDS_LIKE_CALL:
            m_call = PAT_CALL.search(text, m_call.end(), n)
        m_help = PAT_HELPBUF.search(text, i, n)

        # Pick the earliest, breaking ties by priority.
        candidates = []
        if m_sp:   candidates.append((m_sp.start(),   1, "opt",     m_sp))
        if m_st:   candidates.append((m_st.start(),   1, "opt",     m_st))
        if m_ul:   candidates.append((m_ul.start(),   2, "list",    m_ul))
        if m_vf:   candidates.append((m_vf.start(),   3, "varfmt",  m_vf))
        if m_vs:   candidates.append((m_vs.start(),   3, "varfmt",  m_vs))
        if m_call: candidates.append((m_call.start(), 4, "call",    m_call))
        if m_help: candidates.append((m_help.start(), 5, "helpbuf", m_help))
        if not candidates:
            break
        candidates.sort()
        start, _prio, kind, m = candidates[0]

        # Skip past comments/strings if our match landed inside one.
        skip = _skip_strings_comments(text, start)
        if skip is not None:
            i = max(start + 1, skip)
            continue

        if kind == "helpbuf":
            t = helpbuf_title(m.group(1))
            if t:
                last_helpbuf_title = t
            i = m.end()
            continue

        # Determine the call's outer paren.
        paren = text.find("(", m.start(), m.end() + 1)
        if paren < 0:
            i = m.end()
            continue
        bal = _balanced_call(text, paren)
        if not bal:
            i = m.end()
            continue
        end_off, inner = bal
        args = _split_top_level_args(inner)
        i_next = end_off + 1
        ln = line_at(text, start)

        if kind == "helpbuf":
            # Track the most recent helpbuf title so we can fall back to it
            # if a uifc.list has a dynamic title we cannot resolve.
            t = helpbuf_title(m.group(1))
            if t:
                last_helpbuf_title = t
            i = m.end()
            continue

        if kind == "opt":
            # Determine which arg holds the label.
            # snprintf(opt[..], MAX_OPLN, "fmt", "Label", ...)  -> args[3]
            # sprintf (opt[..],            "fmt", "Label", ...) -> args[2]
            # strcpy  (opt[..], "Label")                         -> args[1]
            # The format string at args[1] (sprintf) or args[2] (snprintf)
            # is itself a literal but is not a label.
            label = None
            if len(args) >= 2:
                args1_is_literal = re.fullmatch(STRLIT, args[1].strip()) is not None
                # strcpy form: only 2 args, second is the label.
                if len(args) == 2:
                    cand = args[1].strip()
                    if re.fullmatch(STRLIT, cand):
                        label = _decode(cand[1:-1]).strip()
                else:
                    # If args[1] is the format string -> sprintf form.
                    fmt_arg = 1 if args1_is_literal else 2
                    label_arg = fmt_arg + 1
                    if label_arg < len(args):
                        cand = args[label_arg].strip()
                        if re.fullmatch(STRLIT, cand):
                            decoded = _decode(cand[1:-1]).strip()
                            # Reject if it looks like another format string.
                            if decoded and not _looks_like_format(decoded):
                                label = decoded
            if label and not label_is_skippable(label):
                events.append((start, "opt", (label, ln)))
        elif kind == "varfmt":
            name = m.group(1)
            if name == "opt":
                # Already handled by "opt" pattern.
                pass
            else:
                for cand_idx in (1, 2):
                    if cand_idx < len(args):
                        a = args[cand_idx].strip()
                        if re.fullmatch(STRLIT, a):
                            decoded = _decode(a[1:-1])
                            stable = stable_portion(decoded)
                            if stable:
                                var_titles[name] = stable
                                break
        elif kind == "list":
            title = None
            if len(args) >= 7:
                t = args[6].strip()
                if re.fullmatch(STRLIT, t):
                    title = _decode(t[1:-1]).strip()
                else:
                    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", t) and t in var_titles:
                        title = var_titles[t]
            if title is None:
                for arg in args:
                    a = arg.strip()
                    if re.fullmatch(STRLIT, a):
                        title = _decode(a[1:-1]).strip()
                        break
            if title is None and last_helpbuf_title:
                title = last_helpbuf_title
            # Capture the options-array name (last arg). When it's a named
            # array (not the global `opt`), we resolve its labels from the
            # static-array pre-scan. Otherwise the labels come from the
            # accumulated snprintf(opt[..],..) calls preceding this list.
            options_arr = None
            if len(args) >= 8:
                last = args[7].strip()
                if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", last):
                    options_arr = last
            # Determine whether this list is a navigable menu (its result
            # is dispatched via case handlers) versus a one-shot picker
            # (its result is just stored). Picker entries are value choices,
            # not options the sysop navigates to, so they don't belong in
            # the search index.
            is_nav = is_navigable_list(text, start, body_start, n, known_funcs)
            if title:
                events.append((start, "list",
                               (normalize_title(title), ln, is_nav, options_arr)))
        elif kind == "call":
            callee = m.group(1)
            # Filter: only record calls into our known function set.
            if callee in known_funcs and callee != func_name:
                events.append((start, "call", (callee, ln)))

        i = i_next

    # Build a list of navigable nav lists with offset, title, scope range,
    # and the switch's opening brace (for depth-aware case-index lookup).
    # Sub-menus that fall inside another nav's scope are its descendants.
    nav_list_infos = []  # (offset, title, brace_open, scope_end)
    for off, kind, payload in events:
        if kind != "list":
            continue
        title, ln, is_nav, _options_arr = payload
        if not is_nav:
            continue
        rng = find_scope_range(text, off, body_end, body_start)
        if rng is None:
            brace_open, scope_end = None, body_end
        else:
            brace_open, scope_end = rng
            if scope_end is None:
                scope_end = body_end
        nav_list_infos.append((off, title, brace_open, scope_end))
        if info.first_title is None:
            info.first_title = title

    def menu_chain_for(opt_off: int) -> list[str] | None:
        """Build the chain of menus enclosing this option, outermost first.
        The immediate menu is the first nav list after opt_off; ancestors
        are nav lists earlier in source whose scope range CONTAINS the
        immediate nav."""
        immediate = None
        for entry in nav_list_infos:
            li_off = entry[0]
            if li_off > opt_off:
                immediate = entry
                break
        if immediate is None:
            return None
        chain = [immediate[1]]
        target_off = immediate[0]
        for li_off, li_title, _li_bo, li_scope_end in nav_list_infos:
            if li_off >= immediate[0]:
                break
            # Ancestor iff its scope encloses the immediate nav.
            if li_off < target_off <= li_scope_end:
                chain.insert(-1, li_title)
        return chain

    # Post-process: for each opt event, look up its menu chain. For each
    # call, also attach the option label (in the enclosing nav list) the
    # user clicked to dispatch to that callee - the case index in the
    # switch corresponds 1:1 to the opt[] index.
    pending_opt_offs: list[int] = []
    pending_opt_lines: list[int] = []
    pending_opt_labels: list[str] = []
    current_title: str | None = None
    # nav_off -> [labels in opt[0..N-1] order], for case-index lookup.
    nav_options_by_off: dict[int, list[str]] = {}
    for off, kind, payload in events:
        if kind == "opt":
            label, ln = payload
            pending_opt_offs.append(off)
            pending_opt_lines.append(ln)
            pending_opt_labels.append(label)
        elif kind == "list":
            title, ln, is_nav, options_arr = payload
            if is_nav:
                current_title = title
                # Use the static-array's labels when the call passed a
                # named array; otherwise the snprintf-collected labels.
                if (options_arr and options_arr != "opt"
                        and options_arr in static_arrays):
                    nav_options_by_off[off] = list(static_arrays[options_arr])
                else:
                    nav_options_by_off[off] = list(pending_opt_labels)
                for o_off, o_ln, o_label in zip(
                        pending_opt_offs, pending_opt_lines, pending_opt_labels):
                    chain = menu_chain_for(o_off)
                    if chain:
                        info.options.append((o_label, o_ln, chain))
            pending_opt_offs.clear()
            pending_opt_lines.clear()
            pending_opt_labels.clear()
        elif kind == "call":
            callee, ln = payload
            # Build the chain of enclosing nav lists (outermost first).
            # For each, look up the option label of the case dispatching
            # this call. Helpers called from inline sub-menus inside main()
            # (e.g. main()'s case 6 contains the "Chat Features" nav whose
            # cases dispatch guru_cfg/actsets_cfg/etc.) are enclosed by
            # BOTH the inner nav and main's outer Configure nav, so the
            # full chain - "Chat Features", "Multinode Chat Actions" - is
            # what the path needs.
            entry_chain: list[str] = []
            enclosing_navs = [
                (nl_off, nl_bo)
                for nl_off, _nl_title, nl_bo, nl_scope_end in nav_list_infos
                if nl_off < off and off < nl_scope_end
            ]
            enclosing_navs.sort()  # outermost first (lowest offset)
            for nl_off, nl_bo in enclosing_navs:
                # Need the depth at which THIS nav's case labels live.
                # Without brace_open we can't disambiguate nested switches'
                # cases from this nav's own cases - skip then.
                if nl_bo is None:
                    continue
                target_depth = brace_depth_at(text, body_start, nl_bo) + 1
                last_case = None
                depth = target_depth - 1  # depth right BEFORE the '{'
                i = nl_bo
                while i < off:
                    skip = _skip_strings_comments(text, i)
                    if skip is not None:
                        i = skip
                        continue
                    c = text[i]
                    if c == '{':
                        depth += 1
                        i += 1
                        continue
                    if c == '}':
                        depth -= 1
                        i += 1
                        continue
                    if depth == target_depth:
                        m = PAT_CASE_NUM.match(text, i)
                        if m:
                            last_case = int(m.group(1))
                            i = m.end()
                            continue
                    i += 1
                if last_case is None:
                    continue
                labels = nav_options_by_off.get(nl_off, [])
                if 0 <= last_case < len(labels):
                    entry_chain.append(labels[last_case])
            info.calls.append((callee, ln, current_title, entry_chain))
    return info


# ---------------------------------------------------------------------------
# Path resolution
# ---------------------------------------------------------------------------

def build_paths(option_func: str, menu_chain: list[str], label: str,
                finfo_by_name: dict, callers_of: dict) -> list[str]:
    """Return one resolved menu path per distinct caller chain from
    option_func up to main. Helpers called from multiple places (e.g.
    rate_limit_cfg from each server's submenu, login_attempt_cfg from
    every server type) get one entry per caller so a sysop searching for
    the option lands on every menu path that reaches it.

    Each segment is the option LABEL the user clicked to enter that menu
    (e.g. main()'s "Servers" option label, not server_cfg()'s
    "Server Configuration" screen title). Falls back to the function's
    first nav title when no entry label is recorded.

    `menu_chain` is the ordered list of menu titles WITHIN option_func
    that enclose the option (outermost first, immediate last). The FIRST
    entry is option_func's own primary nav title (i.e. first_title) -
    that gets replaced by the entry label the caller used to reach
    option_func, so it's stripped here. Any inner entries (sub-navs
    within option_func) are real navigation steps the user clicks
    through, so they stay."""
    inner_chain = list(menu_chain[1:]) if menu_chain else []
    suffix = inner_chain + ([label] if label else [])

    results: list[list[str]] = []

    def walk(current: str, prefix: list[str], visited: frozenset[str]):
        if current in visited:
            results.append(list(prefix))
            return
        visited = visited | {current}
        callers = callers_of.get(current, [])
        if not callers:
            # No caller registered - fall back to current's first nav
            # title so the path still has a leading anchor.
            cur_info = finfo_by_name.get(current)
            new_prefix = list(prefix)
            if cur_info and cur_info.first_title:
                new_prefix.insert(0, cur_info.first_title)
            results.append(new_prefix)
            return
        for parent_entry in callers:
            parent = parent_entry[0]
            entry_chain = parent_entry[3] if len(parent_entry) > 3 else []
            seg_prefix: list[str] = []
            if entry_chain:
                # Outermost first; insert in reverse so they end up in order.
                for seg in reversed(entry_chain):
                    if seg:
                        seg_prefix.insert(0, seg)
            else:
                cur_info = finfo_by_name.get(current)
                seg = cur_info.first_title if cur_info else None
                if seg:
                    seg_prefix.insert(0, seg)
            new_prefix = seg_prefix + prefix
            if parent == "main":
                results.append(["Configure"] + new_prefix)
            else:
                walk(parent, new_prefix, visited)

    walk(option_func, [], frozenset())

    # De-duplicate consecutive equal entries (a function whose first_title
    # matches its caller's option label would otherwise duplicate) and
    # collapse identical paths across caller chains.
    out_paths: list[str] = []
    seen: set[str] = set()
    for prefix in results:
        full = prefix + suffix
        dedup: list[str] = []
        for p in full:
            if not p:
                continue
            if dedup and dedup[-1] == p:
                continue
            dedup.append(p)
        s = " > ".join(dedup)
        if s and s not in seen:
            seen.add(s)
            out_paths.append(s)
    return out_paths


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def c_string_literal(s: str) -> str:
    out = []
    for ch in s:
        o = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\n":
            out.append("\\n")
        elif 32 <= o < 127:
            out.append(ch)
        else:
            out.append(f"\\x{o:02X}")
    return '"' + "".join(out) + '"'


def emit(entries: list[dict]) -> str:
    lines = []
    lines.append("/*")
    lines.append(" * scfgindex.h - AUTO-GENERATED. Do not edit by hand.")
    lines.append(" *")
    lines.append(" * Generated by gen_option_index.py from snprintf(opt[..],..)")
    lines.append(" * and uifc.list(..) calls in the surrounding scfg*.c files.")
    lines.append(" * Re-run that script whenever option labels, menu titles,")
    lines.append(" * or call relationships change.")
    lines.append(" */")
    lines.append("#ifndef SCFGINDEX_H")
    lines.append("#define SCFGINDEX_H")
    lines.append("")
    lines.append('#include "scfgsrch.h"')
    lines.append("")
    lines.append("static const scfg_option_t scfg_option_index[] = {")
    for e in entries:
        lines.append("\t{")
        lines.append("\t\t" + c_string_literal(e["label"]) + ",")
        lines.append("\t\t" + c_string_literal(e["menu"])  + ",")
        lines.append("\t\t" + c_string_literal(e["path"]))
        lines.append("\t},")
    lines.append("};")
    lines.append("")
    lines.append("static const size_t scfg_option_index_count =")
    lines.append("\tsizeof(scfg_option_index) / sizeof(scfg_option_index[0]);")
    lines.append("")
    lines.append("#endif /* SCFGINDEX_H */")
    return "\n".join(lines) + "\n"


def main():
    # Pass 1: collect every function in every source file.
    file_texts: dict[str, str] = {}
    file_funcs: dict[str, list[tuple[str, int, int]]] = {}
    all_func_names: set[str] = set()
    for fn in SCFG_SOURCES:
        path = os.path.join(HERE, fn)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        file_texts[fn] = text
        funcs = list(find_functions(text))
        file_funcs[fn] = funcs
        for name, _, _ in funcs:
            all_func_names.add(name)

    # Pass 2: for each function, parse options/lists/calls.
    finfo_by_name: dict[str, FuncInfo] = {}
    for fn, funcs in file_funcs.items():
        text = file_texts[fn]
        for name, body_start, body_end in funcs:
            info = parse_function(text, body_start, body_end, name, fn,
                                  all_func_names)
            # If a function is defined more than once across files (rare),
            # keep the first; emit a warning.
            if name in finfo_by_name:
                continue
            finfo_by_name[name] = info

    # Build callee -> list of (caller, line, title_at_call_site,
    # entry_chain). entry_chain is the ordered list of option labels (one
    # per nav enclosing the call site, outermost first) the user clicks to
    # reach this call. Helpers reached through nested inline navs (e.g.
    # main's case 6 contains a "Chat Features" sub-nav) get a multi-element
    # chain like ["Chat Features", "Multinode Chat Actions"].
    #
    # Skip CALLER_BLACKLIST: functions that aren't reachable from main's
    # Configure-menu dispatch (the only context where Ctrl-F search is
    # available). cfg_wizard runs once at first-install before main's menu
    # loop; indexing its callees would pollute the search results with
    # paths that have no "Configure >" anchor (and steal attribution from
    # the real menu callers via alphabetical sort).
    CALLER_BLACKLIST = {"cfg_wizard"}
    callers_of: dict[str, list[tuple[str, int, str | None, list[str]]]] = {}
    for caller, info in finfo_by_name.items():
        if caller in CALLER_BLACKLIST:
            continue
        for callee, ln, title, entry_chain in info.calls:
            callers_of.setdefault(callee, []).append(
                (caller, ln, title, entry_chain))

    # Build entries: one per (option label, function it's defined in). We
    # also synthesise an entry for each navigable MENU itself, so a sysop
    # searching for the menu's name lands on the menu as a destination
    # (rather than all the options inside it). Menu entries use the menu's
    # own name as the `label` and its parent menu as `menu`.
    entries: list[dict] = []
    seen_menu_paths: set[str] = set()

    for func_name, info in finfo_by_name.items():
        for label, ln, menu_chain in info.options:
            paths = build_paths(func_name, menu_chain, label,
                                finfo_by_name, callers_of)
            for path in paths:
                if path.startswith("Configure > "):
                    path = path[len("Configure > "):]
                # The displayed "menu" for a result is the path segment
                # just before the option label, i.e. the option label the
                # user clicks to enter the menu containing this option.
                # Falling back to menu_chain[-1] (the screen title) only
                # when path resolution produces a single segment.
                path_parts = path.split(" > ") if path else []
                if len(path_parts) >= 2:
                    immediate_menu = path_parts[-2]
                else:
                    immediate_menu = menu_chain[-1] if menu_chain else ""
                entries.append({
                    "label": label,
                    "menu":  immediate_menu,
                    "path":  path,
                    "file":  info.file,
                    "line":  ln,
                })

            # Synthesise menu-as-destination entries: every menu in the
            # chain (and the chain's anchor up through main) is itself a
            # navigable target. Add one entry per unique menu path,
            # across every caller chain.
            for menu_path in build_paths(func_name, menu_chain, "",
                                         finfo_by_name, callers_of):
                if menu_path.endswith(" > "):
                    menu_path = menu_path[:-3]
                if menu_path.startswith("Configure > "):
                    menu_path = menu_path[len("Configure > "):]
                menu_path_parts = [p for p in menu_path.split(" > ") if p]
                for depth in range(1, len(menu_path_parts) + 1):
                    sub_path = " > ".join(menu_path_parts[:depth])
                    if sub_path in seen_menu_paths:
                        continue
                    seen_menu_paths.add(sub_path)
                    this_menu = menu_path_parts[depth - 1]
                    parent_menu = (menu_path_parts[depth - 2]
                                   if depth >= 2 else "")
                    entries.append({
                        "label": this_menu,
                        "menu":  parent_menu,
                        "path":  sub_path,
                        "file":  info.file,
                        "line":  ln,
                    })

    # De-duplicate by (label, menu, path).
    seen = set()
    deduped: list[dict] = []
    for e in entries:
        key = (e["label"], e["menu"], e["path"])
        if key in seen:
            continue
        seen.add(key)
        deduped.append(e)
    deduped.sort(key=lambda x: (x["label"].lower(), x["menu"].lower()))

    out = emit(deduped)
    with open(OUT_PATH, "w", encoding="utf-8") as f:
        f.write(out)
    print(f"Wrote {OUT_PATH}: {len(deduped)} unique options "
          f"(of {len(entries)} occurrences)")


if __name__ == "__main__":
    main()
