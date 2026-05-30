/*
 * llm_tools/bbs_directory.js -- the bbs_directory tool.
 *
 * Unified directory query against sbbslist.json + live finger probes.
 * Two modes: "lookup" (one BBS) and "list" (filtered set).  Replaced
 * the separate lookup_bbs / list_bbses tools 2026-05-27 -- a single
 * mode-gated tool is much easier on the 7B model than two with
 * overlapping argument names.
 *
 * Depends on _common.js (THIS_BBS_NAME).
 */

/* Small integer + duration helpers used by the finger probes below.
 * Kept private to this file -- only the BBS-directory probing paths
 * use them now that the legacy get_bbs_status snapshot is gone. */
function _safe_int(n) {
    var v = parseInt(n, 10);
    return isNaN(v) ? 0 : v;
}

function _format_duration(secs) {
    secs = _safe_int(secs);
    if (secs <= 0) return 'unknown';
    var d = Math.floor(secs / 86400);
    var h = Math.floor((secs % 86400) / 3600);
    var m = Math.floor((secs % 3600) / 60);
    var parts = [];
    if (d) parts.push(d + 'd');
    if (h) parts.push(h + 'h');
    if (m || !parts.length) parts.push(m + 'm');
    return parts.join(' ');
}

/* sbbslist.json cache.  Parsed once per process; the file changes
 * only on the daily sbbslist verify cycle so re-reading per call is
 * wasteful.  Caller-driven invalidation isn't needed -- bot lifetime
 * is usually < daily cycle. */
var SBBSLIST_CACHE = null;
var SBBSLIST_PATH  = (typeof system != 'undefined' && system.data_dir)
    ? system.data_dir + 'sbbslist.json' : 'data/sbbslist.json';

function _load_sbbslist() {
    if (SBBSLIST_CACHE !== null) return SBBSLIST_CACHE;
    var f = new File(SBBSLIST_PATH);
    if (!f.open('r')) { SBBSLIST_CACHE = []; return SBBSLIST_CACHE; }
    var raw = f.read();
    f.close();
    try { SBBSLIST_CACHE = JSON.parse(raw); }
    catch (e) { SBBSLIST_CACHE = []; }
    if (!Array.isArray(SBBSLIST_CACHE)) SBBSLIST_CACHE = [];
    return SBBSLIST_CACHE;
}

function _find_bbs(query) {
    var list = _load_sbbslist();
    var q = String(query || '').toLowerCase();
    if (!q) return null;
    /* Try exact (case-insensitive) name match first. */
    for (var i = 0; i < list.length; i++) {
        if (String(list[i].name || '').toLowerCase() === q) return list[i];
    }
    /* Then substring match on name. */
    for (var i = 0; i < list.length; i++) {
        if (String(list[i].name || '').toLowerCase().indexOf(q) >= 0)
            return list[i];
    }
    /* Then sysop name (exact then substring). */
    for (var i = 0; i < list.length; i++) {
        var sysops = list[i].sysop || [];
        for (var so = 0; so < sysops.length; so++) {
            if (String(sysops[so].name || '').toLowerCase() === q)
                return list[i];
        }
    }
    for (var i = 0; i < list.length; i++) {
        var sysops = list[i].sysop || [];
        for (var so = 0; so < sysops.length; so++) {
            if (String(sysops[so].name || '').toLowerCase().indexOf(q) >= 0)
                return list[i];
        }
    }
    /* Match on any service address.  Bidirectional substring match
     * so "bbs.endofthelinebbs.com" finds an "endofthelinebbs.com"
     * listing (and vice versa).  Also strip leading "bbs." /
     * "telnet." / "ftp." from the query as a normalization step. */
    var nq = q.replace(/^(bbs|telnet|ftp|ssh)\./, '');
    for (var i = 0; i < list.length; i++) {
        var svcs = list[i].service || [];
        for (var s = 0; s < svcs.length; s++) {
            var addr = String(svcs[s].address || '').toLowerCase();
            if (!addr) continue;
            if (addr === q || addr === nq) return list[i];
            if (addr.indexOf(q) >= 0 || q.indexOf(addr) >= 0) return list[i];
            if (addr.indexOf(nq) >= 0 || nq.indexOf(addr) >= 0) return list[i];
        }
    }
    return null;
}

