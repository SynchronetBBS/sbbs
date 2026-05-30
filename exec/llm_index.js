/*
 * llm_index.js -- builds the BM25 retrieval index used by chat_llm.js
 *
 * Reads index_* knobs from ctrl/chat_llm.ini's [<persona>] section
 * (falling back to [default]), walks each configured source crawler under
 * exec/llm_index/, collects {text, provenance, id} chunks, builds a
 * BM25 index, and writes it to <data_dir>/<index_output>.
 *
 * Usage (standalone, via jsexec):
 *     jsexec llm_index.js [<persona_code>]
 * <persona_code> defaults to "default" if omitted (matches chat_llm.ini's
 * fallback section). Use the lowercased guru code per Synchronet
 * convention (see load_cfg.c:330+).
 *
 * Typical sysop cron: nightly run via timed event:
 *     jsexec llm_index.js guru
 *
 * Output format (JSON):
 *   {
 *     version: 1,
 *     persona: "guru",
 *     built:   1779600000,
 *     N:       1234,           -- total docs
 *     avgdl:   50.2,           -- average doc length (tokens)
 *     k1:      1.5,            -- BM25 tunable
 *     b:       0.75,           -- BM25 tunable
 *     docs:    [{id, text, prov, len}, ...],
 *     terms:   { token: {df, postings:[[doc_idx, tf], ...]}, ... }
 *   }
 *
 * The retrieval side (chat_llm.js) loads this file, tokenizes the user's
 * input the same way, computes BM25 scores per document, and returns the
 * top-K hits with text + provenance for prompt injection.
 *
 * MUST keep tokenize() identical to chat_llm.js's tokenize() -- if they
 * diverge, retrieval breaks. (See chat_llm.js's tokenize() comment.)
 */

var CHAT_LLM_CONFIG_FILE = system.ctrl_dir + 'chat_llm.ini';
var BM25_K1              = 1.5;
var BM25_B               = 0.75;

function load_config(persona)
{
    var f = new File(CHAT_LLM_CONFIG_FILE);
    if (!f.open('r'))
        throw new Error('cannot open ' + CHAT_LLM_CONFIG_FILE);
    /* Defaults in the root (unnamed) section; named [<persona>] overrides. */
    var defaults = f.iniGetObject(null) || {};
    var section  = (persona && String(persona).toLowerCase() != 'default')
                 ? f.iniGetObject(persona) : null;
    f.close();

    var cfg = {};
    var k;
    for (k in defaults) cfg[k] = defaults[k];
    if (section) for (k in section) cfg[k] = section[k];

    cfg.index_max_chunks = parseInt(cfg.index_max_chunks, 10) || 5000;
    cfg.index_top_k      = parseInt(cfg.index_top_k, 10)      || 5;
    cfg.index_output     = cfg.index_output    || 'chat/<persona>.idx';
    cfg.index_sources    = cfg.index_sources   || 'msgs';

    return cfg;
}

/* Tokenize text for BM25. KEEP IN SYNC with chat_llm.js's tokenize(). */
function tokenize(text)
{
    if (!text) return [];
    var matches = String(text).toLowerCase().match(/[a-z0-9]+/g);
    return matches || [];
}

/* Accept a user number OR alias string (e.g. "guest", "Guest") and
 * return the user record number, or 0 if the value is empty / no
 * match.  Aliases are preferred in ini files because record numbers
 * can shift across installs / restorations, but the underlying
 * filter API takes a number. */
function resolve_access_user(value)
{
    if (value === undefined || value === null || value === '') return 0;
    var s = String(value).replace(/^\s+|\s+$/g, '');
    if (!s) return 0;
    /* If it parses cleanly as an integer with no extra chars, treat
     * as a record number directly.  Otherwise look it up as alias. */
    if (/^\d+$/.test(s)) return parseInt(s, 10);
    try {
        var n = system.matchuser(s);
        if (n > 0) return n;
        log(LOG_WARNING, 'llm_index: no user matching alias "' + s + '"');
    } catch (e) {
        log(LOG_WARNING, 'llm_index: matchuser("' + s + '") failed: ' + e);
    }
    return 0;
}

function load_source(name)
{
    var lib = {};
    try {
        load(lib, 'llm_index/' + name + '.js');
    } catch (e) {
        throw new Error('failed to load llm_index/' + name + '.js: ' + e);
    }
    if (typeof lib.crawl != 'function')
        throw new Error('llm_index/' + name + '.js has no crawl() function');
    return lib;
}

function parse_source_spec(spec)
{
    /* "msgs" or "dokuwiki:/path/to/data" */
    var colon = spec.indexOf(':');
    if (colon < 0) return { name: spec.trim(), arg: null };
    return { name: spec.slice(0, colon).trim(), arg: spec.slice(colon + 1).trim() };
}

