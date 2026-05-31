/*
 * chat_llm_routes.js -- STOCK deterministic intent routers for chat_llm.js.
 *
 * The engine (chat_llm.js) ships no routing of its own; this module supplies
 * the generic/stock-tool routing and registers it via the engine's
 * register_intent_router() hook.  chat_llm.ini's `intent_routers` key names
 * the modules to load (stock installs: just this one).  Site-specific routing
 * (e.g. Synchronet's GitLab git_* tools on Vertrauen) lives in a separate
 * local module loaded alongside this one.
 *
 * Uses the engine globals _strip_bbs_suffix() / _looks_like_proper_noun() /
 * _looks_like_network() / _looks_like_os() (kept in chat_llm.js as shared
 * text utilities).
 */

(function () {
    /* BBS-directory lookups / listings (the global Synchronet BBS directory). */
    function stock_bbs_router(s) {
        var m;

        /* --- bbs_directory (lookup mode) for specific-BBS queries --- */

        /* Strip-and-test approach for "tell me about <X> BBS"-style
         * queries.  Try each lead-in pattern; if one matches, consume it
         * and the remainder is the candidate name.  Only fires when an
         * ACTUAL lead-in was consumed -- otherwise we'd misclassify any
         * input containing "BBS" + a trailing "?" as a lookup. */
        var LEAD_PATTERNS = [
            /^\s*tell me\s+(?:everything|all|anything|stuff)?\s*(?:you know\s+)?(?:about|of|on|regarding)\s+/i,
            /^\s*what(?:['']?s|\s+is)?\s+(?:do you know|can you tell me)\s+(?:about|of|on)\s+/i,
            /^\s*what\s+(?:info|information|details)\s+(?:do you have\s+)?(?:about|on)\s+/i,
            /^\s*(?:give me|gimme|share|give|provide)\s+(?:info|information|details)?\s*(?:about|on)\s+/i,
            /^\s*(?:info|information|details|description|anything)\s+(?:about|on|of)\s+/i,
            /^\s*describe\s+(?:me\s+)?/i,
            /^\s*about\s+/i
        ];
        var stripped = '';
        var any_leadin_stripped = false;
        for (var li = 0; li < LEAD_PATTERNS.length; li++) {
            if (LEAD_PATTERNS[li].test(s)) {
                stripped = s.replace(LEAD_PATTERNS[li], '');
                any_leadin_stripped = true;
                break;
            }
        }
        if (any_leadin_stripped) {
            stripped = stripped
                .replace(/\s*[,;]?\s*(?:please|pls|plz|thanks|thank you|ty|if you can|if you could|if you don['']?t mind|if possible|when you can)\s*[?.!]*\s*$/i, '')
                .replace(/[?.!,]+\s*$/g, '')
                .replace(/^\s+|\s+$/g, '');
            if (stripped.length > 0 && /\bbbs\b/i.test(s)) {
                var name = _strip_bbs_suffix(stripped);
                if (name && _looks_like_proper_noun(name)) {
                    return { tool: 'bbs_directory',
                             args: { mode: 'lookup', name: name } };
                }
            }
        }

        /* "lookup <X>" / "look up <X>" (no BBS keyword required -- the
         * verb implies directory lookup) */
        m = s.match(/^\s*look(?:ing)?[\s-]*up\s+(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var nm = _strip_bbs_suffix(m[1]);
            if (nm) return { tool: 'bbs_directory',
                             args: { mode: 'lookup', name: nm } };
        }

        /* "who runs <X>" / "who's the sysop of <X>" */
        m = s.match(/^\s*who(?:['']?s|\s+is)?\s+(?:runs?|the\s+sysop\s+of)\s+(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var nA = _strip_bbs_suffix(m[1]);
            if (nA && _looks_like_proper_noun(nA))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: nA } };
        }

        /* "which/what BBS(es) does <sysop> run/operate?" -- sysop -> BBS
         * direction (the inverse of "who runs <BBS>?" above).  list mode
         * with a sysop filter returns the BBS(es) that sysop runs. */
        m = s.match(/^\s*(?:which|what)\s+bbs(?:es)?\s+(?:does|do)\s+(.+?)\s+(?:runs?|operates?|manages?)\b/i);
        if (m) {
            var sop = m[1].replace(/^\s+|\s+$/g, '');
            if (sop && _looks_like_proper_noun(sop))
                return { tool: 'bbs_directory',
                         args: { mode: 'list', sysop: sop, limit: 5 } };
        }
        /* "what BBS(es) is/are run/operated by <sysop>" (passive form) */
        m = s.match(/\bbbs(?:es)?\s+(?:is\s+|are\s+)?(?:run|operated|managed)\s+by\s+(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var sop2 = m[1].replace(/^\s+|\s+$/g, '');
            if (sop2 && _looks_like_proper_noun(sop2))
                return { tool: 'bbs_directory',
                         args: { mode: 'list', sysop: sop2, limit: 5 } };
        }

        /* "is <X> [still] up/around/online/alive/reachable/reliable" --
         * status / reliability check.  Both feed bbs_directory(lookup),
         * which returns the autoverify summary for any answer. */
        m = s.match(/^\s*is\s+(.+?)\s+(?:still\s+)?(?:up|online|alive|around|running|reachable|reliable|down|dead|offline)\s*[?.!]*\s*$/i);
        if (m) {
            var n2 = _strip_bbs_suffix(m[1]);
            if (n2 && _looks_like_proper_noun(n2))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n2 } };
        }
        /* "how reliable is <X>" / "when was <X> last seen / verified /
         * reachable" -- reliability / last-seen queries. */
        m = s.match(/^\s*how\s+reliable\s+is\s+(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var n2b = _strip_bbs_suffix(m[1]);
            if (n2b && _looks_like_proper_noun(n2b))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n2b } };
        }
        m = s.match(/^\s*when\s+was\s+(.+?)\s+(?:last\s+)?(?:seen|reached|reachable|verified|online|up)\s*[?.!]*\s*$/i);
        if (m) {
            var n2c = _strip_bbs_suffix(m[1]);
            if (n2c && _looks_like_proper_noun(n2c))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n2c } };
        }

        /* "what's the [current] uptime/version/status of <X>" --
         * apostrophe optional, one optional adjective allowed. */
        m = s.match(/^\s*what(?:['']?s|\s+is)\s+(?:the\s+)?(?:current|latest|recent|live)?\s*(?:uptime|version|status|sysop)\s+of\s+(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var n3 = _strip_bbs_suffix(m[1]);
            if (n3) return { tool: 'bbs_directory',
                             args: { mode: 'lookup', name: n3 } };
        }
        /* "what is <BBS>'s [current] uptime/version/status" (possessive form) */
        m = s.match(/^\s*what(?:['']?s|\s+is)\s+(\S+?)(?:['']s)?\s+(?:current|latest|recent|live)?\s*(?:uptime|version|status|sysop)\s*[?.!]*\s*$/i);
        if (m) {
            var n3b = _strip_bbs_suffix(m[1]);
            if (n3b && _looks_like_proper_noun(n3b))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n3b } };
        }

        /* "who's online at/on/in <X>" / "who's on <X>" -- connector
         * preposition is optional after "online" (covers "online on
         * <X>") and required after bare "on". */
        m = s.match(/^\s*who(?:['']?s|\s+is)?\s+online\s+(?:at|on|in)?\s*([\w.][\w\s.'-]*?)(?:\s+(?:right\s+now|now|currently|at the moment))?\s*[?.!]*\s*$/i);
        if (m) {
            var n4 = _strip_bbs_suffix(m[1]);
            if (n4 && _looks_like_proper_noun(n4))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n4 } };
        }
        m = s.match(/^\s*who(?:['']?s|\s+is)?\s+on\s+(?:at|in)?\s*([\w.][\w\s.'-]*?)(?:\s+(?:right\s+now|now|currently|at the moment))?\s*[?.!]*\s*$/i);
        if (m) {
            var n4b = _strip_bbs_suffix(m[1]);
            if (n4b && _looks_like_proper_noun(n4b))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n4b } };
        }

        /* "<verb> the BBS [with the] [host]name X" / "BBS at <hostname>"
         * -- treat the hostname as the lookup key.  Hostnames are usually
         * the lookup-by-hostname path (bbs_directory's lookup mode
         * already accepts hostname or BBS name interchangeably). */
        m = s.match(/(?:the\s+)?bbs\s+(?:with\s+(?:the\s+)?(?:host)?name|at|on)\s+([\w][\w.-]*\.[\w][\w.-]*)/i);
        if (m) {
            return { tool: 'bbs_directory',
                     args: { mode: 'lookup', name: m[1] } };
        }

        /* "what users are online on <X> [right now]" */
        m = s.match(/^\s*what\s+users\s+are\s+online\s+(?:at|on|in)\s+([\w.][\w\s.'-]*?)(?:\s+(?:right\s+now|now|currently|at the moment))?\s*[?.!]*\s*$/i);
        if (m) {
            var n5 = _strip_bbs_suffix(m[1]);
            if (n5 && _looks_like_proper_noun(n5))
                return { tool: 'bbs_directory',
                         args: { mode: 'lookup', name: n5 } };
        }

        /* --- bbs_directory (list mode) for BBS lists by network/software --- */

        /* "how many/which/what BBSes (are) run/running/on <OS>" -- OS filter.
         * Per-BBS OS comes from the autoverify finger result; bbs_directory's
         * os arg filters on it.  Gated by _looks_like_os and checked BEFORE
         * the software/network rules below so "how many Synchronet BBSes run
         * FreeBSD" / "what BBSes are on Linux" filter by the OS, not by a
         * companion software/network token. */
        m = s.match(/\bbbses?\s+(?:are\s+)?(?:run(?:ning)?|on|under|using)\s+([A-Za-z][\w.\/ -]*?)\s*[?.!]*\s*$/i);
        if (m && _looks_like_os(m[1])) {
            return { tool: 'bbs_directory',
                     args: { mode: 'list', os: m[1].replace(/^\s+|\s+$/g, ''), limit: 5 } };
        }

        /* "what BBSes are on <network>" / "list BBSes on <X>" / "BBSes on <X>" */
        m = s.match(/(?:what|which|list)\s+bbses?\s+(?:are\s+)?(?:on|in|using|with|via)\s+(\w[\w.-]*)/i);
        if (m) {
            var which = m[1];
            var key = _looks_like_network(which) ? 'network' : 'software';
            var args = { mode: 'list', limit: 10 };
            args[key] = which;
            return { tool: 'bbs_directory', args: args };
        }
        m = s.match(/(?:what|which|list)\s+bbses?\s+(?:run|running)\s+(\w[\w.-]*)/i);
        if (m) {
            return { tool: 'bbs_directory',
                     args: { mode: 'list', software: m[1], limit: 10 } };
        }

        /* "how many <X> BBSes" / "how many BBSes [on|run] <X>" -- count */
        m = s.match(/how many\s+(\w[\w.-]*)\s+bbses?/i);
        if (m && !/^(?:active|local|recent|total|new)$/i.test(m[1])) {
            var w = m[1];
            var k = _looks_like_network(w) ? 'network' : 'software';
            var a = { mode: 'list', limit: 1 };
            a[k] = w;
            return { tool: 'bbs_directory', args: a };
        }
        m = s.match(/how many\s+bbses?\s+(?:on|in|using|with|run|running)\s+(\w[\w.-]*)/i);
        if (m) {
            var w2 = m[1];
            var k2 = _looks_like_network(w2) ? 'network' : 'software';
            var a2 = { mode: 'list', limit: 1 };
            a2[k2] = w2;
            return { tool: 'bbs_directory', args: a2 };
        }

        /* "how many BBSes [are there|listed|in the list/directory/database]"
         * -- UNFILTERED total count.  Must come AFTER the filtered
         * "how many <X> bbses" / "how many bbses on <X>" rules above so a
         * network/software filter always wins; only bare totals reach here.
         * list mode with no filter returns total = full directory size. */
        if (/how many\s+bbses?\s+(?:are\s+)?(?:there|listed|known|tracked|registered|in\s+total|total|in\s+(?:the\s+)?(?:synchronet\s+)?(?:bbs\s+)?(?:list|directory|database))\b/i.test(s)) {
            return { tool: 'bbs_directory', args: { mode: 'list', limit: 1 } };
        }

        return null;
    }

    /* The host BBS's own contents/stats, IRC relay, and BBS-history archives. */
    function stock_misc_router(s) {
        var m;

        /* --- this_bbs (host BBS contents / stats) --- */

        /* Detect references to the host BBS by name or pronoun.  Used
         * to gate this_bbs patterns -- "what subs on Vertrauen" routes
         * here, but "what subs on FidoNet" should NOT (FidoNet is a
         * network, not the host BBS). */
        var THIS_BBS_LC = (typeof system != 'undefined' && system.name)
                          ? String(system.name).toLowerCase()
                          : 'this bbs';
        function _refs_host(text) {
            var lc = text.toLowerCase();
            return lc.indexOf(THIS_BBS_LC) >= 0
                || /\b(?:this bbs|our bbs|us here|here\s*\?)\b/i.test(text);
        }

        /* "how many users / files / posts / emails / logons today" /
         * "today's stats" / "current stats" -- this_bbs(stats) covers
         * live counters even for the host BBS without naming it. */
        if (/^\s*(?:how many|how much|what(?:['']?s)?)\s+(?:are\s+)?(?:the\s+)?(?:total\s+)?(?:users|logons|callers|posts|messages|emails|files|downloads|uploads|feedbacks?)\s+(?:(?:were|was|got|have\s+been|has\s+been)\s+)?(?:downloaded|uploaded|sent|posted|today|this\s+(?:morning|afternoon|evening|day))/i.test(s)
            || /^\s*(?:today['']?s|current|live)\s+(?:stats|counters|activity|numbers)/i.test(s)) {
            return { tool: 'this_bbs', args: { kind: 'stats' } };
        }

        /* "what subs / sub-boards" on <host>" -> this_bbs(subs) */
        m = s.match(/^\s*(?:what|which|list|show)\s+(?:are\s+the\s+)?(?:message\s+)?sub(?:-?board)?s?\s+(?:on|in|are\s+(?:in|on))\s+(.+?)\s*[?.!]*\s*$/i);
        if (m && _refs_host(m[1])) {
            return { tool: 'this_bbs', args: { kind: 'subs' } };
        }
        /* "what subs does <host> have" / "what subs are there?" */
        m = s.match(/^\s*(?:what|which|list|show)\s+(?:message\s+)?sub(?:-?board)?s?\s+(?:does|do)\s+(.+?)\s+have/i);
        if (m && _refs_host(m[1])) {
            return { tool: 'this_bbs', args: { kind: 'subs' } };
        }

        /* "what (file )?libraries / libs / dirs on <host>" */
        m = s.match(/^\s*(?:what|which|list|show)\s+(?:file\s+)?(libraries|libs|directories|dirs)\s+(?:on|are\s+(?:in|on)|does|do)\s+(.+?)(?:\s+have)?\s*[?.!]*\s*$/i);
        if (m && _refs_host(m[2])) {
            var kind_l = /(libraries|libs)/i.test(m[1]) ? 'libs' : 'dirs';
            return { tool: 'this_bbs', args: { kind: kind_l } };
        }

        /* "what (door |external program )?<X> on <host>" -- door_sections */
        m = s.match(/^\s*(?:what|which|list|show)\s+(?:are\s+the\s+)?(?:door|external\s+program|xtrn|game)s?\s+(?:on|in|does|do|are\s+(?:on|in))\s+(.+?)(?:\s+have)?\s*[?.!]*\s*$/i);
        if (m && _refs_host(m[1])) {
            return { tool: 'this_bbs', args: { kind: 'door_sections' } };
        }

        /* "what's been posted on <sub> [recently/lately]" -- recent_messages */
        m = s.match(/^\s*(?:what(?:['']?s)?|any)\s+(?:new\s+)?(?:been\s+)?(?:posted|messages?|posts)\s+(?:on|in|to)\s+([\w][\w. -]*?)(?:\s+(?:lately|recently|today))?\s*[?.!]*\s*$/i);
        if (m) {
            var sub = m[1].replace(/^\s+|\s+$/g, '');
            if (sub && _looks_like_proper_noun(sub)) {
                return { tool: 'this_bbs',
                         args: { kind: 'recent_messages', sub: sub } };
            }
        }

        /* --- relay_message --- */

        /* "tell <X> <msg>" -- but NOT "tell me/us/everyone/the channel about". */
        m = s.match(/^\s*tell\s+(?!me\b|us\b|everyone\b|them\b|him\b|her\b|the\s+(?:channel|sub|group))([A-Za-z][\w._-]{0,30})\s+(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var rcp = m[1];
            var msg = m[2].replace(/^\s+|\s+$/g, '');
            if (msg && msg.length >= 2) {
                return { tool: 'relay_message',
                         args: { recipient: rcp, text: msg } };
            }
        }

        /* "let <X> know [that] <msg>" */
        m = s.match(/^\s*let\s+([A-Za-z][\w._-]{0,30})\s+know\s+(?:that\s+)?(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var rcp2 = m[1];
            var msg2 = m[2].replace(/^\s+|\s+$/g, '');
            if (msg2) {
                return { tool: 'relay_message',
                         args: { recipient: rcp2, text: msg2 } };
            }
        }

        /* "next time you see <X>, tell <X|him/her/them> <msg>" /
         * "when you see <X> next/again/here, ..." */
        m = s.match(/^\s*(?:next\s+time|when)\s+you\s+see\s+([A-Za-z][\w._-]{0,30})(?:\s+(?:next|again|here))?\s*,?\s*(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var rcp3 = m[1];
            var rest = m[2].replace(/^\s+|\s+$/g, '');
            /* Strip "tell <rcp|him/her/them>" / "say <to>" / "let <X> know"
             * preambles so the captured text is just the message body. */
            rest = rest.replace(
                new RegExp('^(?:tell|say(?:\\s+to)?|let|message|give)\\s+(?:'
                    + rcp3.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
                    + '|him|her|them|\\1)\\s+(?:know\\s+(?:that\\s+)?)?',
                    'i'), '');
            rest = rest.replace(/^\s+|\s+$/g, '');
            if (rest && rest.length >= 2) {
                return { tool: 'relay_message',
                         args: { recipient: rcp3, text: rest } };
            }
        }

        /* "pass on to <X> <msg>" / "pass <X> a message: <msg>" */
        m = s.match(/^\s*pass\s+(?:on\s+)?to\s+([A-Za-z][\w._-]{0,30})\s*:?\s*(.+?)\s*[?.!]*\s*$/i);
        if (m) {
            var rcp4 = m[1];
            var msg4 = m[2].replace(/^\s+|\s+$/g, '');
            if (msg4) return { tool: 'relay_message',
                               args: { recipient: rcp4, text: msg4 } };
        }

        /* --- external_archives for named BBS-era topics --- */

        /* Curated set of high-recognition topics the model loves to
         * answer from training data (often wrong).  Pre-execute the
         * archive lookup so the model gets a real URL to cite. */
        var ARCHIVE_TOPICS = [
            'legend of the red dragon', ' lord ', ' lord?', ' lord.', 'lord door',
            'tradewars', 'trade wars',
            'pc-pursuit', 'pc pursuit',
            'steve jackson games', 'sjg raid',
            'modem tax',
            'sea vs pkware', 'sea vs. pkware',
            'pkware', 'pkzip', 'pkarc',
            'wwivwar', 'wwiv war', 'wwivnet',
            'barren realms elite',
            /* BBS software packages -- each has a curated archive entry
             * (bbsdoc-sw-*).  Without these, queries like "tell me about
             * WWIV chains" fall through to the model, which misroutes them
             * (e.g. to git_commits, searching Synchronet's own repo). */
            'wwiv', 'pcboard', 'wildcat', 'renegade', 'remoteaccess',
            'telegard', 'tbbs', 'spitfire', 'maximus', 'mystic',
            'majorbbs', 'major bbs', 'worldgroup', 'searchlight',
            'opus cbcs', 'opus-cbcs', 'rbbs', 'vbbs', 'powerbbs',
            'ezycom', 'citadel',
            'dope wars', 'dopewars',
            'pimp wars', 'pimpwars',
            'fossil driver',
            'cbbs', 'first bbs', 'ward christensen',
            'jason scott', 'bbs documentary',
            'phrack magazine', 'cult of the dead cow',
            'ansi art scene', 'ascii art', 'naplps', 'ripscript', 'rip script',
            'bbs software directory'
        ];
        /* Don't pre-route to the BBS-era archive when the query is a
         * Synchronet how-to / config question that merely NAMES a topic
         * ("how do I configure pkzip in sbbs?", "how to set up WWIV-style
         * menus") -- those want the wiki, not lawsuit/history trivia.  Let
         * them fall through to the model + (docstyle-boosted) wiki RAG.
         * Pure-history phrasings ("how did the SJG raid happen", "tell me
         * about pkzip") are NOT docstyle, so they still route here. */
        if (!_is_docstyle_query(s)) {
            var lc_padded = ' ' + s.toLowerCase().replace(/[?.!,]/g, ' ') + ' ';
            for (var i = 0; i < ARCHIVE_TOPICS.length; i++) {
                var t = ARCHIVE_TOPICS[i];
                var needle = (t.charAt(0) === ' ') ? t : (' ' + t + ' ');
                if (lc_padded.indexOf(needle.replace(/[?.!]/g, ' ')) >= 0) {
                    var qt = t.replace(/^\s+|\s+$/g, '').replace(/[?.!]/g, '');
                    return { tool: 'external_archives',
                             args: { query: qt } };
                }
            }
        }

        return null;
    }

    if (typeof register_intent_router == 'function') {
        register_intent_router(stock_bbs_router, 10);   /* before site routers (20) */
        register_intent_router(stock_misc_router, 30);  /* after site routers */
    }
})();
