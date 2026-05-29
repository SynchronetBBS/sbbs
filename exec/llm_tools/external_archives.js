/*
 * llm_tools/external_archives.js -- the external_archives tool.
 *
 * Curated index of BBS-era resources (textfiles.com,
 * bbsdocumentary.com, et al).  Reads its data file
 * (llm_external_archives.json) from the ctrl directory.
 */

/* Hand-curated index of BBS-era resources on Jason Scott's sites
 * (textfiles.com, bbsdocumentary.com, plus sister sites).  Loaded
 * once and cached.  Returns null on read failure -- caller surfaces
 * the error rather than fabricating. */
var _EXTERNAL_ARCHIVES = null;
var _EXTERNAL_ARCHIVES_LOADED = false;

function _load_external_archives() {
    if (_EXTERNAL_ARCHIVES_LOADED) return _EXTERNAL_ARCHIVES;
    _EXTERNAL_ARCHIVES_LOADED = true;
    var path = system.ctrl_dir + 'llm_external_archives.json';
    var f = new File(path);
    if (!f.open('r')) return null;
    var txt = f.read();
    f.close();
    try {
        _EXTERNAL_ARCHIVES = JSON.parse(txt);
    } catch (e) {
        log(LOG_WARNING, 'external_archives: parse error in ' + path
                       + ': ' + e);
        _EXTERNAL_ARCHIVES = null;
    }
    return _EXTERNAL_ARCHIVES;
}

function external_archives(args) {
    args = args || {};
    var data = _load_external_archives();
    if (!data || !data.entries) {
        return { error: 'external_archives index unavailable.' };
    }
    var limit = parseInt(args.limit, 10) || 5;
    if (limit < 1) limit = 1;
    if (limit > 10) limit = 10;

    var q = String(args.query || '').toLowerCase().replace(/^\s+|\s+$/g, '');
    if (!q) return { error: 'external_archives needs a "query" arg '
                          + '(topic or keyword).' };
    /* Tool-scoped stopwords.  EVERY entry in this index is about
     * BBSes / the BBS era, so "bbs"/"system"/"history" carry no
     * discriminating signal -- they match dozens of entries and
     * give false positives for specific-name queries (the model
     * asks for "Funtopia BBS", gets generic /bbs/ landing hits,
     * and presents them as if they were specifically about Funtopia).
     * After filtering: if no specific terms remain, return 0 hits. */
    var STOPWORDS = {
        bbs:1, bbses:1, system:1, systems:1,
        the:1, a:1, an:1, of:1, and:1, or:1, in:1, on:1, to:1,
        for:1, from:1, with:1, by:1, at:1, as:1, is:1, are:1,
        was:1, were:1, be:1, been:1, being:1,
        what:1, where:1, when:1, who:1, why:1, how:1, which:1,
        tell:1, about:1, info:1, information:1, details:1, detail:1,
        list:1, show:1, find:1, search:1, lookup:1, look:1, give:1,
        me:1, you:1, my:1, your:1, our:1,
        do:1, does:1, did:1, can:1, could:1, would:1, should:1, will:1, may:1,
        any:1, some:1, all:1, more:1
    };
    var all_terms = q.split(/\s+/).filter(function (t) { return t.length > 1; });
    if (!all_terms.length) all_terms = [q];
    var terms = [];
    for (var ai = 0; ai < all_terms.length; ai++) {
        if (!STOPWORDS[all_terms[ai]]) terms.push(all_terms[ai]);
    }
    /* No specific terms left -- query is entirely generic ("tell me
     * about BBSes", "list of systems").  Don't fabricate matches. */
    if (!terms.length) {
        return { query: args.query, count: 0, hits: [],
                 note: 'query has no specific keywords -- '
                     + 'try naming a BBS-era topic, program, '
                     + 'incident, or person.' };
    }

    /* Score each entry by how many query terms match.  Tags hits
     * weighted heavier than summary/url hits (tags are curated
     * keywords, more deliberate than text co-occurrence).  Sort by
     * (distinct terms matched, then score, then insertion order):
     * an entry that matches 2/2 query terms beats one that matches
     * only 1/2 even if scores tie.
     *
     * Matching is word-boundary, not substring -- "line" should NOT
     * fire on "decline", "end" should NOT fire on "legend". */
    function _matches_word(hay, term) {
        var safe = term.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        return new RegExp('\\b' + safe + '\\b', 'i').test(hay);
    }
    var scored = [];
    for (var i = 0; i < data.entries.length; i++) {
        var e = data.entries[i];
        var tags_lc = [];
        var ek = e.tags || [];
        for (var k = 0; k < ek.length; k++) tags_lc.push(String(ek[k]).toLowerCase());
        var tag_hay = tags_lc.join(' ');
        var text_hay = (e.summary || '') + ' ' + (e.url || '');
        var score = 0, distinct = 0, exact_tag = false;
        for (var ti = 0; ti < terms.length; ti++) {
            /* Exact-tag match is the strongest signal: a query term
             * IS one of the entry's curated tags.  Counts as a tag
             * match for score AND flips the exact_tag flag (used as
             * a filter override for multi-term queries). */
            var is_exact = false;
            for (var tj = 0; tj < tags_lc.length; tj++) {
                if (tags_lc[tj] === terms[ti]) { is_exact = true; break; }
            }
            if (is_exact) {
                score += 2; distinct++; exact_tag = true;
            } else if (_matches_word(tag_hay, terms[ti])) {
                score += 2; distinct++;
            } else if (_matches_word(text_hay, terms[ti])) {
                score += 1; distinct++;
            }
        }
        if (score > 0) scored.push({ e: e, score: score,
                                     distinct: distinct,
                                     exact_tag: exact_tag, idx: i });
    }
    /* If the query has multiple specific terms, require either 2+
     * distinct term matches OR an exact-tag match.  A single-term
     * substring coincidence isn't enough to claim relevance to a
     * multi-word proper-noun query ("End Of The Line BBS"); but an
     * exact tag match ("FidoNet" for "FidoNet history") IS enough. */
    if (terms.length >= 2) {
        scored = scored.filter(function (s) {
            return s.distinct >= 2 || s.exact_tag;
        });
    }
    scored.sort(function (a, b) {
        if (b.distinct !== a.distinct) return b.distinct - a.distinct;
        if (b.score    !== a.score)    return b.score    - a.score;
        return a.idx - b.idx;
    });

    var hits = scored.slice(0, limit).map(function (s) {
        return {
            url:     s.e.url,
            summary: s.e.summary,
            tags:    (s.e.tags || []).slice(0, 5)
        };
    });
    return { query: args.query, count: hits.length, hits: hits };
}

