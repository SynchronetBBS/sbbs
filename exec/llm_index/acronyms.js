/*
 * llm_index/acronyms.js -- source crawler for DokuWiki acronyms files
 *
 * Reads one or more "<ACRONYM><whitespace><expansion>" files (the
 * DokuWiki conf/acronyms.conf + conf/acronyms.local.conf format) and
 * emits one chunk per acronym, phrased as a definition so BM25 matches
 * "what is X" / "what does X stand for" / "define X" queries.
 *
 * opts:
 *   opts.arg -- semicolon- or comma-separated list of file paths.  List
 *               the most authoritative file FIRST: the first definition
 *               seen for an acronym wins, so a site's acronyms.local.conf
 *               should precede the generic acronyms.conf.
 *
 * Returns: [{id, text, provenance, ts, title}, ...]
 *
 * Called from exec/llm_index.js.  NOT meant to be run directly.
 */

function crawl(opts)
{
    opts = opts || {};
    if (!opts.arg) {
        log(LOG_WARNING, 'llm_index/acronyms: missing path arg '
            + '(usage: acronyms:<file>[;<file>...])');
        return [];
    }
    var paths  = String(opts.arg).split(/[;,]/);
    var chunks = [];
    var seen   = {};   /* lowercased acronym -> already emitted */

    for (var p = 0; p < paths.length; p++) {
        var path = paths[p].replace(/^\s+|\s+$/g, '');
        if (!path) continue;
        var f = new File(path);
        if (!f.open('r')) {
            log(LOG_WARNING, 'llm_index/acronyms: cannot open ' + path);
            continue;
        }
        var raw   = f.read();
        var mtime = f.date || 0;
        f.close();
        if (!raw) continue;

        var lines = raw.split(/\r?\n/);
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].replace(/^\s+|\s+$/g, '');
            if (!line || line.charAt(0) === '#' || line.charAt(0) === ';')
                continue;
            /* "<ACRONYM><whitespace><expansion>" -- acronym is the first
             * whitespace-delimited token, expansion is the rest. */
            var m = line.match(/^(\S+)\s+(.+)$/);
            if (!m) continue;
            var acr = m[1];
            var exp = m[2].replace(/^\s+|\s+$/g, '');
            var key = acr.toLowerCase();
            if (seen[key]) continue;   /* first file wins (local before generic) */
            seen[key] = true;

            chunks.push({
                id:         'acronym/' + acr,
                text:       acr + ' stands for ' + exp
                          + '. (' + acr + ' = ' + exp + ')',
                provenance: 'Acronym: ' + acr + ' = ' + exp,
                ts:         mtime,
                title:      acr
            });
        }
    }
    return chunks;
}

this;  /* load({}, 'llm_index/acronyms.js') exposes crawl() */
