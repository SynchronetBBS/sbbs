/*
 * llm_tools.js -- tool / function-calling registry for llm.js
 *
 * Thin loader.  Iterates exec/llm_tools/*.js and load()s each into
 * the current scope.  Each tool file defines its execute function +
 * any local helpers, then calls llm_tool_register({name, execute,
 * def}) at the bottom to publish itself.
 *
 * Files starting with "_" (underscore) load first alphabetically, so
 * shared helpers in _common.js are available to subsequent tool
 * files.  Missing tool files are warned-and-skipped; load failures
 * never break the rest of the engine.
 *
 * Exposes (to the caller, typically llm.js):
 *   llm_tools_defs       -- array of Ollama-format tool specs, in
 *                           registration order.
 *   llm_tools_execute    -- function (name, args, env) -> string
 *                           result.  Calls the tool's executor, JSON-
 *                           encodes the return value, and traps errors
 *                           into an {error:...} JSON blob.
 *   llm_tool_register    -- registration entry point.  Each tool
 *                           file calls it once.
 *
 * SECURITY MODEL (read carefully before adding tools):
 *   - All tools must be READ-ONLY.  No side-effect methods on host
 *     objects (.save_msg, .write, .delete, etc.); only property
 *     access and read-only enumeration walks.
 *   - No path / eval / shell pass-through.  Tools take only args the
 *     LLM emits in tool_calls.arguments, constrained by the JSON
 *     Schema in the tool def.
 *   - Whitelist approach for fields exposed.  Forbidden-key tripwire
 *     in llm_tools/_common.js audits returned objects.
 *   - Caller-supplied query strings from chat are NEVER forwarded
 *     into tool execution.  Args come only from the model's
 *     structured tool_calls payload.
 */

var llm_tools_defs     = [];
var llm_tools_dispatch = {};

function llm_tool_register(spec) {
    if (!spec || !spec.name || typeof spec.execute != 'function' || !spec.def)
        throw new Error('llm_tool_register: spec needs {name, execute, def}');
    if (llm_tools_dispatch[spec.name])
        log(LOG_WARNING, 'llm_tools: tool "' + spec.name
            + '" registered twice (later wins)');
    llm_tools_dispatch[spec.name] = spec.execute;
    /* Defs array preserves first-registration order; if a name is
     * re-registered we replace the def in-place rather than push a
     * duplicate to the model's tool list. */
    for (var i = 0; i < llm_tools_defs.length; i++) {
        if (llm_tools_defs[i]
            && llm_tools_defs[i]['function']
            && llm_tools_defs[i]['function'].name === spec.name) {
            llm_tools_defs[i] = spec.def;
            return;
        }
    }
    llm_tools_defs.push(spec.def);
}

function llm_tools_execute(name, args, env) {
    var fn = llm_tools_dispatch[name];
    if (!fn) return JSON.stringify({ error: 'unknown tool: ' + name });
    try {
        var result = fn(args || {}, env || {});
        return (typeof result == 'string')
             ? result
             : JSON.stringify(result);
    } catch (e) {
        return JSON.stringify({ error: 'tool exec: ' + e });
    }
}

/* Auto-load every llm_tools/*.js.  Sorted alphabetically so that
 * "_common.js" (and any other underscore-prefixed helper files) load
 * before tool files that depend on them.  Tools that fail to load
 * are warned and skipped -- one broken file doesn't disable the rest.
 *
 * Tool files run in this scope: any `function` / `var` they declare
 * is visible to subsequent loads.  Use that for shared helpers in
 * _common.js; keep tool-private state inside the tool's function. */
(function () {
    var dir = system.exec_dir + 'llm_tools';
    var files = [];
    try { files = directory(dir + '/*.js') || []; }
    catch (e) {
        log(LOG_WARNING, 'llm_tools: cannot list ' + dir + ': ' + e);
        return;
    }
    files.sort();
    for (var i = 0; i < files.length; i++) {
        try { load(files[i]); }
        catch (e) {
            log(LOG_WARNING, 'llm_tools: failed to load ' + files[i]
                + ': ' + e);
        }
    }
})();

/* Make the registry visible to the loading scope (llm.js does
 * `load("llm_tools.js")` and reads llm_tools_defs / calls
 * llm_tools_execute directly). */
this;
