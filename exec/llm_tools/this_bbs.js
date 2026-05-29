/*
 * llm_tools/this_bbs.js -- the this_bbs tool.
 *
 * Read-only inspection of the LOCAL host BBS's contents -- message
 * groups/subs, file libs/dirs, door sections/programs, recent posts,
 * and today's live activity counters.
 *
 * Depends on _common.js (THIS_BBS_NAME).  Has no external tool
 * dependencies otherwise -- the few helpers it shares conceptually
 * with bbs_directory (_safe_int) are inlined privately, since the
 * legacy get_bbs_status snapshot they were originally shared with
 * is gone.
 */

/* Small integer coercion used by _local_stats below. */
function _safe_int(n) {
    var v = parseInt(n, 10);
    return isNaN(v) ? 0 : v;
}

/* Access-filter user.  Mirrors the indexer's index_access_user knob
 * (see exec/llm_index/msgbase.js): we load that user record once
 * and use its compare_ars() to drop subs / dirs / doors the user
 * can't reach.  Without this, the bot would happily list sysop-only
 * subs to guests in IRC.  When unset / load fails, no filtering. */
var _access_user = null;
var _access_user_loaded = false;

function _get_access_user() {
    if (_access_user_loaded) return _access_user;
    _access_user_loaded = true;
    try {
        if (typeof system == 'undefined' || !system.ctrl_dir) return null;
        var f = new File(system.ctrl_dir + 'chat_llm.ini');
        if (!f.open('r')) return null;
        var raw = f.iniGetValue(null, 'index_access_user', '');
        f.close();
        if (raw === undefined || raw === null || raw === '') return null;
        var s = String(raw).replace(/^\s+|\s+$/g, '');
        if (!s) return null;
        var num = /^\d+$/.test(s) ? parseInt(s, 10) : system.matchuser(s);
        if (num > 0) {
            var u = new User(num);
            if (u && u.number) _access_user = u;
        }
    } catch (e) { /* leave null -- no filter */ }
    return _access_user;
}

function _user_can_read_sub(grp, sub) {
    var u = _get_access_user();
    if (!u) return true;
    if (grp.ars      && !u.compare_ars(grp.ars))      return false;
    if (sub.ars      && !u.compare_ars(sub.ars))      return false;
    if (sub.read_ars && !u.compare_ars(sub.read_ars)) return false;
    return true;
}

function _user_can_read_dir(lib, dir) {
    var u = _get_access_user();
    if (!u) return true;
    if (lib.ars && !u.compare_ars(lib.ars)) return false;
    if (dir.ars && !u.compare_ars(dir.ars)) return false;
    return true;
}

function _user_can_run_xtrn(sec, prog) {
    var u = _get_access_user();
    if (!u) return true;
    if (sec.ars && !u.compare_ars(sec.ars)) return false;
    if (prog.ars && !u.compare_ars(prog.ars)) return false;
    /* run_ars is the gate that matters for execution; some xtrns
     * also expose ars (same / less-strict).  Honor whichever is set. */
    if (prog.run_ars && !u.compare_ars(prog.run_ars)) return false;
    return true;
}

/* Resolve a group name (case-insensitive, exact then substring). */
function _find_group(query) {
    var q = String(query || '').toLowerCase();
    if (!q) return null;
    var grps = (typeof msg_area != 'undefined' && msg_area.grp_list) || [];
    for (var i = 0; i < grps.length; i++) {
        if (String(grps[i].name || '').toLowerCase() === q) return grps[i];
    }
    for (var i = 0; i < grps.length; i++) {
        if (String(grps[i].name || '').toLowerCase().indexOf(q) >= 0)
            return grps[i];
    }
    return null;
}

/* Resolve a sub-board by code (exact, lowercase) or name (substring).
 * Returns { sub, group } pair so callers can show the parent group
 * without having to walk grp_list a second time. */
function _find_sub_in_groups(query) {
    var q = String(query || '').toLowerCase();
    if (!q) return null;
    var grps = (typeof msg_area != 'undefined' && msg_area.grp_list) || [];
    /* Exact code match first -- sub codes are lowercased internally
     * per load_cfg.c so a lowercased query matches directly. */
    for (var gi = 0; gi < grps.length; gi++) {
        var subs = grps[gi].sub_list || [];
        for (var si = 0; si < subs.length; si++) {
            if (String(subs[si].code || '').toLowerCase() === q)
                return { sub: subs[si], group: grps[gi] };
        }
    }
    /* Then substring on the human-facing name. */
    for (var gi = 0; gi < grps.length; gi++) {
        var subs = grps[gi].sub_list || [];
        for (var si = 0; si < subs.length; si++) {
            if (String(subs[si].name || '').toLowerCase().indexOf(q) >= 0)
                return { sub: subs[si], group: grps[gi] };
        }
    }
    return null;
}