/* Find ALL BBSes a sysop runs (returns array, may be empty).
 * Substring match on sysop[].name, case-insensitive. */
function _find_bbses_by_sysop(query) {
    var list = _load_sbbslist();
    var q = String(query || '').toLowerCase();
    if (!q) return [];
    var hits = [];
    for (var i = 0; i < list.length; i++) {
        var sysops = list[i].sysop || [];
        for (var so = 0; so < sysops.length; so++) {
            if (String(sysops[so].name || '').toLowerCase().indexOf(q) >= 0) {
                hits.push(list[i]);
                break;
            }
        }
    }
    return hits;
}

/* Try to finger an address for ?active-users.json (Synchronet-specific
 * special query).  Returns parsed JSON array on success, null on
 * failure / timeout / non-Synchronet finger.  Hard 5-second total
 * timeout so chat doesn't hang on slow remotes. */
function _finger_active_users(host) {
    if (!host) return null;
    try {
        var lib = load({}, 'finger_lib.js');
        var lines = lib.request(host, '?active-users.json', 'finger', false);
        if (typeof lines != 'object' || !lines.length) return null;
        var json_str = lines.join('');
        try { var parsed = JSON.parse(json_str); return parsed; }
        catch (e) { return null; }
    } catch (e) { return null; }
}

/* Generic single-line finger probe.  Returns the joined response on
 * success, null on failure / timeout / non-Synchronet finger.  Used
 * for the small text-only special queries (?ver, ?auto.msg, etc.). */
function _finger_text(host, query, max_lines) {
    if (!host) return null;
    try {
        var lib = load({}, 'finger_lib.js');
        var lines = lib.request(host, query, 'finger', false);
        if (typeof lines != 'object' || !lines.length) return null;
        if (max_lines && lines.length > max_lines)
            lines = lines.slice(0, max_lines);
        var text = lines.join('\n');
        /* Strip Synchronet Ctrl-A attribute / color codes that often
         * appear in auto.msg and other display-formatted responses
         * (Ctrl-A followed by one character: 'h' = high intensity,
         * 'c' = cyan, '[' = home, etc.).  We deliberately AVOID the
         * built-in strip_ctrl(), which also removes \n / other control
         * chars and flattens multi-line responses (?ver, etc.) into
         * one run-on line.  Targeted regex below preserves \n. */
        text = text.replace(/\x01./g, '').replace(/\x01/g, '');
        text = text.replace(/^\s+|\s+$/g, '');
        return text || null;
    } catch (e) { return null; }
}

/* Query a Synchronet BBS's stats via finger ?stats.json.
 * Returns the parsed system.stats object on success (lifetime totals
 * and today's counts: logons_today, messages_posted_today,
 * files_uploaded_today, bytes_uploaded_today, etc.), null otherwise. */
function _finger_stats(host) {
    if (!host) return null;
    try {
        var lib = load({}, 'finger_lib.js');
        var lines = lib.request(host, '?stats.json', 'finger', false);
        if (typeof lines != 'object' || !lines.length) return null;
        try { return JSON.parse(lines.join('')); }
        catch (e) { return null; }
    } catch (e) { return null; }
}

/* Query a Synchronet BBS's uptime via finger ?uptime.
 * Response format (4 lines):
 *   "Duration: <seconds>"
 *   "<unix-ts-of-start>"
 *   "<human start time>"
 *   "<human current time>"
 * Returns { seconds, started_at, queried_at } or null on failure. */
function _finger_uptime(host) {
    if (!host) return null;
    try {
        var lib = load({}, 'finger_lib.js');
        var lines = lib.request(host, '?uptime', 'finger', false);
        if (typeof lines != 'object' || !lines.length) return null;
        var dur_match = String(lines[0] || '').match(/Duration:\s*(\d+)/i);
        if (!dur_match) return null;
        var secs = _safe_int(dur_match[1]);
        if (secs <= 0) return null;
        return {
            seconds:    secs,
            human:      _format_duration(secs),
            started_at: lines[2] || '',
            queried_at: lines[3] || ''
        };
    } catch (e) { return null; }
}

