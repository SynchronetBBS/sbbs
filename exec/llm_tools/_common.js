/*
 * llm_tools/_common.js -- shared helpers used across multiple tool files.
 *
 * Loaded first (underscore-prefixed file sorts before any tool file in
 * the llm_tools.js loader's alphabetical pass).  Defines no tool of its
 * own -- only helpers that more than one tool file needs.
 *
 * Keep this file small: anything used by exactly ONE tool should live
 * in that tool's file, not here.
 */

/* Forbidden-key tripwire.  If a maintainer ever accidentally adds a
 * field whose name matches one of these patterns to the snapshot,
 * audit the call -- something sensitive may be leaking. */
var FORBIDDEN_KEY_PATTERN = /(password|passwd|secret|private_key|api_key|token|passphrase|\bauth\b)/i;
function _audit_no_secrets(obj, path) {
    if (!obj || typeof obj != 'object') return;
    for (var k in obj) {
        if (FORBIDDEN_KEY_PATTERN.test(k))
            throw new Error('llm_tools: forbidden field "'
                + (path || '<root>') + '.' + k
                + '" -- review llm_tools security policy');
        if (obj[k] && typeof obj[k] == 'object')
            _audit_no_secrets(obj[k], (path || '') + '.' + k);
    }
}

/* Name of the BBS hosting this bot, used in tool descriptions so the
 * LLM treats "<BBS name>" and "this BBS" as synonymous in tool
 * selection (otherwise a "what subs does Vertrauen have?" query
 * would route to lookup_bbs("Vertrauen") -- which has finger / cached
 * stats but NO sub/dir/door lists -- instead of this_bbs). */
var THIS_BBS_NAME = (typeof system != 'undefined' && system.name)
    ? String(system.name) : 'this BBS';