function _find_lib(query) {
    var q = String(query || '').toLowerCase();
    if (!q) return null;
    var libs = (typeof file_area != 'undefined' && file_area.lib_list) || [];
    for (var i = 0; i < libs.length; i++) {
        if (String(libs[i].name || '').toLowerCase() === q) return libs[i];
    }
    for (var i = 0; i < libs.length; i++) {
        if (String(libs[i].name || '').toLowerCase().indexOf(q) >= 0)
            return libs[i];
    }
    return null;
}

function _find_xtrn_section(query) {
    var q = String(query || '').toLowerCase();
    if (!q) return null;
    var secs = (typeof xtrn_area != 'undefined' && xtrn_area.sec_list) || [];
    for (var i = 0; i < secs.length; i++) {
        if (String(secs[i].name || '').toLowerCase() === q) return secs[i];
    }
    for (var i = 0; i < secs.length; i++) {
        if (String(secs[i].name || '').toLowerCase().indexOf(q) >= 0)
            return secs[i];
    }
    return null;
}

/* Open a sub-board MsgBase, pull the last N headers, return compact
 * records (newest-first).  Handles the lazy-field gotcha: any header
 * obtained via get_all_msg_headers() returns undefined on its first
 * touched field if that field is a *_NULL-defaulted one (to_ext,
 * from_ext, etc.).  Touch a non-NULL field (.to) first to prime. */
/* Local activity stats: today's counters from system.stats + lifetime
 * totals.  Replaces the relevant chunk of the removed get_bbs_status
 * tool for the specific "how busy is this BBS today / right now" use
 * case that lookup_bbs(THIS_BBS_NAME)'s cached_stats can't answer
 * (sbbslist verification snapshots are daily, not real-time). */
/* Format a byte count as a short human-readable string -- the 7B
 * model has trouble reading 10-digit integers and tends to drop a
 * place-value ("4,213,273,065 bytes" ends up as "42 billion bytes"
 * in chat output).  Pre-rendering as "3.92 GB" sidesteps that. */
function _human_bytes(n) {
    n = parseInt(n, 10) || 0;
    if (n >= 1024 * 1024 * 1024 * 1024) return (n / (1024 * 1024 * 1024 * 1024)).toFixed(2) + ' TiB';
    if (n >= 1024 * 1024 * 1024)        return (n / (1024 * 1024 * 1024)).toFixed(2) + ' GiB';
    if (n >= 1024 * 1024)               return (n / (1024 * 1024)).toFixed(1) + ' MiB';
    if (n >= 1024)                      return (n / 1024).toFixed(0) + ' KiB';
    return n + ' B';
}

function _local_stats() {
    var st = (typeof system != 'undefined' && system.stats) || {};
    /* Field-naming notes:
     *   - "logons" = login SESSIONS, not unique users.  Renamed to
     *     "login_sessions" so the model is less likely to misread
     *     it as "users" (observed failure 2026-05-27: the bot said
     *     "over 500k users" based on lifetime.logons = 509,795 while
     *     the actual active_users was 1,188).
     *   - bytes fields are pre-rendered as human-readable strings
     *     (e.g. "3.92 GiB") -- the raw 10-digit byte count is what
     *     the model would mis-quote ("4,213,273,065" -> "42 billion").
     *   - active_users moved BEFORE login_sessions in lifetime so the
     *     "user count" lookup hits the correct field first. */
    return {
        kind: 'stats',
        bbs:  THIS_BBS_NAME,
        today: {
            login_sessions:   _safe_int(st.logons_today),
            messages_posted:  _safe_int(st.messages_posted_today),
            email_sent:       _safe_int(st.email_sent_today),
            feedback_sent:    _safe_int(st.feedback_sent_today),
            files_uploaded:   _safe_int(st.files_uploaded_today),
            bytes_uploaded:   _human_bytes(st.bytes_uploaded_today),
            files_downloaded: _safe_int(st.files_downloaded_today),
            bytes_downloaded: _human_bytes(st.bytes_downloaded_today),
            timeon_seconds:   _safe_int(st.timeon_today)
        },
        lifetime: {
            active_users:   _safe_int(st.total_users),
            login_sessions: _safe_int(st.total_logons),
            messages:       _safe_int(st.total_messages),
            email:          _safe_int(st.total_email),
            feedback:       _safe_int(st.total_feedback),
            files:          _safe_int(st.total_files),
            timeon_seconds: _safe_int(st.total_timeon)
        }
    };
}