function lookup_bbs(query) {
    var entry = _find_bbs(query);
    if (!entry) {
        return {
            query: query,
            found: false,
            note: 'No BBS matching "' + query + '" in sbbslist.json -- '
                + 'this is the Synchronet-curated BBS directory; if the '
                + 'BBS isn\'t listed there I have no info on it.'
        };
    }

    var out = {
        query: query,
        found: true,
        name: entry.name || '',
        sysop: (entry.sysop || []).map(function (s) { return s.name; }).join(', '),
        location: entry.location || '',
        software: entry.software || '',
        description: (entry.description || []).join(' '),
        web_site: entry.web_site || '',
        first_online: entry.first_online || '',
        services: (entry.service || []).map(function (s) {
            return {
                protocol: s.protocol,
                address:  s.address,
                port:     s.port
            };
        }),
        terminal: entry.terminal || null,
        networks: (entry.network || []).map(function (n) {
            return n.name + (n.address ? ' (' + n.address + ')' : '');
        }),
        /* Nightly autoverify summary -- the sbbslist verifier probes
         * each entry's services nightly and tracks lifetime success/
         * failure counts plus the most recent success and failure.
         * Lets the model answer "is X up?" / "is X reliable?" /
         * "when was X last reachable?" without doing a live finger
         * probe (and for non-Synchronet BBSes where finger isn't
         * available). */
        verify_status:
            (entry.entry && entry.entry.autoverify)
                ? (entry.entry.autoverify.success ? 'up' : 'down')
                : 'unknown',
        verify_last_success:
            (entry.entry && entry.entry.autoverify
             && entry.entry.autoverify.last_success
             && entry.entry.autoverify.last_success.on) || '',
        verify_last_failure:
            (entry.entry && entry.entry.autoverify
             && entry.entry.autoverify.last_failure
             && entry.entry.autoverify.last_failure.on) || '',
        verify_last_failure_reason:
            (entry.entry && entry.entry.autoverify
             && entry.entry.autoverify.last_failure
             && entry.entry.autoverify.last_failure.result) || '',
        verify_reliability:
            (function () {
                var av = (entry.entry && entry.entry.autoverify) || {};
                if (!av.attempts) return null;
                return {
                    attempts:  av.attempts,
                    successes: av.successes,
                    ratio:     +(av.successes / av.attempts).toFixed(3)
                };
            })(),
        /* Cached stats from the last sbbslist verification (daily) --
         * not real-time, but reasonable for "how many users / files /
         * messages does X BBS have?" type questions when we can't
         * afford the 14s ?stats.json finger probe per lookup. */
        cached_stats: entry.total || null,
        cached_stats_asof: (entry.entry && entry.entry.verified
                          && entry.entry.verified.on) || ''
    };

    /* Live finger probe -- ONLY for Synchronet BBSes (the
     * ?active-users.json query is Synchronet-specific).  Find a
     * finger service if listed; otherwise try a known telnet/ssh
     * address as the host.  Don't probe if software doesn't say
     * Synchronet -- the query will just hang/error.
     *
     * Also skip when the nightly autoverify says the BBS is currently
     * DOWN -- the verifier already paid the connect-timeout cost last
     * night, and a fresh ~5s probe on top of that is wasted time per
     * lookup.  Mark the absence so the model can say "down per last
     * verify" instead of "probe results unknown".  out.verify_status
     * was set above from the same data; the model can cite it. */
    var is_synchronet = /synchronet/i.test(entry.software || '');
    if (is_synchronet && out.verify_status === 'down') {
        out.online_query_result = 'skipped live probe -- '
            + 'nightly autoverify last marked this BBS as '
            + 'unreachable (verify_status=down).  See '
            + 'verify_last_failure for the last failure mode and '
            + 'verify_last_success for when it was last reachable.';
    } else if (is_synchronet) {
        /* Find a host to probe.  Prefer an explicit "finger" entry in
         * the services list (rare -- only ~1/235 sbbslist entries
         * actually list it), otherwise fall back to the host of any
         * telnet/ssh/rlogin listener (finger on port 79 of the same
         * host is the Synchronet default).  Non-Synchronet BBSes
         * don't speak the special finger queries, so we skip them
         * entirely.  The ?ver query at the top of the probe chain
         * acts as the early-exit if the host doesn't actually run
         * finger -- one ~3-5s timeout, then we skip the rest. */
        var finger_host = '';
        var svcs = entry.service || [];
        for (var s = 0; s < svcs.length; s++) {
            if ((svcs[s].protocol || '').toLowerCase() === 'finger') {
                finger_host = svcs[s].address;
                break;
            }
        }
        if (!finger_host) {
            for (var s = 0; s < svcs.length; s++) {
                var p = (svcs[s].protocol || '').toLowerCase();
                if (p == 'telnet' || p == 'ssh' || p == 'rlogin') {
                    finger_host = svcs[s].address;
                    break;
                }
            }
        }
        if (finger_host) {
            /* Probe order matters: try the cheapest query first.  If
             * ?ver fails outright (BBS down / no finger / firewalled /
             * not Synchronet), early-exit before paying timeouts on
             * the other 4 queries.  Each query is bounded by finger's
             * socket timeout, but 5x bounded is still annoying if the
             * remote is unresponsive. */
            var ver = _finger_text(finger_host, '?ver', 5);
            if (!ver) {
                out.online_query_result = 'finger to ' + finger_host
                    + ' failed -- BBS may be offline, behind a firewall, '
                    + 'or not running a Synchronet finger service';
            } else {
                out.synchronet_version = ver;
                /* ?uptime */
                var up = _finger_uptime(finger_host);
                if (up) {
                    out.bbs_uptime_seconds = up.seconds;
                    out.bbs_uptime_human   = up.human;
                    out.bbs_started_at     = up.started_at;
                }
                /* ?active-users.json */
                var users = _finger_active_users(finger_host);
                if (users && users.length !== undefined) {
                    out.online_callers = users.map(function (u) {
                        return {
                            alias:      u.name || '',
                            doing:      u.action || '',
                            node:       u.node || 0,
                            connection: u.prot || ''
                        };
                    });
                    out.online_count = users.length;
                }
                /* Dropped from default probe set:
                 *   - ?stats.json (worst per-call latency at ~14s
                 *     vs ~3s for the others; total/lifetime counts
                 *     are already in sbbslist.json's entry.total
                 *     field, exposed below as cached_stats, just
                 *     not real-time current)
                 *   - ?auto.msg (rare query, +1s)
                 *   - ?logon.lst (legacy/deprecated file format)
                 * If a caller specifically asks about today's
                 * activity (logons_today / posts_today / etc.),
                 * the bot has to say it doesn't have those live
                 * for other BBSes -- the daily-cached totals are
                 * the available info. */
            }
        }
    }
    return out;
}

