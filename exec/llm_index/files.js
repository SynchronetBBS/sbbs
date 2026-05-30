/*
 * llm_index/files.js -- source crawler for local Synchronet file bases
 *
 * Walks all libraries + directories visible to the running install,
 * skips non-public dirs (any with a non-empty access ARS), and emits
 * one chunk per file with metadata + description.
 *
 * Returns: [{id, text, provenance}, ...]
 *
 * opts:
 *   opts.arg              -- comma-separated list of library NAMES to
 *                            include (case-insensitive), with optional
 *                            per-library dir exclusions: Lib/-tok/-tok.
 *                            Empty/missing = ALL libraries.
 *                            Examples:
 *                              files
 *                              files:Main,Music
 *                              files:Main/-sysop,Music
 *   opts.max_chunks       -- early-stop when this many files emitted
 *   opts.min_desc_length  -- skip files with NO useful description
 *                            (default: 20 chars combined desc+extdesc)
 *   opts.max_chunk_chars  -- truncate huge extdesc bodies (default 2000)
 *
 * NON-PUBLIC FILTERING: any dir whose `access_ars` is non-empty is
 * SKIPPED.  This is conservative -- it excludes age-gated and level-
 * gated dirs even if a guest could technically read them.  But it's
 * the simplest "safe by default" rule: the bot grounds its chat in
 * content the guest account has open access to, never citing a sysop-
 * only file by name to a caller who can't get to it.  Sysops who want
 * to expose level-gated content can adjust ARS or use a different
 * approach (e.g. dedicated public mirror dirs).
 *
 * Note on FILE_ID.DIZ: Synchronet imports FILE_ID.DIZ from the archive
 * on upload and stores it as the file's `extdesc`.  We just use
 * file.extdesc as-is -- no need to re-extract from the archive bytes.
 *
 * Called from exec/llm_index.js. NOT meant to be run directly via jsexec.
 */

/* Internal cap on file chunks emitted regardless of the orchestrator's
 * remaining-budget value.  Without it, an active install with hundreds
 * of dirs and large redistributable archives (Simtel, Walnut Creek,
 * etc.) can emit tens of thousands of chunks and starve msgs of any
 * budget.  2000 file chunks is plenty for grounding "got any X files?"
 * / "what's that file about?" queries. */
var INTERNAL_CAP = 2000;