function _recent_messages(sub_query, limit) {
    var found = _find_sub_in_groups(sub_query);
    if (!found) {
        return { error: 'I do not see a sub-board called "' + sub_query
                 + '" on ' + THIS_BBS_NAME + '.' };
    }
    var sub = found.sub;
    if (!_user_can_read_sub(found.group, sub)) {
        return { error: 'The "' + (sub.name || sub.code)
                 + '" sub-board is not publicly readable.' };
    }
    var mb;
    try { mb = new MsgBase(sub.code); }
    catch (e) { return { error: 'I could not open the message base '
                 + 'for that sub-board.' }; }
    if (!mb.open()) {
        return { error: 'I could not open the message base '
                 + 'for that sub-board.' };
    }
    try {
        /* include_votes=false skips vote/poll rows; expand_fields=false
         * skips reverse_path expansion etc. that we don't need. */
        var hdrs = mb.get_all_msg_headers(false, false);
        var nums = [];
        for (var n in hdrs) {
            if (typeof hdrs[n] != 'object' || hdrs[n] == null) continue;
            nums.push(parseInt(n, 10));
        }
        nums.sort(function (a, b) { return a - b; });   /* ascending */
        var tail = nums.slice(-limit);
        var msgs = [];
        var now = time();
        for (var i = 0; i < tail.length; i++) {
            var h = hdrs[String(tail[i])];
            if (!h) continue;
            /* PRIME: touch a non-NULL field before any *_NULL field
             * (see javascript skill -- lazy-field gotcha).  .to is
             * always defined; reading it makes all other fields
             * including to_ext, from_ext, summary etc. resolve. */
            var primer = h.to; void primer;
            var written = h.when_written_time || 0;
            msgs.push({
                number:       h.number,
                from:         h.from || '',
                to:           h.to || '',
                subject:      h.subject || '',
                when_written: written
                    ? (new Date(written * 1000).toISOString().slice(0, 19) + ' UTC')
                    : '',
                age_seconds:  written ? (now - written) : 0
            });
        }
        msgs.reverse();   /* newest first for the model */
        return {
            kind:     'recent_messages',
            sub:      sub.name,
            sub_code: sub.code,
            group:    found.group.name || '',
            total:    mb.total_msgs || nums.length,
            shown:    msgs.length,
            messages: msgs
        };
    } finally {
        try { mb.close(); } catch (ce) {}
    }
}