/* Compact view of a BBS record for list responses -- one row per
 * BBS, just the headline fields, so the model can summarize lists
 * without choking on 235x full records. */
function _compact_bbs(entry) {
    var primary_addr = '';
    var svcs = entry.service || [];
    for (var s = 0; s < svcs.length; s++) {
        var p = (svcs[s].protocol || '').toLowerCase();
        if (p == 'telnet' || p == 'ssh' || p == 'rlogin') {
            primary_addr = svcs[s].address + ':' + svcs[s].port + ' (' + p + ')';
            break;
        }
    }
    return {
        name:     entry.name || '',
        sysop:    (entry.sysop || []).map(function (s) { return s.name; }).join(', '),
        location: entry.location || '',
        software: entry.software || '',
        address:  primary_addr,
        first_online: entry.first_online || '',
        users:    (entry.total && entry.total.users) || null
    };
}

/* list_bbses({ kind, limit, software?, network? })
 *
 * Browse-style query over the sbbslist directory.  Used for "what
 * BBSes does <sysop> run?", "what BBSes are new?", "which BBSes are
 * on FidoNet?", "what's the most popular BBS?", etc.  Returns an
 * array (capped at `limit`) of compact BBS records.  All filtering
 * is done locally against sbbslist.json -- no finger probing (would
 * be too slow for 235 BBSes). */
