# Commit messages are for a future human, not for this session

A commit message is read months or years later by someone who was not here.
Write it so it stands on its own: what changed, and why. Keep it succinct.

**Never reference ephemeral local state.** The working tree, the index, a
scratch worktree, a file you reverted mid-session, what a subagent reported,
what "HEAD" happened to be while you worked — none of that exists for the
reader, and none of it survives the commit. Sentences like "Both files are
therefore back to HEAD" or "the temporary probe in the worktree is removed"
are chat replies, not commit messages. Delete them.

Likewise, don't narrate the session: no blow-by-blow of approaches tried and
abandoned, no "as discussed above", no addressing the user. Describe the diff
as it will be read by someone applying `git show` to it cold.

## The test

Before committing, read the message as if you had never seen this session.
If any sentence only makes sense to someone who watched the work happen,
rewrite it or cut it.

# Referencing a previous commit — give the date

When you cite an earlier commit — in a commit message, or in a code comment
where one has earned its place — give its date alongside the SHA:

    a1b2c3d (2026-07-25)

A SHA on its own says nothing about *when*. The reader cannot tell whether the
commit you named comes before or after the code in front of them, nor line it
up against a release, a bug report, or anything else they already know the date
of — and that ordering is usually the whole reason the commit was worth naming.
The date is also the part that still helps after a SHA stops resolving, in a
mirror, a tarball, or a rewritten branch.

Use the commit's own author date, not the date you are writing:

    git show -s --format=%as <sha>      # YYYY-MM-DD

Read it from the repo rather than guessing. A wrong date is worse than none: it
is confidently checkable, and it will be checked.

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