function this_bbs(args) {
    args = args || {};
    var kind = String(args.kind || 'groups').toLowerCase();
    var limit = parseInt(args.limit, 10) || 20;
    if (limit < 1)  limit = 1;
    if (limit > 50) limit = 50;

    switch (kind) {
    case 'groups':
        var groups = [];
        var all_grps = (typeof msg_area != 'undefined' && msg_area.grp_list) || [];
        for (var gi = 0; gi < all_grps.length; gi++) {
            var g = all_grps[gi];
            /* Skip groups whose name is just digits (scratch/test
             * groups in scfg).  Same heuristic as get_bbs_status. */
            if (/^[\d_\-]+$/.test(g.name || '')) continue;
            /* Count only ACCESSIBLE subs for the access user.  A
             * group with zero readable subs is dropped entirely so
             * we don't tease the caller with empty groups. */
            var all_subs = g.sub_list || [];
            var visible = 0;
            for (var si = 0; si < all_subs.length; si++) {
                if (_user_can_read_sub(g, all_subs[si])) visible++;
            }
            if (visible === 0) continue;
            groups.push({
                name:        g.name || '',
                description: g.description || '',
                sub_count:   visible
            });
        }
        return { kind: 'groups', count: groups.length, groups: groups };

    case 'subs':
        if (!args.group) {
            /* Auto-fallback: caller said "what subs are on this BBS"
             * without a group -- return the GROUPS list (which is
             * the answer they actually want as a starting point)
             * with a note explaining how to drill in. */
            var fb_groups = [];
            var fb_all = (typeof msg_area != 'undefined' && msg_area.grp_list) || [];
            for (var fbi = 0; fbi < fb_all.length; fbi++) {
                var fbg = fb_all[fbi];
                if (/^[\d_\-]+$/.test(fbg.name || '')) continue;
                var fbsl = fbg.sub_list || [];
                var fbvis = 0;
                for (var fbsi = 0; fbsi < fbsl.length; fbsi++) {
                    if (_user_can_read_sub(fbg, fbsl[fbsi])) fbvis++;
                }
                if (fbvis === 0) continue;
                fb_groups.push({ name: fbg.name, sub_count: fbvis });
            }
            return {
                kind:   'groups',
                count:  fb_groups.length,
                groups: fb_groups,
                _note:  THIS_BBS_NAME + ' organizes sub-boards into '
                        + fb_groups.length + ' message groups -- the count '
                        + 'is per group.  Name them all (or the top few) as '
                        + 'a single comma-separated sentence; to list the '
                        + 'subs in a specific group, the caller can ask '
                        + 'about that group by name.'
            };
        }
        var grp = _find_group(args.group);
        if (!grp) return { error: 'I do not see a message group called "'
            + args.group + '" on ' + THIS_BBS_NAME + '.' };
        var sub_list = grp.sub_list || [];
        var sub_visible = [];
        for (var si = 0; si < sub_list.length; si++) {
            if (_user_can_read_sub(grp, sub_list[si])) sub_visible.push(sub_list[si]);
        }
        var subs_out = [];
        for (var si2 = 0; si2 < sub_visible.length && subs_out.length < limit; si2++) {
            var sub = sub_visible[si2];
            subs_out.push({
                code:        sub.code || '',
                name:        sub.name || '',
                description: sub.description || ''
            });
        }
        return {
            kind:  'subs',
            group: grp.name,
            count: sub_visible.length,
            shown: subs_out.length,
            subs:  subs_out
        };

    case 'libs':
        var libs_out = [];
        var all_libs = (typeof file_area != 'undefined' && file_area.lib_list) || [];
        for (var li = 0; li < all_libs.length; li++) {
            var lib = all_libs[li];
            var all_dirs = lib.dir_list || [];
            var visible_dirs = 0;
            for (var di = 0; di < all_dirs.length; di++) {
                if (_user_can_read_dir(lib, all_dirs[di])) visible_dirs++;
            }
            if (visible_dirs === 0) continue;
            libs_out.push({
                name:        lib.name || '',
                description: lib.description || '',
                dir_count:   visible_dirs
            });
        }
        return { kind: 'libs', count: libs_out.length, libs: libs_out };

    case 'dirs':
        if (!args.lib) {
            return { error: 'I would need to know which file library '
                + 'you mean before I can list its directories.' };
        }
        var lib_found = _find_lib(args.lib);
        if (!lib_found)
            return { error: 'I do not see a file library called "'
                + args.lib + '" on ' + THIS_BBS_NAME + '.' };
        var dir_list = lib_found.dir_list || [];
        var dir_visible = [];
        for (var di = 0; di < dir_list.length; di++) {
            if (_user_can_read_dir(lib_found, dir_list[di]))
                dir_visible.push(dir_list[di]);
        }
        var dirs_out = [];
        for (var di2 = 0; di2 < dir_visible.length && dirs_out.length < limit; di2++) {
            var d = dir_visible[di2];
            dirs_out.push({
                code:        d.code || '',
                name:        d.name || '',
                description: d.description || ''
            });
        }
        return {
            kind:    'dirs',
            library: lib_found.name,
            count:   dir_visible.length,
            shown:   dirs_out.length,
            dirs:    dirs_out
        };

    case 'door_sections':
        var secs_out = [];
        var all_secs = (typeof xtrn_area != 'undefined' && xtrn_area.sec_list) || [];
        for (var xi = 0; xi < all_secs.length; xi++) {
            var sec = all_secs[xi];
            var all_progs = sec.prog_list || [];
            var visible_progs = 0;
            for (var pi = 0; pi < all_progs.length; pi++) {
                if (_user_can_run_xtrn(sec, all_progs[pi])) visible_progs++;
            }
            if (visible_progs === 0) continue;
            secs_out.push({
                name:          sec.name || '',
                description:   sec.description || '',
                program_count: visible_progs
            });
        }
        return {
            kind:     'door_sections',
            count:    secs_out.length,
            sections: secs_out
        };

    case 'doors':
        if (!args.section) {
            return { error: 'I would need to know which door / external '
                + 'program section you mean before I can list its programs.' };
        }
        var sec = _find_xtrn_section(args.section);
        if (!sec) return { error: 'I do not see a door section called "'
            + args.section + '" on ' + THIS_BBS_NAME + '.' };
        var prog_list = sec.prog_list || [];
        var prog_visible = [];
        for (var pi = 0; pi < prog_list.length; pi++) {
            if (_user_can_run_xtrn(sec, prog_list[pi]))
                prog_visible.push(prog_list[pi]);
        }
        var doors_out = [];
        for (var pi2 = 0; pi2 < prog_visible.length && doors_out.length < limit; pi2++) {
            var p = prog_visible[pi2];
            doors_out.push({
                code:        p.code || '',
                name:        p.name || '',
                description: p.description || ''
            });
        }
        return {
            kind:    'doors',
            section: sec.name,
            count:   prog_visible.length,
            shown:   doors_out.length,
            doors:   doors_out
        };

    case 'recent_messages':
        if (!args.sub) {
            return { error: 'I would need to know which sub-board you '
                + 'mean before I can pull recent messages.' };
        }
        return _recent_messages(args.sub, limit);

    case 'stats':
        return _local_stats();

    default:
        return { error: 'I do not have a way to look that up here.' };
    }
}