function list_bbses(args) {
    args = args || {};
    var kind  = String(args.kind  || 'recent').toLowerCase();
    var limit = parseInt(args.limit, 10) || 10;
    if (limit < 1) limit = 1;
    if (limit > 30) limit = 30;
    var filter_software = args.software ? String(args.software).toLowerCase() : '';
    var filter_network  = args.network  ? String(args.network).toLowerCase()  : '';
    var filter_sysop    = args.sysop    ? String(args.sysop).toLowerCase()    : '';

    var list = _load_sbbslist();
    /* Pre-filter by software / network / sysop if specified. */
    var pool = [];
    for (var i = 0; i < list.length; i++) {
        var e = list[i];
        if (filter_software
            && String(e.software || '').toLowerCase().indexOf(filter_software) < 0)
            continue;
        if (filter_network) {
            var nets = e.network || [];
            var nm = false;
            for (var n = 0; n < nets.length; n++) {
                if (String(nets[n].name || '').toLowerCase().indexOf(filter_network) >= 0) {
                    nm = true; break;
                }
            }
            if (!nm) continue;
        }
        if (filter_sysop) {
            var sysops = e.sysop || [];
            var sm = false;
            for (var so = 0; so < sysops.length; so++) {
                if (String(sysops[so].name || '').toLowerCase().indexOf(filter_sysop) >= 0) {
                    sm = true; break;
                }
            }
            if (!sm) continue;
        }
        pool.push(e);
    }

    /* Sort by the requested kind. */
    switch (kind) {
    case 'recent':
        /* Most-recently first-online or entry-created.  Use the
         * later of the two; fall back to whichever exists. */
        pool.sort(function (a, b) {
            var ad = (a.first_online || (a.entry && a.entry.created && a.entry.created.on) || '');
            var bd = (b.first_online || (b.entry && b.entry.created && b.entry.created.on) || '');
            return bd > ad ? 1 : (bd < ad ? -1 : 0);
        });
        break;
    case 'oldest':
        /* Earliest first_online -- this is the "long-running BBS"
         * ranking (Vertrauen at 1988, etc.). */
        pool.sort(function (a, b) {
            var ad = a.first_online || '9999';
            var bd = b.first_online || '9999';
            return ad > bd ? 1 : (ad < bd ? -1 : 0);
        });
        break;
    case 'active':
        /* Recently verified by the autoverify cycle (proxy for "this
         * BBS is currently online and reachable"). */
        pool.sort(function (a, b) {
            var ad = (a.entry && a.entry.autoverify && a.entry.autoverify.last_success
                      && a.entry.autoverify.last_success.on) || '';
            var bd = (b.entry && b.entry.autoverify && b.entry.autoverify.last_success
                      && b.entry.autoverify.last_success.on) || '';
            return bd > ad ? 1 : (bd < ad ? -1 : 0);
        });
        break;
    case 'reliable':
        /* Highest autoverify success ratio.  Proxy for "longest-
         * running, most consistently online" -- doesn't measure
         * popularity, just uptime/reachability over the verify
         * history. */
        pool.sort(function (a, b) {
            var av = (a.entry && a.entry.autoverify) || {};
            var bv = (b.entry && b.entry.autoverify) || {};
            var ar = av.attempts ? av.successes / av.attempts : 0;
            var br = bv.attempts ? bv.successes / bv.attempts : 0;
            return br - ar;
        });
        break;
    case 'name':
        pool.sort(function (a, b) {
            return String(a.name || '').toLowerCase()
                 < String(b.name || '').toLowerCase() ? -1 : 1;
        });
        break;
    case 'popular':
    case 'users':
    case 'busiest':
        /* Most users.  sbbslist entries carry self-reported lifetime
         * totals (entry.total.users); sort descending.  Answers "which
         * BBS has the most users?" / "what's the most popular BBS?". */
        pool.sort(function (a, b) {
            var au = (a.total && a.total.users) || 0;
            var bu = (b.total && b.total.users) || 0;
            return bu - au;
        });
        break;
    default:
        /* Unknown kind -- fall through with no sort.  Caller will
         * see the order it came in (file order). */
        break;
    }

    return {
        kind:    kind,
        limit:   limit,
        total:   pool.length,
        filters: {
            software: filter_software || null,
            network:  filter_network  || null,
            sysop:    filter_sysop    || null
        },
        bbses:   pool.slice(0, limit).map(_compact_bbs)
    };
}