function build_index(chunks)
{
    var docs   = [];                       /* [{id, text, prov, len}, ...] */
    /* Object.create(null) makes a prototype-less object so tokens that
     * happen to match built-in Object property names (constructor,
     * toString, hasOwnProperty, valueOf, etc.) don't trip our
     * `if (!terms[tok])` check on inherited properties. */
    var terms  = Object.create(null);      /* token -> {df, postings:[[doc_idx, tf], ...]} */

    for (var i = 0; i < chunks.length; i++) {
        var c = chunks[i];
        if (!c.text) continue;
        var tokens = tokenize(c.text);
        if (tokens.length == 0) continue;

        /* Per-doc term frequencies (also prototype-less). */
        var tf = Object.create(null);
        for (var t = 0; t < tokens.length; t++) {
            var tok = tokens[t];
            tf[tok] = (tf[tok] || 0) + 1;
        }

        /* Title tokens, stored as a prototype-less {tok:1} set so
         * bm25_search can do an O(1) lookup per (term, doc) pair to
         * decide whether to apply the title-match boost.  Title comes
         * from the crawler (msgs: subject, dokuwiki: first H1,
         * files: filename, gitissue: issue title).  When a query
         * token matches a doc's title token, that doc's per-term BM25
         * contribution gets multiplied -- surfaces authoritative short
         * pages (e.g. wiki howto:report) over long msgs posts that
         * happen to mention the same words in passing. */
        var title_tokens = null;
        if (c.title) {
            var tt = tokenize(c.title);
            if (tt.length) {
                title_tokens = Object.create(null);
                for (var ti = 0; ti < tt.length; ti++)
                    title_tokens[tt[ti]] = 1;
            }
        }

        var doc_idx = docs.length;
        docs.push({
            id:   c.id   || ('chunk-' + doc_idx),
            text: c.text,
            prov: c.provenance || '',
            len:  tokens.length,
            /* Unix-epoch authorship/publish time when the crawler can
             * supply one (msgs: when_written_time, dokuwiki: file
             * mtime, gitlab: created_at/updated_at, files: f.time).
             * Used by bm25_search's recency decay; omitted/0 = treat
             * as ageless (no decay). */
            ts:   c.ts || 0,
            tt:   title_tokens   /* null when crawler doesn't supply */
        });

        for (var tok in tf) {
            if (!terms[tok]) terms[tok] = { df: 0, postings: [] };
            terms[tok].df++;
            terms[tok].postings.push([doc_idx, tf[tok]]);
        }
    }

    var total = 0;
    for (var i = 0; i < docs.length; i++) total += docs[i].len;
    var avgdl = docs.length ? total / docs.length : 0;

    return {
        version: 1,
        built:   time(),
        N:       docs.length,
        avgdl:   avgdl,
        k1:      BM25_K1,
        b:       BM25_B,
        docs:    docs,
        terms:   terms
    };
}

function write_index(idx, output_path)
{
    /* mkdir parent. */
    var dir = output_path.replace(/\/[^\/]*$/, '/');
    mkdir(dir);

    var tmp = output_path + '.tmp';
    var f = new File(tmp);
    if (!f.open('w'))
        throw new Error('cannot write ' + tmp);
    /* No indent -- compact JSON. */
    f.write(JSON.stringify(idx));
    f.close();

    if (file_exists(output_path)) file_remove(output_path);
    if (!file_rename(tmp, output_path))
        throw new Error('cannot rename ' + tmp + ' -> ' + output_path);
}

function run(persona)
{
    persona = (persona || 'default').toLowerCase();
    var cfg = load_config(persona);
    /* Sources are SEMICOLON-separated (commas are reserved for source-arg
     * lists, e.g. msgs:Local,DOVE-Net,FsxNet). */
    var sources = String(cfg.index_sources || '').split(';')
                      .map(function (s) { return s.trim(); })
                      .filter(function (s) { return s.length > 0; });
    if (sources.length == 0) {
        print('llm_index: no sources configured for persona "' + persona + '"');
        return false;
    }

    var all_chunks = [];
    var per_source_max = cfg.index_max_chunks;

    for (var i = 0; i < sources.length; i++) {
        var spec = parse_source_spec(sources[i]);
        var start_total = all_chunks.length;
        var t0 = system.timer;

        try {
            var src = load_source(spec.name);
            var chunks = src.crawl({
                arg:             spec.arg,
                max_chunks:      per_source_max - all_chunks.length,
                /* Pass the access-filter user number through to
                 * crawlers that can use it (msgs, files).
                 * The ini value (cfg.index_access_user) may be a
                 * user number OR an alias (e.g. "guest", "Guest");
                 * resolve_access_user() handles both. */
                access_user_num: resolve_access_user(cfg.index_access_user)
            });
            all_chunks = all_chunks.concat(chunks);
            print('llm_index: ' + spec.name + ': '
                + chunks.length + ' chunks in '
                + (system.timer - t0).toFixed(2) + 's');
        } catch (e) {
            print('llm_index: ' + spec.name + ' FAILED: ' + e);
        }

        if (all_chunks.length >= cfg.index_max_chunks) {
            print('llm_index: hit max_chunks cap ('
                + cfg.index_max_chunks + '), stopping');
            break;
        }
    }

    print('llm_index: building BM25 from '
        + all_chunks.length + ' chunks...');
    var t1 = system.timer;
    var idx = build_index(all_chunks);

    var output_path = system.data_dir
                    + cfg.index_output.replace(/<persona>/g, persona);
    write_index(idx, output_path);

    print('llm_index: wrote ' + output_path
        + ' (' + idx.N + ' docs, '
        + Object.keys(idx.terms).length + ' unique terms, '
        + 'build ' + (system.timer - t1).toFixed(2) + 's)');
    return true;
}

/* --- standalone mode --- */

if (typeof bbs == 'undefined') {
    var persona = (argv && argv[0]) || 'default';
    try {
        run(persona);
    } catch (e) {
        print('llm_index ERROR: ' + e);
        exit(1);
    }
}