llm_tool_register({
    name: 'this_bbs',
    execute: this_bbs,
    def: {
        type: 'function',
        'function': {
            name: 'this_bbs',
            description:
                'Query the LOCAL contents of the host BBS (' + THIS_BBS_NAME
              + ').  "' + THIS_BBS_NAME + '", "this BBS", "us", "our BBS", '
              + '"here", or no BBS name -- all refer to this tool.\n\n'
              + 'kind="stats" -- TODAY\'S live counters (logons/posts/'
              + 'uploads/downloads today) + lifetime totals.  Use for ANY '
              + 'question with "today", "right now", "currently", "this '
              + 'morning".  Triggers: "how many files downloaded today", '
              + '"how many logons today", "how busy today".  NEVER use '
              + 'libs/dirs/groups for activity-count questions.\n\n'
              + 'kind="groups" -- list message groups.  Triggers: "what '
              + 'message groups does <BBS> have?"\n'
              + 'kind="subs"   -- subs in a group (pass group="X"). '
              + 'Triggers: "what subs / sub-boards [are in <group> | does '
              + '<BBS> have | on <group>]?", "list the subs on <group>".\n'
              + 'kind="libs"   -- list file libraries (DOWNLOADABLE '
              + 'files).  Triggers: "what file libraries / file libs", '
              + '"what\'s available for download", "what files are on '
              + '<BBS>".  If the user says "download" or "files", this '
              + 'is the right kind -- NOT door_sections.\n'
              + 'kind="dirs"   -- dirs in a library (pass lib="X"). '
              + 'Triggers: "what dirs / file directories are in <lib>?", '
              + '"what files are in <lib>".\n'
              + 'kind="door_sections" -- list door / xtrn SECTIONS (door '
              + 'categories like Games/Casino/Operator).  Triggers: "what '
              + 'door categories / sections / kinds of doors does <BBS> '
              + 'have?".  Doors are run ONLINE, not downloaded -- if the '
              + 'question mentions "download" or "files", use libs/dirs '
              + 'instead.\n'
              + 'kind="doors"  -- doors in a section (pass section="X").  '
              + 'Triggers: "what door GAMES / programs are on <BBS>?" '
              + '(name actual games, not categories) -- pass section= '
              + '"Games" or whatever section the user implied.\n'
              + 'kind="recent_messages" -- last N msgs on a sub (pass '
              + 'sub="<code or name>").  Triggers: "what\'s been posted on '
              + '<sub> lately?", "any new posts on <sub>?", "what\'s the '
              + 'latest on <sub>?", "recent messages on <sub>".\n\n'
              + 'For ' + THIS_BBS_NAME + ' STATUS questions (is it up, '
              + 'who\'s on, uptime, version, networks, lifetime totals) use '
              + 'bbs_directory(mode:lookup, name:' + THIS_BBS_NAME + ') -- '
              + 'this_bbs is for LIST contents, not status.\n\n'
              + 'Returns: { kind, count, shown, plus a list field named '
              + 'after the kind }.  Default limit 20, max 50.\n\n'
              + 'RESPONSE: name 5-10 items as comma-separated inline prose '
              + '("the libraries here are Main, bbsfiles.com, Music, ..."). '
              + 'NO bullets, numbered lists, or markdown.  Don\'t dump the '
              + 'full list unless explicitly asked.  Always produce SOME '
              + 'prose -- never empty.',
            parameters: {
                type: 'object',
                properties: {
                    kind:    { type: 'string',
                               'enum': ['groups','subs','libs','dirs',
                                        'door_sections','doors',
                                        'recent_messages','stats'] },
                    group:   { type: 'string' },
                    lib:     { type: 'string' },
                    section: { type: 'string' },
                    sub:     { type: 'string' },
                    limit:   { type: 'integer' }
                },
                required: ['kind']
            }
        }
    }
});
