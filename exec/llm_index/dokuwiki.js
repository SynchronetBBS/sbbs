/*
 * llm_index/dokuwiki.js -- source crawler for a local DokuWiki page tree
 *
 * Walks <root>/<ns>/.../<page>.txt recursively, emits one chunk per page.
 * Page id is derived from the relative path with '/' replaced by ':'
 * (DokuWiki namespace convention).
 *
 * Returns: [{id, text, provenance}, ...]
 *
 * opts:
 *   opts.arg              -- path to DokuWiki "data/pages" root, e.g.
 *                              /var/www/html/wiki/data/pages on Linux,
 *                              C:/inetpub/wiki/data/pages on Windows
 *                            (the dir that holds the top-level namespace
 *                            subdirs, NOT the parent "data" dir).
 *   opts.max_chunks       -- early-stop after this many pages
 *   opts.min_body_length  -- skip pages shorter than this (default 30 chars)
 *
 * Called from exec/llm_index.js. NOT meant to be run directly via jsexec.
 */

function strip_dokuwiki_markup(text)
{
    /* Lightweight cleanup: keep the content, drop the syntax noise so
     * BM25 tokenization doesn't fragment on markup characters. */

    /* Directives like ~~NOCACHE~~, ~~NOTOC~~. */
    text = text.replace(/~~[A-Z]+~~/g, '');

    /* Heading markers: strip the leading/trailing "=" runs but keep the
     * heading text itself (the words in the heading are usually the most
     * important tokens for retrieval of that page). */
    text = text.replace(/^={2,6}\s*(.+?)\s*={2,6}\s*$/gm, '$1');

    /* Image / file embeds {{ns:file.png}} or {{ns:doc.pdf|caption}}. Drop
     * entirely -- the filename is rarely useful in retrieval. */
    text = text.replace(/\{\{[^}]*\}\}/g, '');

    /* Internal links: [[ns:page]] or [[ns:page|label]]. Keep the label
     * (or the target, if no label) so the linked concept stays in tokens. */
    text = text.replace(/\[\[([^\]]*)\]\]/g, function (_m, inner) {
        var pipe = inner.indexOf('|');
        return pipe >= 0 ? inner.slice(pipe + 1) : inner;
    });

    /* Bold/italic/underline markers: keep content, strip markers. */
    text = text.replace(/\*\*/g, '');
    /* '//' is the DokuWiki italic marker, but it also appears inside
     * URLs as the scheme separator (https://, ftp://, mailto:).
     * Stripping every '//' mangled URLs in chunk text -- e.g.
     * 'https://gitlab.synchro.net/main/sbbs/-/issues' became
     * 'https:gitlab.synchro.net/main/sbbs/-/issues', and the bot
     * couldn't cite the URL because it looked broken (and was).
     * Skip the strip when '//' is preceded by ':' (URL scheme).
     * SpiderMonkey 1.8.5 has no lookbehind, so do this via a
     * capture-and-conditional callback. */
    text = text.replace(/(^|.)\/\//g, function (_m, prev) {
        return prev === ':' ? _m : prev;
    });
    text = text.replace(/__/g, '');

    /* Inline code wrappers ''...'' and no-format wrappers %%...%%. */
    text = text.replace(/''/g, '');
    text = text.replace(/%%/g, '');

    /* Strip <code>/<file>/<nowiki> tags but KEEP their content -- code
     * blocks often hold the config keys / commands we want indexed. */
    text = text.replace(/<\/?(?:code|file|nowiki|html|php)(?:\s[^>]*)?>/g, '');

    /* Collapse whitespace. */
    text = text.replace(/\r/g, '').replace(/[ \t]+/g, ' ');
    text = text.replace(/\n{3,}/g, '\n\n');

    return text.replace(/^\s+|\s+$/g, '');
}

function first_heading(text)
{
    /* DokuWiki H1 is "====== Title ======" (more equals = higher level).
     * Match any level 1-6 and return the first one as the page title. */
    var m = text.match(/^={2,6}\s*(.+?)\s*={2,6}\s*$/m);
    return m ? m[1].replace(/^\s+|\s+$/g, '') : '';
}

