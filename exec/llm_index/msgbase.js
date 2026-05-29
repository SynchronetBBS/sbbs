/*
 * llm_index/msgbase.js -- source crawler for local Synchronet message bases
 *
 * Walks all groups + subs visible to the running install, reads non-deleted
 * non-private messages, and emits one chunk per message (subject + body).
 * Provenance carries sub name, author, date so the LLM can cite when it
 * uses retrieved content in a reply.
 *
 * Returns: [{id, text, provenance}, ...]
 *
 * opts:
 *   opts.arg              -- comma-separated list of group NAMES to include,
 *                            with optional per-group sub exclusions in the
 *                            form group/-sub.  Set via source-spec syntax
 *                            in chat_llm.ini, e.g.:
 *                              index_sources = msgbase:Main,DOVE-Net
 *                              index_sources = msgbase:Main/-gitlog/-commits,DOVE-Net
 *                            Excludes apply CASE-INSENSITIVELY to sub
 *                            *code* OR sub *name*.  Use to skip bot-posted
 *                            subs that mirror GitLab webhooks (gitlog,
 *                            commits, sync-dev, etc.) -- those are
 *                            already covered better by the gitissue /
 *                            gitpush crawlers and would otherwise crowd
 *                            BM25 with low-value reformatted duplicates.
 *                            Empty/missing arg = all groups.
 *   opts.max_chunks       -- early-stop when this many chunks accumulated
 *   opts.min_body_length  -- skip messages shorter than this (default 30)
 *
 * Called from exec/llm_index.js. NOT meant to be run directly via jsexec.
 */

require('sbbsdefs.js', 'MSG_DELETE');

