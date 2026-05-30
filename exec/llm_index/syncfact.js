/*
 * llm_index/syncfact.js -- source crawler for a flat, one-fact-per-line
 * file (the Synchronet "fact" list, data/syncfact.lst).  Emits one chunk
 * per fact so BM25 can retrieve a specific fact for a matching query.
 *
 * opts:
 *   opts.arg -- path to the facts file.  A relative path (no leading "/"
 *               or drive letter) resolves under the data directory, so
 *               "syncfact:syncfact.lst" reads <data_dir>/syncfact.lst.
 *
 * Returns: [{id, text, provenance, ts, title}, ...]
 *
 * Called from exec/llm_index.js.  NOT meant to be run directly.
 */

function crawl(opts)
{
    opts = opts || {};
    var path = String(opts.arg || '').replace(/^\s+|\s+$/g, '');
    if (!path) {
        log(LOG_WARNING, 'llm_index/syncfact: missing path arg '
            + '(usage: syncfact:<file>)');
        return [];
    }
    /* Resolve a relative path under the data directory. */
    if (!/^(\/|[a-zA-Z]:)/.test(path))
        path = system.data_dir + path;

    var f = new File(path);
    if (!f.open('r')) {
        log(LOG_WARNING, 'llm_index/syncfact: cannot open ' + path);
        return [];
    }
    var raw   = f.read();
    var mtime = f.date || 0;
    f.close();
    if (!raw) return [];

    var lines  = raw.split(/\r?\n/);
    var chunks = [];
    for (var i = 0; i < lines.length; i++) {
        /* Strip any Ctrl-A attribute codes, then trim. */
        var line = lines[i].replace(/\x01./g, '').replace(/^\s+|\s+$/g, '');
        if (!line || line.charAt(0) === '#' || line.charAt(0) === ';')
            continue;
        chunks.push({
            id:         'syncfact/' + (i + 1),
            text:       line,
            provenance: 'Synchronet fact',
            ts:         mtime,
            title:      ''
        });
    }
    return chunks;
}

this;  /* load({}, 'llm_index/syncfact.js') exposes crawl() */