/* Namespaces to skip entirely. "wiki" is DokuWiki's own help / syntax
 * pages (wiki:syntax, wiki:welcome) -- not relevant to the BBS. The
 * playground / sandbox conventions are noise even when not empty. */
var SKIP_NS = { wiki: true, playground: true, sandbox: true };

function walk(root, prefix, chunks, opts)
{
    var max      = opts.max_chunks;
    var min_body = opts.min_body_length;
    var entries;
    try {
        entries = directory(root + '/*');
    } catch (e) {
        return;
    }
    if (!entries) return;

    for (var i = 0; i < entries.length; i++) {
        if (chunks.length >= max) return;
        var ent  = entries[i];
        /* Strip a trailing slash FIRST -- Synchronet's directory()
         * returns subdirs with a trailing slash, so the naive
         * basename-extraction (.replace of everything before the
         * final slash) would leave name as "" and the subdir would
         * recurse with an empty namespace component.  That caused
         * every page under a subdir like history/ to get the wrong
         * id ("versions" instead of "history:versions") and an
         * invalid wiki URL. */
        var name = ent.replace(/\\/g, '/').replace(/\/+$/, '')
                                          .replace(/.*\//, '');
        if (name.charAt(0) == '.') continue;
        if (name.charAt(0) == '_') continue;
        if (!name) continue;

        if (file_isdir(ent)) {
            if (SKIP_NS[name.toLowerCase()]) continue;
            walk(ent, prefix ? (prefix + ':' + name) : name, chunks, opts);
            continue;
        }

        if (!/\.txt$/i.test(name)) continue;

        var page = name.replace(/\.txt$/i, '');
        var id   = prefix ? (prefix + ':' + page) : page;

        var f = new File(ent);
        if (!f.open('r')) continue;
        var raw = f.read();
        /* Page mtime as the chunk's recency-decay timestamp.  Captured
         * before close() while the File object's date property is
         * still meaningful. */
        var mtime = f.date || 0;
        f.close();
        if (!raw) continue;

        var title = first_heading(raw);
        var body  = strip_dokuwiki_markup(raw);
        if (body.length < min_body) continue;

        /* Prepend the title to the indexed text only when it isn't
         * already the first thing in `body` (avoids duplicating it
         * for BM25). */
        var text = body;
        if (title && body.toLowerCase().indexOf(title.toLowerCase()) != 0)
            text = title + '\n' + body;

        /* URL form: DokuWiki maps the namespace separator ':' onto the
         * URL path.  Hard-coded default is the Synchronet community
         * wiki (https://wiki.synchro.net); sysops who run their own
         * wiki should edit this fallback or wire opts.arg2 ->
         * cfg.dokuwiki_url_base (TODO).  The URL is embedded in
         * provenance so the LLM can refer the caller to the canonical
         * page instead of reproducing it. */
        var url_base = opts.url_base || 'https://wiki.synchro.net/';
        if (url_base.charAt(url_base.length - 1) !== '/') url_base += '/';
        var url = url_base + id;

        chunks.push({
            id:         'dokuwiki/' + id,
            text:       text,
            provenance: 'Wiki:' + url + (title ? ' "' + title + '"' : ''),
            ts:         mtime,
            title:      title || ''
        });
    }
}

function crawl(opts)
{
    opts = opts || {};
    var max      = parseInt(opts.max_chunks, 10) || 5000;
    var min_body = parseInt(opts.min_body_length, 10) || 30;
    var root     = opts.arg;
    if (!root) {
        log(LOG_WARNING, 'llm_index/dokuwiki: missing path arg '
            + '(usage: dokuwiki:<path/to/data/pages>)');
        return [];
    }
    /* Normalize forward-slashes and strip trailing slash. */
    root = String(root).replace(/\\/g, '/').replace(/\/$/, '');

    var chunks = [];
    walk(root, '', chunks, { max_chunks: max, min_body_length: min_body });
    return chunks;
}

this;  /* load({}, 'llm_index/dokuwiki.js') exposes crawl() */