/* ------------------ Unified directory tool ------------------ */

/* bbs_directory({mode, name?, kind?, limit?, software?, network?, sysop?})
 *
 * Replaces the two separate lookup_bbs / list_bbses tools.  Branches on
 * mode ("lookup" or "list") to either lookup_bbs() or list_bbses().
 * mode is REQUIRED; missing or unknown mode returns a prose error so
 * the model gets clear feedback instead of silent failure.
 *
 * Enforces arg discipline: mode="lookup" rejects list-only args
 * (kind/limit/software/network/sysop); mode="list" rejects the "name"
 * arg.  When the model passes a wrong-mode arg, the response tells it
 * what to fix.
 */
function bbs_directory(args) {
    args = args || {};
    var mode = String(args.mode || '').toLowerCase();

    /* Infer mode if the model forgot it or invented an invalid one.
     * qwen2.5:7b regularly drops "mode" and passes "name"/"query"
     * or filter args alone, OR invents modes like "search" / "find"
     * that don't exist in the enum.  This is much friendlier than
     * refusing with a generic prose error.
     *
     * Heuristic:
     *   - name alone (no list-style filters) -> lookup
     *   - any list filter present -> list (network/software/sysop/kind)
     *   - query alone, no other hint -> list (promote query to
     *     a network filter; the model usually means "BBSes about X")
     */
    if (mode !== 'lookup' && mode !== 'list') {
        if (args.name && !(args.network || args.software || args.sysop)) {
            mode = 'lookup';
        } else if (args.network || args.software || args.sysop
                   || args.kind   || args.limit) {
            mode = 'list';
        } else if (args.query) {
            /* "search FidoNet" with mode=search -- promote query
             * to network and use list mode. */
            if (!args.network) args.network = args.query;
            mode = 'list';
        }
    }

    if (!mode) {
        return {
            error: 'I need to know which mode -- "lookup" for one '
                 + 'specific BBS (pass name) or "list" for a filtered '
                 + 'list (pass kind/network/software/sysop).'
        };
    }

    if (mode === 'lookup') {
        var name = String(args.name || args.query || '').replace(/^\s+|\s+$/g, '');
        if (!name) {
            return {
                error: 'lookup mode needs a "name" argument (a BBS name '
                     + 'or hostname).  To filter by network / software / '
                     + 'sysop instead, pass mode="list".'
            };
        }
        /* Flag wrong-mode args explicitly so the model knows what to '
         * fix on retry. */
        if (args.kind || args.limit || args.software
            || args.network || args.sysop) {
            return {
                error: 'lookup mode takes ONLY "name".  You also passed '
                     + 'list-mode args (kind/limit/software/network/'
                     + 'sysop) -- drop those, or switch to mode="list" '
                     + 'if you actually want a filtered list.'
            };
        }
        return lookup_bbs(name);
    }

    if (mode === 'list') {
        if (args.name) {
            return {
                error: 'list mode does not take a "name" arg.  To look '
                     + 'up ONE specific BBS, use mode="lookup" with the '
                     + 'name there instead.  To filter the list to BBSes '
                     + 'whose sysop name contains a string, use the '
                     + '"sysop" arg.'
            };
        }
        return list_bbses({
            kind:     args.kind,
            limit:    args.limit,
            software: args.software,
            network:  args.network,
            sysop:    args.sysop
        });
    }

    return {
        error: 'unknown mode "' + args.mode + '" -- valid modes are '
             + '"lookup" (one BBS by name) or "list" (filtered list).'
    };
}

