# Comment discipline for C/C++ source and headers

Comments explain what the code cannot: the non-obvious, the edge case, the
"why it's this way and not the obvious other way." They are **not** a place to
narrate the change, restate what the code plainly does, or record the history
of how the code got here. Git already does that — `git log`, `git blame`, and
`git annotate` are the archaeology tools, and the commit message is where the
story of a change belongs.

**Do not add comments that duplicate the commit message.** A block comment
justifying a change — "we do X here instead of Y because Z would be racy",
"this was added to fix the hand-off in …", "preserve the existing behavior of
…" — is commit-message prose in the wrong place. It rots (the code moves on,
the comment doesn't), it bloats the source, and it says nothing `git blame`
plus the commit message wouldn't. Write that reasoning in the commit message
instead, where it is durably tied to the exact diff it describes.

## Use a comment only when it earns its place

- **Non-obvious or edge-case code** where a reader who understands C/C++ would
  still misread the intent, miss a subtle invariant, or "fix" a deliberate
  quirk. Keep it to the fact that isn't visible in the code — the invariant,
  the off-by-one that's intentional, the ordering constraint — not a paragraph.
- **A direct GitLab issue reference** when a fix or workaround exists because
  of a tracked problem: `// GitLab #1178` (or `(#1178)`), on or beside the code
  it concerns. The *details* of the problem — the analysis, the repro, the debate
  — live in the GitLab issue, which auto-links to the fixing commit. The
  in-code reference is a pointer, not a retelling.
- **Terse tool/analyzer directives** that must sit at the code site to work,
  each with a one-line justification — e.g.
  `// coverity[INTEGER_OVERFLOW:SUPPRESS] <why it's safe>`. These are exempt
  from "no rationale in comments" precisely because the annotation is inert
  without its reason, and the reason cannot live anywhere else.

## The test

Before writing a comment, ask: *"Is this something `git blame` + the commit
message, or a linked GitLab issue, would already tell the reader?"* If yes,
delete it and put it there instead. If the comment survives only because the
code genuinely surprises, keep it — and keep it short.