function crawl(opts)
{
    opts = opts || {};
    var max          = Math.min(parseInt(opts.max_chunks, 10) || INTERNAL_CAP,
                                INTERNAL_CAP);
    var min_desc     = parseInt(opts.min_desc_length, 10) || 20;
    var max_chars    = parseInt(opts.max_chunk_chars, 10) || 2000;
    var chunks       = [];

    /* Parse opts.arg as a comma-separated allow-list of library names,
     * with optional per-library dir exclusion tokens (Lib/-token).
     * Mirrors the msgs crawler's syntax. */
    var allow_libs = null;
    var dir_excludes = {};
    if (opts.arg) {
        allow_libs = {};
        var entries = String(opts.arg).split(',');
        for (var i = 0; i < entries.length; i++) {
            var entry = entries[i].replace(/^\s+|\s+$/g, '');
            if (!entry) continue;
            var ex_parts = entry.split('/-');
            var lib_name = ex_parts[0].toLowerCase();
            allow_libs[lib_name] = true;
            for (var j = 1; j < ex_parts.length; j++) {
                var tok = ex_parts[j].replace(/^\s+|\s+$/g, '').toLowerCase();
                if (!tok) continue;
                if (!dir_excludes[lib_name]) dir_excludes[lib_name] = [];
                dir_excludes[lib_name].push(tok);
            }
        }
    }

    var libs = file_area && file_area.lib_list;
    if (!libs) return chunks;

    var skipped_nonpublic = 0;
    var dedup_count       = 0;

    /* Filename dedup.  Across an active install's hundreds of dirs
     * across many libs there's heavy filename overlap (every
     * shareware archive has its own copy of 4DOS.ZIP, README.TXT,
     * etc.).  First occurrence wins; lowercased name as the dedup
     * key. */
    var seen_names = Object.create(null);

    /* Access-filter user.  When set, skip dirs the user can't read /
     * download.  Without it the bot would index sysop-only dirs and
     * potentially name files a caller can't reach. */
    var access_user = null;
    if (opts.access_user_num) {
        try {
            var u = new User(parseInt(opts.access_user_num, 10));
            if (u && u.number) access_user = u;
        } catch (e) {
            log(LOG_WARNING, 'llm_index/files: bad access_user_num '
                + opts.access_user_num + ': ' + e);
        }
    }

    for (var l = 0; l < libs.length && chunks.length < max; l++) {
        var lib = libs[l];
        var lib_lower = (lib.name || '').toLowerCase();
        if (allow_libs && !allow_libs[lib_lower]) continue;
        var excludes = dir_excludes[lib_lower] || [];

        /* Library-level access check.  Short-circuit dirs if the
         * filter user can't even reach the library. */
        var lib_accessible = !access_user
                          || access_user.compare_ars(lib.ars || '');

        for (var d = 0; d < lib.dir_list.length && chunks.length < max; d++) {
            var dir = lib.dir_list[d];

            /* Dir-level access check.  Skip if the filter user can't
             * read/download from this dir.  We check `download_ars`
             * because the bot's chat about a file implies the caller
             * might want to fetch it -- naming files they can't
             * download is just frustrating. */
            if (access_user) {
                if (!lib_accessible
                    || !access_user.compare_ars(dir.ars || '')
                    || !access_user.compare_ars(dir.download_ars || '')) {
                    skipped_nonpublic++;
                    continue;
                }
            }

            /* Apply name-based dir excludes (Lib/-token syntax). */
            if (excludes.length) {
                var code_lower = (dir.code || '').toLowerCase();
                var name_lower = (dir.name || '').toLowerCase();
                var skip = false;
                for (var x = 0; x < excludes.length; x++) {
                    if (code_lower.indexOf(excludes[x]) >= 0
                        || name_lower.indexOf(excludes[x]) >= 0) {
                        skip = true;
                        break;
                    }
                }
                if (skip) continue;
            }

            var fb = new FileBase(dir.code);
            if (!fb.open()) continue;
            var files;
            try {
                files = fb.get_list();
            } catch (e) {
                fb.close();
                continue;
            }
            fb.close();
            if (!files || !files.length) continue;

            for (var fi = 0; fi < files.length && chunks.length < max; fi++) {
                var f = files[fi];
                if (!f || !f.name) continue;

                /* Filename dedup -- skip if this filename (case-
                 * insensitive) already came up in another dir. */
                var name_key = String(f.name).toLowerCase();
                if (seen_names[name_key]) {
                    dedup_count++;
                    continue;
                }
                seen_names[name_key] = true;

                var desc    = String(f.desc    || '').replace(/^\s+|\s+$/g, '');
                var extdesc = String(f.extdesc || '');
                /* Drop \r and strip Ctrl-A codes from extdesc -- ANSI
                 * art and color codes inflate the chunk without
                 * adding retrieval value. */
                extdesc = extdesc.replace(/\r/g, '').replace(/\x01./g, '');
                extdesc = extdesc.replace(/\s+$/g, '');

                /* Combine for length check.  Files with neither a
                 * useful short desc nor an FILE_ID.DIZ-style extdesc
                 * are mostly noise (random uploads with no metadata). */
                var combined_len = desc.length + extdesc.length;
                if (combined_len < min_desc) continue;

                var text_body = extdesc || desc;
                if (text_body.length > max_chars)
                    text_body = text_body.slice(0, max_chars) + '...';

                var uploader = f.from || '?';
                var date = '';
                if (f.time)
                    date = new Date(f.time * 1000).toISOString().slice(0, 10);
                else if (f.when)
                    date = new Date(f.when * 1000).toISOString().slice(0, 10);

                /* Prepend a discoverable header line so BM25 can hit
                 * this chunk on "what files were recently uploaded"
                 * type queries.  Without this, the chunk text is just
                 * filename + desc, neither of which contains the words
                 * "file" / "uploaded" / "upload" / "library" -- so
                 * msgs posts that happen to mention "files" beat
                 * actual file-area listings on file-finding queries.
                 *
                 * Include the LIBRARY (parent group) as well as the
                 * directory so the bot can answer "what's in the Music
                 * library?" -- previously the chunk only mentioned the
                 * dir.name with no way to know which library it sat in. */
                var header = 'Uploaded file "' + f.name + '" in '
                           + lib.name + '/' + dir.name
                           + ' file library by ' + uploader
                           + (date ? ' on ' + date : '');
                chunks.push({
                    id:         'files/' + dir.code + '/' + f.name,
                    text:       header + '\n'
                              + f.name + (desc ? '\n' + desc : '')
                              + (text_body && text_body !== desc
                                 ? '\n\n' + text_body : ''),
                    provenance: 'File:' + lib.name + '/' + dir.name
                              + ' "' + f.name + '"'
                              + ' by ' + uploader
                              + (date ? ' (' + date + ')' : ''),
                    ts:         f.time || f.when || 0,
                    title:      f.name
                });
            }
        }
    }

    if (skipped_nonpublic)
        log(LOG_INFO, 'llm_index/files: skipped ' + skipped_nonpublic
            + ' non-public dirs');
    if (dedup_count)
        log(LOG_INFO, 'llm_index/files: deduped ' + dedup_count
            + ' duplicate filenames');

    return chunks;
}

this;  /* load({}, 'llm_index/files.js') exposes crawl() */