llm_tool_register({
    name: 'external_archives',
    execute: external_archives,
    def: {
        type: 'function',
        'function': {
            name: 'external_archives',
            description:
                'Authoritative source for BBS-era history, culture, '
              + 'lore, software, door games, lawsuits, and incidents.  '
              + 'Searches a curated index of textfiles.com (Jason '
              + 'Scott\'s text-file archive), bbsdocumentary.com (the '
              + 'documentary film + Library of curated topics + Software '
              + 'Directory of every known BBS program), and the BBS '
              + 'Timeline at timeline.textfiles.com.\n\n'
              + 'CALL THIS TOOL FIRST -- BEFORE answering from memory -- '
              + 'whenever the caller asks about ANY of these by name:\n'
              + '  - Door games: LORD (Legend of the Red Dragon), '
              + 'TradeWars / TW2002, Dope Wars, Barren Realms Elite '
              + '(BRE), Pimp Wars\n'
              + '  - Incidents / lawsuits: Steve Jackson Games raid, '
              + 'Operation Cybersnare, ALCOR, SEA vs. PKWARE (ARC vs '
              + 'PKZIP), Thompson v. Predaina, the WWIVwar / WWIVnet vs '
              + 'WWIVlink split\n'
              + '  - Services: PC-Pursuit, satellite BBS feeds, '
              + 'modem tax rumor (FCC)\n'
              + '  - BBS software: Synchronet, PCBoard, WWIV, Wildcat, '
              + 'RemoteAccess (RA), Renegade, Telegard, TBBS, Spitfire, '
              + 'Maximus, Mystic, MajorBBS / Worldgroup, Searchlight, '
              + 'Opus-CBCS, RBBS, Citadel, CBBS (the first BBS), '
              + 'VBBS, PowerBBS, Ezycom\n'
              + '  - Graphics: ANSI, RIPscript, NAPLPS, ATASCII\n'
              + '  - Networking / drivers: FOSSIL, FidoNet (history)\n'
              + '  - Culture: ANSI art scene, warez scene, phreaking, '
              + 'pirate groups, sysop newsletters, e-zines / Phrack / '
              + '2600 / CoTDC\n'
              + '  - Meta: "list of BBS software", "history of BBSes", '
              + '"when did <X> happen", "BBS documentary", "Jason Scott '
              + 'archive"\n\n'
              + 'Args: { query: <topic keywords>, limit? }.  Returns: '
              + '{ count, hits: [{ url, summary, tags }, ...] }.\n\n'
              + 'WHY this matters: training-data answers about these '
              + 'topics are frequently WRONG (e.g. the SJG raid was '
              + '1990 not 1984, PC-Pursuit was Sprint\'s service not '
              + 'Synchronet, ProComm is a terminal program not a BBS).  '
              + 'The tool has authoritative URLs -- use them.\n\n'
              + 'PRECEDENCE: if the retrieved wiki context above '
              + 'already answers the question with a wiki.synchro.net '
              + 'URL (a Synchronet-specific topic like the host BBS\'s '
              + 'door menu, a Synchronet feature, or sysop setup), '
              + 'use the wiki -- do NOT also call external_archives.  '
              + 'This tool is for BBS-era / industry-wide history that '
              + 'the Synchronet wiki does not cover.\n\n'
              + 'DO NOT use this tool to look up a SPECIFIC BBS by '
              + 'name (e.g. "Funtopia BBS", "End of the Line BBS", '
              + '"Vertrauen").  It does NOT contain per-BBS records.  '
              + 'For specific BBSes use bbs_directory(mode:"lookup", '
              + 'name:"<bbs name>") instead.  If count=0 from this '
              + 'tool, do NOT then quote unrelated hits and claim '
              + 'they describe the BBS the caller asked about.\n\n'
              + 'RESPONSE: paraphrase the matching hit\'s summary and '
              + 'cite its URL inline.  Do NOT dump JSON or markdown.  '
              + 'Prefer ONE highly-relevant hit.  If count=0, say so '
              + 'plainly (don\'t fabricate URLs or details).  Do NOT '
              + 'call git_issues / git_commits for these history '
              + 'questions -- those tools cover Synchronet code, not '
              + 'BBS-era history.',
            parameters: {
                type: 'object',
                properties: {
                    query: { type: 'string',
                             description: 'Topic / keyword(s) to look up '
                                        + '(e.g. "ANSI art", "FidoNet '
                                        + 'history", "warez scene").' },
                    limit: { type: 'integer',
                             description: '1-10, default 5.' }
                },
                required: ['query']
            }
        }
    }
});