llm_tool_register({
    name: 'bbs_directory',
    execute: bbs_directory,
    def: {
        type: 'function',
        'function': {
            name: 'bbs_directory',
            description:
                'Query the Synchronet BBS directory (sbbslist).  Two modes:\n\n'
              + 'mode="lookup" -- ONE specific BBS by name, sysop name, or '
              + 'hostname.  Args: { mode, name }.  Returns full record + '
              + 'live finger probe (online_callers, online_count, '
              + 'bbs_uptime_human, synchronet_version) for Synchronet '
              + 'BBSes, PLUS nightly autoverify summary (verify_status: '
              + 'up/down/unknown, verify_last_success, verify_last_'
              + 'failure, verify_last_failure_reason, verify_reliability: '
              + '{attempts, successes, ratio}) -- works for ALL listed '
              + 'BBSes, not just Synchronet.\n'
              + 'Triggers: "tell me about <BBS>", "is <BBS> up?", "is '
              + '<BBS> reliable?", "how reliable is <BBS>?", "when was '
              + '<BBS> last seen?", "who\'s on <BBS>?", "<BBS> uptime", '
              + '"what BBS is at <hostname>?".\n\n'
              + 'mode="list" -- filtered list of BBSes.  Args: { mode, kind?, '
              + 'limit?, software?, network?, sysop? } -- all filters '
              + 'optional, combine freely.  kind values: "recent" (default), '
              + '"oldest", "active", "reliable", "popular" (most users), '
              + '"name".  Default limit 10, '
              + 'max 30.  Triggers: "what BBSes does <sysop> run?", "what '
              + 'BBSes are on <network>?", "what BBSes run <software>?", '
              + '"what are the [recent/oldest/active] BBSes?", "which BBS '
              + 'has the most users?" / "what\'s the most popular BBS?" -> '
              + 'kind:"popular" (read users field).\n\n'
              + 'For COUNT questions ("how many BBSes [total / on FidoNet '
              + '/ running Mystic]") call list mode with the matching '
              + 'filter and read the "total" field -- it\'s the count '
              + 'BEFORE truncation to limit.  Ignore bbses[] if you only '
              + 'need the count.  EXAMPLES:\n'
              + '  "how many Synchronet BBSes?" -> mode:"list", '
              + 'software:"Synchronet" -> answer is total field.\n'
              + '  "how many on FidoNet?"       -> mode:"list", '
              + 'network:"FidoNet"   -> answer is total field.\n'
              + '  Without the filter the count is wrong (it returns '
              + 'the total of ALL BBSes in the directory).\n\n'
              + 'CRITICAL ARG DISCIPLINE: name is ONLY for lookup mode.  '
              + 'kind/limit/software/network/sysop are ONLY for list mode.  '
              + 'Mixing args between modes is the #1 cause of failure.\n\n'
              + 'HOST BBS (' + THIS_BBS_NAME + ') ROUTING:\n'
              + '  - lookup mode: status, uptime, who\'s on, what version, '
              + 'networks, software, lifetime totals (cached_stats).\n'
              + '  - this_bbs(stats): today\'s live counters (downloads '
              + 'today, logons today, posts today).\n'
              + '  - this_bbs(subs/libs/dirs/doors): list local contents.\n\n'
              + 'RESPONSE: cite only fields relevant to the question; don\'t '
              + 'recite the whole record.  found:false means the BBS isn\'t '
              + 'in the directory.  For list results, name 5-7 BBSes inline '
              + 'as comma-separated prose with sysop/location.  No bullets, '
              + 'numbered lists, or markdown.',
            parameters: {
                type: 'object',
                properties: {
                    mode: {
                        type: 'string',
                        'enum': ['lookup', 'list'],
                        description: 'lookup = one BBS by name; list = filtered/sorted list'
                    },
                    name: {
                        type: 'string',
                        description: 'BBS name or hostname (ONLY for mode=lookup).'
                    },
                    kind: {
                        type: 'string',
                        'enum': ['recent','oldest','active','reliable','popular','name'],
                        description: 'Sort order (ONLY for mode=list).'
                    },
                    limit:    { type: 'integer', description: '1-30, default 10 (mode=list only).' },
                    software: { type: 'string',  description: 'e.g. "Synchronet", "Mystic" (mode=list only).' },
                    network:  { type: 'string',  description: 'e.g. "FidoNet", "DOVE-Net" (mode=list only).' },
                    sysop:    { type: 'string',  description: 'sysop-name substring (mode=list only).' }
                },
                required: ['mode']
            }
        }
    }
});