function crawl(opts)
{
    opts = opts || {};
    var max         = parseInt(opts.max_chunks, 10) || 5000;
    var min_body    = parseInt(opts.min_body_length, 10) || 30;
    var chunks            = [];
    var dup_count         = 0;
    var skipped_nonpublic = 0;

    /* Access-filter user: skip subs this user can't read.  Without
     * it the bot would index sysop-only / level-gated subs and
     * potentially cite their content to a guest caller who can't
     * actually reach the sub.  When 0/unset, no filtering. */
    var access_user = null;
    if (opts.access_user_num) {
        try {
            var u = new User(parseInt(opts.access_user_num, 10));
            if (u && u.number) access_user = u;
        } catch (e) {
            log(LOG_WARNING, 'llm_index/msgbase: bad access_user_num '
                + opts.access_user_num + ': ' + e);
        }
    }

    /* Parse opts.arg as a comma-separated allow-list of group names.
     * Per-group sub exclusions in the form GroupName/-subToken can be
     * appended to a group entry; -sub means "skip subs whose code OR
     * name matches sub (case-insensitive substring)".
     *
     * Entries starting with '+' name SPECIFIC SUB CODES to force-
     * include regardless of the access-filter user's ARS.  Use for
     * sysop-only subs that the bot should know about even though
     * regular callers can't read them (e.g. DOVE-Net's sync_sys,
     * where GitLab issue notifications land but require FLAG S).
     *
     * Examples:
     *   Main, DOVE-Net, fsxNet, FidoNet           -- allow these groups
     *   Main/-gitlog/-commits, DOVE-Net           -- allow Main except gitlog/commits subs
     *   Main, DOVE-Net, +sync_sys                 -- allow Main+DOVE-Net plus the
     *                                                 sync_sys sub even if guest can't read it
     */
    var allow_groups = null;     /* group-name (lower) -> true */
    var sub_excludes = {};       /* group-name (lower) -> [tokens] */
    var force_subs = {};         /* sub-code (lower) -> true */
    if (opts.arg) {
        allow_groups = {};
        var entries = String(opts.arg).split(',');
        for (var i = 0; i < entries.length; i++) {
            var entry = entries[i].replace(/^\s+|\s+$/g, '');
            if (!entry) continue;
            if (entry.charAt(0) === '+') {
                /* +subcode -- force-include this sub by code. */
                var sub_code = entry.slice(1).toLowerCase();
                if (sub_code) force_subs[sub_code] = true;
                continue;
            }
            /* Split on "/-" for sub-exclude tokens. */
            var ex_parts = entry.split('/-');
            var grp_name = ex_parts[0].toLowerCase();
            allow_groups[grp_name] = true;
            for (var j = 1; j < ex_parts.length; j++) {
                var tok = ex_parts[j].replace(/^\s+|\s+$/g, '').toLowerCase();
                if (!tok) continue;
                if (!sub_excludes[grp_name]) sub_excludes[grp_name] = [];
                sub_excludes[grp_name].push(tok);
            }
        }
    }

    /* Content-hash dedupe.  Catches periodic auto-posts (echo rules,
     * sponsor / NetBot ads, FidoNet area-fix help messages) that get
     * re-posted by their respective bots/moderators.  Each post has a
     * unique MsgID (it IS a new message technically) so MsgID-based
     * dedupe wouldn't catch them.  Content fingerprint does.
     *
     * Key = normalized subject + body (lowercased, whitespace
     * collapsed).  Object keys are JS strings so this gives exact
     * content match with no hash collisions to worry about.  Memory
     * cost is ~the body of every unique message which is fine for
     * BBS-scale corpora. */
    var seen      = Object.create(null);
    function fingerprint(subj, body) {
        return (subj + '\n' + body).toLowerCase()
            .replace(/\s+/g, ' ').replace(/^ | $/g, '');
    }

    /* msg_area.grp_list -> [{name, sub_list:[{code, name, ...}]}] */
    var groups = msg_area && msg_area.grp_list;
    if (!groups) return chunks;

    /* Reorder so force_subs (+subcode entries) are visited FIRST.
     * Otherwise they sit at their natural alphabetic position in
     * the group's sub_list and the per-source budget runs out
     * before we reach them (DOVE-Net has 23 subs ~1000 msgs each,
     * so the first 8-9 alpha-order subs exhaust an 8609 budget).
     * Build a flat sub_visit_list: forced subs first, then the
     * rest in their natural order, skipping subs we've already
     * placed at the front. */
    var sub_visit_list = [];   /* [{grp, sub}, ...] */
    var visited_codes  = {};
    for (var fs in force_subs) {
        for (var ag = 0; ag < groups.length; ag++) {
            for (var as = 0; as < groups[ag].sub_list.length; as++) {
                var fsub = groups[ag].sub_list[as];
                if ((fsub.code || '').toLowerCase() === fs) {
                    sub_visit_list.push({ grp: groups[ag], sub: fsub });
                    visited_codes[fs] = true;
                    /* Also force the parent group into allow_groups
                     * so the access-check fallback doesn't skip it
                     * if the user didn't list it. */
                    if (allow_groups)
                        allow_groups[(groups[ag].name || '').toLowerCase()] = true;
                    break;
                }
            }
        }
    }
    for (var ag = 0; ag < groups.length; ag++) {
        for (var as = 0; as < groups[ag].sub_list.length; as++) {
            var s = groups[ag].sub_list[as];
            if (visited_codes[(s.code || '').toLowerCase()]) continue;
            sub_visit_list.push({ grp: groups[ag], sub: s });
        }
    }

    /* Walk the precomputed visit list (force_subs first, then natural
     * group/sub order).  Each entry carries its parent group so we
     * can apply group-level allow / exclude rules. */
    for (var v = 0; v < sub_visit_list.length && chunks.length < max; v++) {
        var grp = sub_visit_list[v].grp;
        var sub = sub_visit_list[v].sub;

        /* Skip groups not in the allow-list (when one is set). */
        var grp_lower = (grp.name || '').toLowerCase();
        if (allow_groups && !allow_groups[grp_lower]) {
            continue;
        }
        var excludes = sub_excludes[grp_lower] || [];

        /* Group-level access check (only used when access_user set). */
        var grp_accessible = !access_user
                          || access_user.compare_ars(grp.ars || '');

        {
            var sub_code_lower = (sub.code || '').toLowerCase();
            var forced = !!force_subs[sub_code_lower];

            /* Sub-level access check against the filter user.  Skip
             * if the user can't satisfy the sub's combined ARS chain
             * (sub.ars + sub.read_ars on top of grp.ars).  Bypassed
             * for force-included subs (+subcode in opts.arg). */
            if (access_user && !forced) {
                if (!grp_accessible
                    || !access_user.compare_ars(sub.ars || '')
                    || !access_user.compare_ars(sub.read_ars || '')) {
                    skipped_nonpublic++;
                    continue;
                }
            }

            /* Apply sub-level excludes -- skip if the sub's code or
             * name contains any excluded token (case-insensitive
             * substring).  Use for bot-posted subs that mirror
             * webhook content (gitlog, commits, etc.).  Bypassed
             * for force-included subs. */
            if (excludes.length && !forced) {
                var code_lower = (sub.code || '').toLowerCase();
                var name_lower = (sub.name || '').toLowerCase();
                var skip = false;
                for (var x = 0; x < excludes.length; x++) {
                    if (code_lower.indexOf(excludes[x]) >= 0
                        || name_lower.indexOf(excludes[x]) >= 0) {
                        skip = true;
                        break;
                    }
                }
                if (skip) {
                    log(LOG_DEBUG, 'llm_index/msgbase: skipping sub "'
                        + sub.name + '" (excluded)');
                    continue;
                }
            }

            var mb = new MsgBase(sub.code);
            if (!mb.open()) continue;

            try {
                /* false/false: skip vote messages, don't expand lazy fields */
                var headers = mb.get_all_msg_headers(false, false);
                /* Walk newest-first.  Under the global max_chunks cap,
                 * the budget exhausts before reaching older messages,
                 * so we want the *recent* ones to land.  This biases
                 * retrieval toward currency: "latest SyncTERM release"
                 * pulls v1.9rc2 instead of v1.3 because the newer
                 * announcement is the one we indexed.  Msg numbers are
                 * monotonically increasing, so descending numeric sort
                 * = newest-first. */
                var hkeys = [];
                for (var hk in headers) hkeys.push(hk);
                hkeys.sort(function (a, b) {
                    return parseInt(b, 10) - parseInt(a, 10);
                });
                for (var hi = 0; hi < hkeys.length; hi++) {
                    if (chunks.length >= max) break;
                    var h = hkeys[hi];
                    var hdr = headers[h];
                    if (!hdr) continue;
                    if (hdr.attr & MSG_DELETE) continue;
                    /* Skip private mail-style messages even in pub subs */
                    if (typeof MSG_PRIVATE != 'undefined'
                        && (hdr.attr & MSG_PRIVATE)) continue;

                    var body;
                    try {
                        /* MsgBase.get_msg_body args:
                         *   header, strip_ctrl_a, dot_stuffing,
                         *   include_tails, plain_text
                         * We want tails stripped (no signatures, taglines,
                         * FTN Origin/SEEN-BY/PATH cruft) and plain-text
                         * extraction for MIME-encoded internet mail. */
                        body = mb.get_msg_body(hdr,
                            /* strip_ctrl_a:  */ true,
                            /* dot_stuffing:  */ false,
                            /* include_tails: */ false,
                            /* plain_text:    */ true);
                    } catch (e) {
                        continue;
                    }
                    if (!body) continue;

                    /* Drop quoted reply lines -- the original content is
                     * already in a separate indexed message. */
                    body = body.replace(/\r/g, '');
                    body = body.split('\n').filter(function (line) {
                        return !/^\s*(>|::)/.test(line);
                    }).join('\n');
                    /* Strip the legacy QWK-style reply header that many BBS
                     * readers embed at the top of replies:
                     *
                     *     Re: <parent subject>
                     *       By: <from> to <to> on <date>
                     *
                     * It's content, not RFC822 headers, so get_msg_body
                     * doesn't strip it.  Leaving it in dilutes the chunk's
                     * BM25 specificity -- a reply about "SyncTERM v1.9rc2"
                     * picks up "Re: SyncTERM v1.9rc1 released!" as a
                     * v1.9rc1 keyword hit and BM25-shadows the actual
                     * v1.9rc2 announcement.  Match-and-remove on the
                     * leading block only (the body proper follows).  Two
                     * shapes seen in the wild: with the Re: prefix line
                     * (BBS readers like SBBSecho's auto-quote), or
                     * without (poster manually removed it) -- handle
                     * both. */
                    body = body.replace(
                        /^\s*Re\s*:[^\n]*\n\s*By\s*:[^\n]*\n+/i, '');
                    body = body.replace(/\n{3,}/g, '\n\n').trim();
                    if (body.length < min_body) continue;

                    var date = '';
                    if (hdr.when_imported_time) {
                        date = new Date(hdr.when_imported_time * 1000)
                                   .toISOString().slice(0, 10);
                    }

                    var subj = (hdr.subject || '').replace(/\x01./g, '');
                    /* Strip "Re:" / "Re[N]:" / "Re Re Re ..." prefixes
                     * from reply subjects.  Reply subjects normally
                     * contain the PARENT thread's version/topic, which
                     * dilutes BM25 specificity of THIS message's
                     * content -- e.g. "Re: SyncTERM v1.9rc1 released!"
                     * on the v1.9rc2 reply makes the chunk score
                     * lower for "v1.9rc2"-specific queries because
                     * "v1.9rc1" steals keyword density.  Strip the
                     * Re: prefix so the chunk's subject reflects only
                     * what THIS message is about, while the body
                     * retains the full thread context. */
                    subj = subj.replace(/^\s*(?:re\s*(?:\[\d+\])?\s*:\s*)+/i, '');
                    var from = (hdr.from    || '?').replace(/\x01./g, '');

                    /* Content-hash dedupe: skip if we've seen this
                     * subject+body verbatim before (echo rules,
                     * sponsor ads, etc.).  First occurrence wins;
                     * Object.create(null) avoids prototype-key
                     * collisions on subjects like "constructor". */
                    var fp = fingerprint(subj, body);
                    if (seen[fp]) {
                        dup_count++;
                        continue;
                    }
                    seen[fp] = true;

                    /* Provenance carries Group/Sub so the bot can answer
                     * "what's been posted in DOVE-Net recently?" by
                     * recognizing the group name in the chunk header --
                     * previously the bot only saw the sub name, with no
                     * way to know that "Synchronet Announcements" sits
                     * inside the DOVE-Net group vs a local one.
                     *
                     * Also prepend a "Posted in <Group>/<Sub>" line to
                     * the chunk TEXT so BM25 can match queries that
                     * name the source ("what's in the DOVE-Net debate
                     * sub?", "what are users in Synchronet Discussion
                     * talking about?").  Previously the group/sub only
                     * lived in provenance; BM25 doesn't see provenance,
                     * so source-filtered queries got generic meta-
                     * content (the howto:dove-net wiki page) instead
                     * of actual posts from the named sub. */
                    var src_header = 'Posted in ' + grp.name + '/' + sub.name
                                   + ' sub-board by ' + from
                                   + (date ? ' on ' + date : '');

                    /* Bug/issue tag: many posts in sync_sys are mirrors
                     * of GitLab webhook events.  Their bodies carry
                     * the issue text but their subjects use the bug
                     * title (e.g. "services.ini: add NO_LISTEN ...")
                     * rather than meta-words "issue"/"bug"/"report" --
                     * so BM25 queries like "what are the most recently
                     * reported Synchronet issues?" miss them entirely.
                     * Detect via the hdr.to recipient, which the
                     * webhook bridge sets to "GitLab issue in main/sbbs"
                     * or "GitLab MR in main/sbbs" -- a very distinctive
                     * marker that's not in regular discussion posts. */
                    var to = (hdr.to || '').replace(/\x01./g, '');
                    var is_issue_post  = /^GitLab\s+(issue|MR|merge)/i.test(to);
                    var is_commit_post = /^Git\s+(commit|push)\b/i.test(to);
                    /* Inflection-friendly keyword tag: cover singular AND
                     * plural and a couple of common verb forms so BM25
                     * (which doesn't stem) catches "issue"/"issues",
                     * "bug"/"bugs", "report"/"reported"/"reporting",
                     * "ticket"/"tickets" all at once.  Same idea for
                     * commits: "commit"/"committed"/"commits",
                     * "contribute"/"contributor"/"contribution". */
                    var issue_tag = '';
                    if (is_issue_post) {
                        issue_tag = 'Bug bugs issue issues report reported '
                                  + 'reporting ticket tickets on Synchronet '
                                  + 'GitLab tracker. Recently filed defect '
                                  + 'feature request.\n';
                    } else if (is_commit_post) {
                        issue_tag = 'Git commit committed commits source code '
                                  + 'change patch contribution contributor '
                                  + 'contributed contributing recently authored '
                                  + 'pushed by developer to Synchronet GitLab '
                                  + 'repository.\n';
                    }

                    chunks.push({
                        id:         'msgbase/' + sub.code + '/' + hdr.number,
                        text:       src_header + '\n'
                                  + issue_tag
                                  + (subj ? subj + '\n' : '') + body,
                        provenance: 'Sub:' + grp.name + '/' + sub.name
                                  + ' "' + subj + '" by '
                                  + from + (date ? ' (' + date + ')' : ''),
                        title:      subj,
                        /* Unix-epoch post time for recency-decay scoring.
                         * Synchronet's when_written_time is the
                         * authored-at moment (vs when_imported_time which
                         * is when the message landed in our smb).  Falls
                         * back to import time if author time is missing. */
                        ts:         hdr.when_written_time
                                  || hdr.when_imported_time || 0
                    });
                }
            } catch (e) {
                log(LOG_WARNING, 'llm_index/msgbase: '
                    + sub.code + ': ' + e);
            }

            mb.close();
        }
    }

    if (dup_count)
        log(LOG_INFO, 'llm_index/msgbase: deduped ' + dup_count
            + ' content-duplicate messages');
    if (skipped_nonpublic)
        log(LOG_INFO, 'llm_index/msgbase: skipped ' + skipped_nonpublic
            + ' non-public subs (non-empty read ARS)');

    return chunks;
}

this;  /* load({}, 'llm_index/msgbase.js') exposes crawl() */
