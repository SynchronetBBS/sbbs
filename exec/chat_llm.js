/*
 * chat_llm.js -- minimal LLM-backed chat engine, v0
 *
 * Transport-agnostic LLM conversation core. Consumers (the BBS private-
 * paging dispatch, multinode-chat dispatch, IRC bot adapters) call:
 *
 *   llm_open(ctx)              -- generate an opening greeting
 *   llm_chat(input, ctx)       -- generate a reply
 *   is_addressed(line, names)  -- helper to decide whether the bot was
 *                                 addressed (callers use this in multinode
 *                                 or irc modes to gate llm_chat invocation)
 *
 * Context object shape:
 *   {
 *     persona: {                  -- bot identity
 *       code: 'GURU',             -- section in chat_llm.ini ([default] fallback)
 *       name: 'The Guru'          -- display name (sets @bot_name@ macro)
 *     },
 *     speaker: {                  -- who is speaking RIGHT NOW
 *       id: 'user:42',            -- namespaced identity for memory keying
 *       alias: 'digital man',
 *       attrs: {                  -- optional, used for macro substitution
 *         real_name, level, location, lang
 *       }
 *     },
 *     participants: [             -- everyone present (optional; default [speaker])
 *       {id, alias}
 *     ],
 *     transcript: [               -- labeled recent history (optional; default [])
 *       {who: 'alias_or_persona_name', text, ts, bot: true|false}
 *     ],
 *     mode: 'private' | 'multinode' | 'irc',
 *     supports_utf8: true,        -- terminal capability
 *     addressed: true             -- private: always true; multinode/irc:
 *                                    caller checks is_addressed() first
 *   }
 *
 * Standalone test via jsexec:
 *     jsexec chat_llm.js [-u N] ["your message"]
 *   Builds a single-participant 'private' ctx around the loaded User.
 */

load('http.js');

var CHAT_LLM_CONFIG_FILE = system.ctrl_dir + 'chat_llm.ini';
var DEFAULT_TIMEOUT      = 60;
var DEFAULT_TOKENS       = 500;
var DEFAULT_TEMP         = 0.7;
/* After the primary endpoint fails, skip it (go straight to the
 * fallback) for this many seconds so an outage doesn't add the primary's
 * full timeout to every reply.  Module-level state below persists across
 * turns in a long-running process (the IRC bot); a one-shot process
 * (guru_ask) re-probes the primary each run. */
var ENDPOINT_FAILOVER_COOLDOWN = 60;
var _primary_down_until = 0;   /* unix time; 0 = primary presumed up */

/* ISO 639-1 -> English name lookup. Synchronet's user.lang field is
 * usually a 2-letter code (matching the text.<lang>.ini convention),
 * but models handle bare codes inconsistently (qwen2.5 mistook "ja"
 * for Finnish). Expand known codes to full names; pass unknowns
 * through as-is and let the model do its best. */
var LANG_NAMES = {
    en: 'English',     fr: 'French',      es: 'Spanish',    de: 'German',
    it: 'Italian',     pt: 'Portuguese',  nl: 'Dutch',      ja: 'Japanese',
    zh: 'Chinese',     ko: 'Korean',      ru: 'Russian',    pl: 'Polish',
    sv: 'Swedish',     no: 'Norwegian',   da: 'Danish',     fi: 'Finnish',
    tr: 'Turkish',     ar: 'Arabic',      he: 'Hebrew',     el: 'Greek',
    cs: 'Czech',       hu: 'Hungarian',   ro: 'Romanian',   uk: 'Ukrainian',
    vi: 'Vietnamese',  th: 'Thai',        id: 'Indonesian', hi: 'Hindi'
};

function lang_name(code)
{
    if (!code) return '';
    var key = String(code).toLowerCase();
    return LANG_NAMES[key] || code;
}

function load_config(persona_code)
{
    var f = new File(CHAT_LLM_CONFIG_FILE);
    if (!f.open('r'))
        throw new Error('cannot open ' + CHAT_LLM_CONFIG_FILE);
    /* Defaults live in the root (unnamed) section; a named [<persona_code>]
     * section overrides them.  "default" is the reserved fallback persona
     * -- it has no section of its own (it IS the root defaults), so we
     * don't look up a [default] section for it.  A sysop guru must not be
     * coded "default" (see ctx_from_user's canonicalization). */
    var defaults = f.iniGetObject(null) || {};
    var section  = (persona_code && String(persona_code).toLowerCase() != 'default')
                 ? f.iniGetObject(persona_code) : null;
    f.close();

    /* Merge: persona section overrides root defaults. */
    var cfg = {};
    var k;
    for (k in defaults) cfg[k] = defaults[k];
    if (section) for (k in section) cfg[k] = section[k];

    if (!cfg.endpoint)
        throw new Error(CHAT_LLM_CONFIG_FILE + ': missing endpoint=');
    if (!cfg.model)
        throw new Error(CHAT_LLM_CONFIG_FILE + ': missing model=');

    cfg.provider    = cfg.provider    || 'openai';
    cfg.api_key     = cfg.api_key     || 'placeholder';
    /* Optional secondary endpoint.  When set and the primary endpoint
     * fails (transport error / non-200), dispatch() retries once here. */
    cfg.endpoint_fallback = cfg.endpoint_fallback || '';
    cfg.max_tokens  = parseInt(cfg.max_tokens, 10)  || DEFAULT_TOKENS;
    cfg.temperature = parseFloat(cfg.temperature)   || DEFAULT_TEMP;
    cfg.timeout     = parseInt(cfg.timeout, 10)     || DEFAULT_TIMEOUT;
    /* Ollama context window (tokens).  0/unset => omitted from the
     * request, so Ollama uses its server default (~4096).  Set in
     * chat_llm.ini when the system prompt + RAG exceed the default and
     * the model truncates the front of the prompt (where the identity
     * and style rules live). */
    cfg.num_ctx     = parseInt(cfg.num_ctx, 10)     || 0;
    /* keep_alive is passed through as a string; empty/unset means
     * "use the backend's default" (5m for Ollama). */
    cfg.keep_alive  = cfg.keep_alive  || '';

    /* Persistent-memory knobs (step 1 of the roadmap). */
    cfg.memory_persist      = (cfg.memory_persist === undefined)
                              || (cfg.memory_persist != 'false');
    cfg.history_window      = parseInt(cfg.history_window, 10)      || 8;
    cfg.summarize_threshold = parseInt(cfg.summarize_threshold, 10) || 30;
    cfg.summarize_batch     = parseInt(cfg.summarize_batch, 10)     || 10;
    cfg.memory_max_age_days = parseInt(cfg.memory_max_age_days, 10) || 365;
    /* Relay queue cap: max pending relays held per recipient AND per
     * sender (anti-abuse).  Passed to the relay_message tool via env. */
    cfg.relay_max_pending   = parseInt(cfg.relay_max_pending, 10)   || 5;

    /* BM25 retrieval knobs (step 3). The indexer (exec/chat_index.js)
     * writes the index file; we just need its path and top_k here. */
    cfg.index_top_k         = parseInt(cfg.index_top_k, 10)         || 5;
    cfg.index_output        = cfg.index_output || 'chat/<persona>/llm_rag.idx';

    /* Per-source BM25 score multipliers.  Comma-separated key=value
     * pairs where key matches the source identifier (the first
     * segment of a chunk's id: dokuwiki, msgbase, filebase, gitlab).
     * Default 1.0 for any source not listed.  Use to make the wiki
     * (curated, authoritative) outrank rich-but-noisy msgbase
     * content when both retrieve for the same query.
     *
     * Example: index_source_weights = dokuwiki=2.0,gitlab=1.5
     *          gives wiki a 2x boost and gitissue/commit refs 1.5x.
     */
    cfg.index_source_weights = parse_source_weights(cfg.index_source_weights);

    /* Group aliases for source-aware retrieval boosting.  When the caller's
     * query mentions a network/group name (e.g. "what's debated on FidoNet?"),
     * bm25_search uses this map to translate query tokens to provenance-
     * group-name keys and boost chunks actually FROM that group over
     * chunks merely discussing it.  This is per-deployment data -- group
     * names are sysop choices (e.g. "Main", "DOVE-Net", "fsxNet", or
     * "Local", "RetroNet", whatever the local SCFG calls them) -- so
     * it belongs in chat_llm.ini, NOT hard-coded in chat_llm.js.
     *
     * Format: token=group,group; token=group; ...   (semicolon-separated)
     *   Left of '=' = a query token (lowercase)
     *   Right of '=' = comma-separated lowercased group-name(s) as they
     *                  appear in provenance after .toLowerCase()
     * Group names in chunk provenance go through .toLowerCase(); match
     * the form there (e.g. "DOVE-Net" -> "dove-net").
     *
     * Example:
     *   group_aliases = fidonet=fidonet; dove,dovenet=dove-net;
     *                   fsxnet,fsx=fsxnet; sync=sync_sys,syncanno,syncprog
     *
     * Empty/missing = no group-boost applied (older configs keep working). */
    cfg.group_aliases = parse_group_aliases(cfg.group_aliases);

    /* Streaming toggle. When true (default) chat_openai uses SSE to
     * stream tokens; consumer is responsible for emitting them. Sysops
     * can disable for backends that don't support SSE. */
    if (cfg.stream === undefined) cfg.stream = 'true';

    /* Resolve long-form prompts: a *_file= value overrides the inline
     * value. Prompt files live in ctrl/ (sysop-editable plain text,
     * any length, with @TOKEN@ macros). Trailing newline is stripped
     * because most editors add one and we don't want it in the prompt. */
    cfg.system_prompt  = resolve_prompt(cfg, 'system_prompt_file',  'system_prompt');
    cfg.opening_prompt = resolve_prompt(cfg, 'opening_prompt_file', 'opening_prompt');

    /* Optional smaller system prompt used ONLY for the opening
     * greeting call.  The opening doesn't need the full archive-
     * usage / anti-fabrication / wiki-URL rules (those are for
     * chat-turn retrieval grounding) -- a 1KB persona+style prompt
     * cuts opening TTFC dramatically on small models.  Falls back
     * to the regular system_prompt when not configured. */
    cfg.opening_system_prompt = resolve_prompt(cfg,
                                               'opening_system_prompt_file',
                                               'opening_system_prompt')
                              || cfg.system_prompt;

    return cfg;
}

/* Parse "key=val,key=val" source-weight string into a {source: weight}
 * object.  Empty input or missing weights default to {}; the caller
 * (bm25_search) treats missing entries as 1.0 (no boost). */
function parse_source_weights(s)
{
    var out = {};
    if (!s) return out;
    var parts = String(s).split(',');
    for (var i = 0; i < parts.length; i++) {
        var kv = parts[i].split('=');
        if (kv.length != 2) continue;
        var k = kv[0].replace(/^\s+|\s+$/g, '').toLowerCase();
        var v = parseFloat(kv[1]);
        if (!k || isNaN(v) || v < 0) continue;
        out[k] = v;
    }
    return out;
}

/* Parse "token=group,group; token=group" group-aliases string into a
 * {query_token: [provenance_group_name, ...]} map for bm25_search's
 * source-aware boost.
 *
 * Returns the empty object {} on missing/empty input -- bm25_search
 * then runs as if no aliases were configured (no group-boost).
 *
 * Lowercases both keys and values (group names in provenance are
 * compared after .toLowerCase()).  Left-of-= may be a comma-separated
 * list of synonym tokens (e.g. "dove,dovenet=dove-net"); each one
 * maps to the same group list. */
function parse_group_aliases(s)
{
    var out = {};
    if (!s || typeof s != 'string') return out;
    var entries = s.split(';');
    for (var i = 0; i < entries.length; i++) {
        var entry = entries[i].replace(/^\s+|\s+$/g, '');
        if (!entry) continue;
        var eq = entry.indexOf('=');
        if (eq < 0) continue;
        var lhs = entry.substring(0, eq).toLowerCase();
        var rhs = entry.substring(eq + 1).toLowerCase();
        var tokens = lhs.split(',');
        var groups = rhs.split(',');
        var clean_groups = [];
        for (var g = 0; g < groups.length; g++) {
            var grp = groups[g].replace(/^\s+|\s+$/g, '');
            if (grp) clean_groups.push(grp);
        }
        if (!clean_groups.length) continue;
        for (var t = 0; t < tokens.length; t++) {
            var tok = tokens[t].replace(/^\s+|\s+$/g, '');
            if (tok) out[tok] = clean_groups;
        }
    }
    return out;
}

function resolve_prompt(cfg, file_key, inline_key)
{
    if (cfg[file_key]) {
        var path = system.ctrl_dir + cfg[file_key];
        if (!file_exists(path))
            throw new Error('chat_llm: ' + file_key + '=' + cfg[file_key]
                + ' but file not found at ' + path);
        var f = new File(path);
        if (!f.open('r'))
            throw new Error('cannot open ' + path);
        var content = f.read();
        f.close();
        return content.replace(/\r?\n$/, '');
    }
    return cfg[inline_key] || '';
}

function expand_macros(template, mctx)
{
    return template.replace(/@(\w+)@/g, function (_, name) {
        return (mctx && mctx[name] != undefined) ? String(mctx[name]) : '';
    });
}

/* Build the macro context (the @TOKEN@ substitution map) from the chat
 * context. Pulls speaker attributes, persona name, and a language
 * directive derived from the speaker's lang attribute. */
function build_macro_ctx(ctx)
{
    var attrs     = (ctx.speaker && ctx.speaker.attrs) || {};
    var lang_code = attrs.lang ? String(attrs.lang) : '';
    var lang_full = lang_name(lang_code);
    /* TODO (C++ dispatch step): the model emits UTF-8. Synchronet's
     * output pipeline auto-converts UTF-8 -> terminal charset (CP437 etc.)
     * which works for Western-European Latin but produces garbage for
     * CJK/Cyrillic/Greek/Arabic/Hebrew on non-UTF8 terminals. The C++
     * caller passes ctx.supports_utf8; if false AND lang_code requires a
     * non-Latin script (ja/zh/ko/ru/uk/el/ar/he/th/hi), override
     * language_directive below to force English. Add a
     * needs_non_latin_script() helper for that table. Skipped here in
     * v0 because jsexec has no real terminal context. JS-side constant
     * is USER_UTF8 from exec/load/userdefs.js:34 (require it). */
    /* Synchronet version as the BBS actually reports it.  Falls back
     * gracefully when not in a Synchronet runtime (jsexec without a
     * system global, tests, etc.).  Macro lets the system prompt
     * anchor the bot in the real running version so it doesn't
     * fabricate version numbers from training data. */
    var sync_version = '';
    if (typeof system != 'undefined') {
        sync_version = system.full_version
                    || system.version_notice
                    || system.version
                    || '';
    }

    return {
        system_name:        system.name,
        system_op:          system.operator,
        sync_version:       sync_version,
        bot_name:           (ctx.persona && ctx.persona.name) || 'The Guru',
        alias:              (ctx.speaker && ctx.speaker.alias) || 'tester',
        real_name:          attrs.real_name || '',
        level:              attrs.level || 0,
        location:           attrs.location || '',
        lang:               lang_code,
        lang_name:          lang_full,
        language_directive: lang_full
            ? 'Respond in ' + lang_full + '.'
            : 'Respond in English ONLY -- no other languages or scripts.',
        /* memory_summary is set by chat_session()/open_session() after
         * loading persistent memory. Empty string when there's no prior
         * context (so the @memory_summary@ macro disappears cleanly).
         * Wrapped in XML-ish <long_term_memory> markers -- matches the
         * <board_archive> pattern that the system prompt knows not to
         * quote back. Replaces an earlier "PRIVATE NOTES" prose-style
         * wrapper that small models would echo verbatim into replies
         * ("Digital Man inquired about streaming mode..."). */
        memory_summary:     ctx.summary
            ? '<long_term_memory>\n' + ctx.summary + '\n</long_term_memory>'
            : '',
        /* retrieved_context is set by chat_session() via inject_retrieval()
         * after running a BM25 query against the per-persona index. Empty
         * when no index exists or no hits. */
        retrieved_context:  ctx.retrieved_context || '',
        /* Channel context: a rolling buffer of recent room chatter passed
         * in by the IRC bot (chat_llm_irc.js).  Each entry is "<nick>: text".
         * Lets the bot see what OTHERS in the room have said recently --
         * so when someone asks a follow-up that depends on the room's
         * previous turn ("yeah, like Deuce said?"), the model has the
         * context.  Per-user memory (ctx.summary above) is personal
         * continuity; channel_context is short-term shared room context.
         * Empty string when not in a multi-party setting. */
        channel_context:    ctx.channel_context
            ? '<channel_room>\n' + ctx.channel_context + '\n</channel_room>'
            : ''
    };
}

/* Convert the transcript (labeled {who, text, ts, bot}) into the
 * OpenAI-style messages array. In private mode, user turns are plain
 * content. In multinode/irc modes, user turns are prefixed with
 * "<speaker> " so the model can attribute lines in a multi-party log.
 * (We use in-content tags rather than the optional `name` field on
 * messages because provider support for `name` is inconsistent.) */
function transcript_to_messages(ctx)
{
    var msgs = [];
    var t = ctx.transcript || [];
    var tag_speakers = ctx.mode != 'private';
    for (var i = 0; i < t.length; i++) {
        var turn = t[i];
        if (turn.bot) {
            msgs.push({ role: 'assistant', content: turn.text });
        } else {
            var content = tag_speakers
                ? '<' + (turn.who || 'anon') + '> ' + turn.text
                : turn.text;
            msgs.push({ role: 'user', content: content });
        }
    }
    return msgs;
}

function build_messages(cfg, user_input, ctx, sys_override)
{
    var mctx = build_macro_ctx(ctx);
    var sys_template = sys_override
        || cfg.system_prompt
        || 'You are a regular caller on a BBS.';
    var sys = expand_macros(sys_template, mctx);

    /* Mode-specific grounding sentence.  Concrete situational
     * anchors ("you are in a public IRC channel", "speaker is X",
     * etc.) reliably keep smaller models from drifting into the
     * generic "AI assistant answering from training data" mode and
     * ignoring the retrieved board_archive chunks.  Without one,
     * private-mode replies on qwen2.5:7b were fabricating
     * version numbers, fake wiki URLs, etc. even though the
     * correct info was sitting in retrieval chunks just below. */
    if (ctx.mode != 'private') {
        var present = (ctx.participants || []).map(function (p) {
            return p.alias;
        }).join(', ');
        sys += '\n\nYou are currently in a '
             + (ctx.mode == 'irc' ? 'public IRC channel' : 'multinode chat channel')
             + '. Present: ' + (present || '(unknown)') + '. The current speaker is '
             + mctx.alias + '; address your reply to them. Other participants will see it.';
    } else {
        sys += '\n\nYou are in a private 1:1 paged chat session on '
             + mctx.system_name + ' with ' + mctx.alias
             + '.  No one else can see this conversation -- it\'s just'
             + ' you two.  ' + mctx.alias + ' is using a BBS terminal and'
             + ' is actively watching you type, so keep replies tight'
             + ' (1-3 sentences).  When ' + mctx.alias + ' asks a factual'
             + ' question, ground your answer in the retrieved'
             + ' <board_archive> chunks below -- never make up a version'
             + ' number, URL, page name, or person\'s role if it isn\'t'
             + ' in the chunks.  The wiki is the source of truth for'
             + ' Synchronet facts; cite a real chunk URL or admit you'
             + ' don\'t know.  END YOUR REPLY WITH A PERIOD, NOT A'
             + ' QUESTION (see STYLE).';
    }

    /* Mode-specific knowledge that ONLY applies in this transport.
     * Don't bake into the system_prompt template -- it'd be wrong
     * advice in the other modes. */
    if (ctx.mode == 'multinode') {
        sys += '\n\nMULTINODE CHAT COMMANDS (Synchronet-specific, current'
             + ' mode only): callers type "/" to bring up the command'
             + ' menu, then a letter -- C=change channel, E=edit,'
             + ' L=list users in this channel, W=who is online BBS-wide,'
             + ' Q=quit chat, ?=help, *=channel-action submenu.  So if'
             + ' someone asks how to leave the chat, the answer is "/Q".'
             + '  These keys DO NOT apply in IRC or private 1:1 chat.';
    } else if (ctx.mode == 'irc') {
        var irc_chan = ctx.channel ? ('the IRC channel ' + ctx.channel)
                                   : 'an IRC channel';
        sys += '\n\nIRC CONTEXT: this is ' + irc_chan + ' on the Synchronet'
             + ' IRC network -- a shared public channel, NOT the '
             + mctx.system_name + ' BBS.  The people here are IRC users from'
             + ' many different boards (or none); do NOT assume they are on '
             + mctx.system_name + ' or are its callers.  You are '
             + mctx.system_name + '\'s guru visiting the channel -- never'
             + ' greet anyone with "what brings you to ' + mctx.system_name
             + '" or otherwise talk as if they are logged into it.  Standard'
             + ' IRC client commands apply (/quit, /part, /msg, etc.), NOT'
             + ' Synchronet terminal commands.  END YOUR REPLY WITH A PERIOD,'
             + ' NOT A QUESTION (see STYLE).';
    }

    var messages = [{ role: 'system', content: sys }];
    var history_msgs = transcript_to_messages(ctx);
    for (var i = 0; i < history_msgs.length; i++)
        messages.push(history_msgs[i]);

    /* The current input gets the speaker tag in multi-party modes. */
    var input_content = (ctx.mode != 'private')
        ? '<' + mctx.alias + '> ' + user_input
        : user_input;
    messages.push({ role: 'user', content: input_content });
    return messages;
}

function chat_openai(cfg, messages, opts)
{
    opts = opts || {};
    /* Streaming is opt-IN. Internal callers (llm_open, summarize_old_turns)
     * dispatch() without opts and MUST get a buffered, non-streamed reply --
     * otherwise their text gets emitted to the terminal via
     * console.simulate_type before the high-level caller has a chance to
     * decide how/whether to display it (which double-types openings and
     * leaks summarizer output into the chat). chat_session() passes
     * streaming=true explicitly when it wants live typing. */
    var streaming = (opts.streaming === true);

    var req = {
        model:       cfg.model,
        messages:    messages,
        max_tokens:  cfg.max_tokens,
        temperature: cfg.temperature,
        stream:      streaming
    };
    /* Ollama-specific extension; other OpenAI-compat backends ignore it. */
    if (cfg.keep_alive) req.keep_alive = cfg.keep_alive;
    var body = JSON.stringify(req);

    var headers = { 'Authorization': 'Bearer ' + cfg.api_key };
    var http = new HTTPRequest(undefined, undefined, headers, cfg.timeout);

    if (streaming) {
        /* Streaming: parse SSE events as chunks arrive, emit each token's
         * text via console.simulate_type() so the bot appears to type as
         * the model generates. Returns the assembled full reply for
         * memory storage + logging. */
        var full_reply = '';
        var sse_buffer = '';
        var have_console = (typeof console != 'undefined'
                            && typeof console.simulate_type == 'function');
        var typos       = opts.simulate_typos !== false;
        var speed       = opts.typing_speed_factor || 1.0;
        /* Breadcrumbs for profile line. */
        var t_stream_start = system.timer;
        var t_first_chunk  = null;
        var chunks_count   = 0;
        var deltas_count   = 0;

        /* One-question-max enforcement via live "stop after first ?".
         * Chars stream out char-by-char as they arrive (preserving
         * TTFC).  Once the first '?' is emitted, we switch to drain
         * mode: keep reading the SSE so the connection completes but
         * drop everything else on the floor.  Tradeoff: any
         * statement the model emits AFTER its first question is
         * lost too, but 7B-model replies almost always end with the
         * question -- subsequent text is usually more questions or
         * filler.  Net effect: realistic chat where the bot ends
         * with one question, no extra "anything else? what about X?"
         * piling on. */
        var stopped = false;
        /* Per-emit buffer used by simulate_type's per-call timing
         * model.  Chars come in chunks of N from each SSE delta;
         * we want to emit per-char (live) but call simulate_type
         * once per delta with the chars we kept. */

        http.PostStreaming(cfg.endpoint, body, function (chunk) {
            chunks_count++;
            if (t_first_chunk === null) t_first_chunk = system.timer - t_stream_start;
            sse_buffer += chunk;
            /* Parse complete SSE events (terminated by "\n\n") from buffer. */
            while (true) {
                var brk = sse_buffer.indexOf('\n\n');
                if (brk < 0) break;
                var event = sse_buffer.substring(0, brk);
                sse_buffer = sse_buffer.substring(brk + 2);
                /* An event may have multiple lines; we only care about
                 * "data: <json>" lines. */
                var lines = event.split('\n');
                for (var li = 0; li < lines.length; li++) {
                    var line = lines[li];
                    if (line.indexOf('data: ') !== 0) continue;
                    var payload = line.substring(6);
                    if (payload == '[DONE]') continue;
                    var json;
                    try { json = JSON.parse(payload); } catch (e) { continue; }
                    if (!json.choices || !json.choices.length) continue;
                    var delta = json.choices[0].delta;
                    if (!delta || !delta.content) continue;
                    var text = delta.content;
                    /* Sanitize per-token: strips emoji, backticks. */
                    text = sanitize_reply(text);
                    if (!text) continue;
                    deltas_count++;
                    if (stopped) continue;   /* drain after first '?' */

                    /* Build emit-this-delta string char-by-char:
                     *   - collapse any '\n' to ' ' (no paragraph
                     *     breaks within a reply)
                     *   - stop emitting at first '?' (single-question
                     *     rule); include the '?' itself, drop rest
                     *     of this delta and all future deltas */
                    var out = '';
                    for (var ci = 0; ci < text.length; ci++) {
                        var ch = text.charAt(ci);
                        if (ch === '\n' || ch === '\r') ch = ' ';
                        out += ch;
                        if (ch === '?') { stopped = true; break; }
                    }
                    if (!out) continue;
                    full_reply += out;
                    if (have_console)
                        console.simulate_type(out, typos, speed);
                    else
                        write(out);
                }
            }
        }, 'application/json');

        if (http.response_code != 200)
            throw new Error('HTTP ' + http.response_code);

        /* Stash breadcrumbs on opts for chat_session to pick up. */
        opts._stream_used    = true;
        opts._have_console   = have_console;
        opts._chunks_count   = chunks_count;
        opts._deltas_count   = deltas_count;
        opts._ttfc           = t_first_chunk;

        return full_reply;
    }
    opts._stream_used = false;

    /* Non-streaming fallback (kept for backends that don't support SSE
     * or for testing). Same shape as the original Post() based call. */
    var raw = http.Post(cfg.endpoint, body, undefined, undefined,
                        'application/json');
    if (http.response_code != 200)
        throw new Error('HTTP ' + http.response_code + ': ' + raw);
    var resp = JSON.parse(raw);
    if (!resp.choices || !resp.choices.length)
        throw new Error('no choices in response: ' + raw);
    return resp.choices[0].message.content;
}

/* Deterministic pre-classifier for high-signal tool routing.  Maps
 * a small set of frequent user-input patterns to a (tool, args) pair
 * the dispatcher will pre-execute, bypassing qwen2.5:7b's unreliable
 * tool-choice.  Only fires when the pattern match is high-confidence
 * -- ambiguous inputs fall through to the model.  Returns null when
 * no rule matches.
 *
 * Why this exists: even with strong "Triggers:" lists in tool
 * descriptions, the 7B model misroutes ~30% of the obvious cases
 * (e.g. "tell me about Funtopia BBS" -> external_archives instead
 * of bbs_directory, "what's the uptime of Vertrauen?" -> git_issues,
 * "Steve Jackson Games raid" -> no tool at all + fabricated year).
 * The classifier closes the gap on the most repeatable patterns.
 *
 * Format: { tool: <name>, args: {...} } or null. */
function classify_intent(input) {
    var s = String(input || '').replace(/^\s+|\s+$/g, '');
    if (!s) return null;
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

    /* --- git_commits (recent / search) -- check BEFORE by_author so
     * "what's been committed lately" doesn't accidentally land in
     * the by_author pattern with author="been". --- */

    /* "what's been committed lately" / "any recent Synchronet commits" */
    if (/^\s*(?:what(?:['']?s|\s+has)\s+been\s+committed|any\s+recent\s+(?:synchronet\s+)?commits|recent\s+(?:synchronet\s+)?commits)(?:\s+to\s+[\w.\/ -]+?)?(?:\s+(?:lately|recently))?\s*[?.!]*\s*$/i.test(s)) {
        return { tool: 'git_commits',
                 args: { kind: 'recent', limit: 5 } };
    }
    /* "any commits about/mentioning <topic>" -- search mode */
    m = s.match(/(?:any\s+)?(?:recent\s+)?commits?\s+(?:about|mentioning|on|regarding|for)\s+(.+?)\s*[?.!]*\s*$/i);
    if (m) {
        var topic = m[1].replace(/^\s+|\s+$/g, '');
        if (topic) return { tool: 'git_commits',
                            args: { kind: 'search', query: topic, limit: 5 } };
    }

    /* --- git_commits (by_author) -- "what has <X> been working on" --- */

    /* "what has/is/'s <author> [been] <verb> [lately|to X|...]"
     * Captures up to 3-word author names; llm_tools.js's
     * _author_search_terms handles confusable-fold + alias expansion
     * (DigitalMan -> rswindell, Cyrillic-Deuce, etc.).  No end anchor
     * -- the optional "lately" / "to Synchronet" / etc. tail is
     * ignored.  Reject "been" as author to prevent false-positive
     * on the no-author "what's been committed" form above. */
    m = s.match(/^\s*what(?:['']?s|\s+has|\s+is|\s+have)\s+(\S+(?:\s+\S+){0,2}?)\s+(?:been\s+)?(?:working\s+on|up\s+to|doing|contributing|committing|coding|made|done|making)\b/i);
    if (m) {
        var auth = m[1].replace(/^\s+|\s+$/g, '');
        if (auth && _looks_like_proper_noun(auth) && !/^been$/i.test(auth)) {
            return { tool: 'git_commits',
                     args: { kind: 'by_author', author: auth, limit: 5 } };
        }
    }
    /* "recent commits by <X>" / "commits from <X>" / "commits of <X>" */
    m = s.match(/(?:recent\s+)?commits?\s+(?:by|from|of)\s+(.+?)\s*[?.!]*\s*$/i);
    if (m) {
        var auth2 = m[1].replace(/^\s+|\s+$/g, '');
        if (auth2 && _looks_like_proper_noun(auth2)) {
            return { tool: 'git_commits',
                     args: { kind: 'by_author', author: auth2, limit: 5 } };
        }
    }

    /* --- git_issues --- */

    /* "issue #<N>" / "GitLab issue #N" / "what's issue N about" */
    m = s.match(/(?:gitlab\s+)?issue\s+#?(\d+)/i);
    if (m) {
        return { tool: 'git_issues',
                 args: { kind: 'lookup', iid: parseInt(m[1], 10) } };
    }
    /* "recent (open|closed) issues" / "what are the (open|closed) issues" */
    m = s.match(/(?:what\s+are\s+the\s+)?(?:most\s+)?recent\s+(open|closed)?\s*(?:synchronet\s+)?issues?\s*[?.!]*\s*$/i);
    if (m) {
        var state = m[1] ? m[1].toLowerCase() : 'opened';
        if (state === 'open') state = 'opened';
        return { tool: 'git_issues',
                 args: { kind: 'recent', state: state, limit: 5 } };
    }
    /* "issues about <topic>" -- search */
    m = s.match(/(?:any\s+)?(?:gitlab\s+)?issues?\s+(?:about|mentioning|on|regarding|for)\s+(.+?)\s*[?.!]*\s*$/i);
    if (m) {
        var itopic = m[1].replace(/^\s+|\s+$/g, '');
        if (itopic) return { tool: 'git_issues',
                             args: { kind: 'search', query: itopic, limit: 5 } };
    }

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

/* Strip trailing "BBS" / "bbs" suffix and trim whitespace from a
 * captured name fragment.  "Funtopia BBS" -> "Funtopia".  Bypass
 * trailing punctuation also. */
function _strip_bbs_suffix(name) {
    if (!name) return '';
    return String(name)
        .replace(/[?.!,]+$/g, '')
        .replace(/\s+bbs\s*$/i, '')
        .replace(/^\s+|\s+$/g, '');
}

/* Heuristic: looks like a proper noun (BBS name, sysop name, etc.).
 * Reject generic/single-letter inputs and obvious questions.  Used
 * to gate "tell me about X" from firing on "tell me about it" or
 * "tell me about the BBS scene". */
function _looks_like_proper_noun(s) {
    if (!s || s.length < 2) return false;
    /* Allow "The <Noun>" -- some BBSes are named "The X-Bit BBS",
     * "The Bread Board System", etc.  Strip the leading article
     * for the test so the body still needs to look proper-noun-y. */
    var test = String(s).replace(/^\s*the\s+/i, '').replace(/^\s+|\s+$/g, '');
    if (!test || test.length < 2) return false;
    /* Reject if it starts with a pronoun / article / question word
     * (after the leading-"the" strip). */
    if (/^(?:it|that|this|a|an|some|any|all|my|your|our|their|them|there)\b/i.test(test))
        return false;
    /* Reject if it looks like a phrase about a general topic */
    if (/\b(?:scene|culture|history|software|programs|systems|files|world|era|community|stuff|things|everything)\b/i.test(test))
        return false;
    /* Reject if "BBS" appears mid-name -- _strip_bbs_suffix strips
     * trailing BBS, so a remaining "BBS" means we over-captured
     * (e.g. "the BBS with the hostname X" got greedily matched). */
    if (/\bbbs\b/i.test(test))
        return false;
    return true;
}

/* Heuristic: which arg key to use for an ambiguous "<X> BBSes".
 * Known networks vs software.  Case-insensitive substring match. */
function _looks_like_network(w) {
    return /^(?:fidonet|fido|dove-net|dovenet|usenet|sync|syncnet|csdn|micronet|filegate|raidernet|agoranet|fsxnet)$/i.test(w);
}

/* Heuristic: does <w> name an operating system (not BBS software)?
 * Routes "how many BBSes run <OS>" to bbs_directory's os filter
 * instead of mis-binding the OS as a software/network value. */
function _looks_like_os(w) {
    return /^(?:(?:free|open|net)bsd|linux|debian|ubuntu|raspbian|raspberry\s*pi|windows|win(?:32|64|nt)?|mac\s*os(?:\s*x)?|macos|osx|darwin|os\/?2|ms-?dos|dos|solaris|haiku)$/i.test(String(w).replace(/^\s+|\s+$/g, ''));
}

/* Ollama-native /api/chat path.  Mirrors chat_openai's surface (return
 * value, opts breadcrumbs) but speaks Ollama's request/response shape:
 *   request:  { model, messages, stream, think, keep_alive, options:{
 *                 temperature, num_predict } }
 *   response: { message:{ content }, done, eval_count, ... } (non-stream)
 *             or NDJSON (one JSON object per line) when stream=true.
 * Used to send think:false for qwen3-style thinking models, which the
 * OpenAI-compat /v1/chat/completions endpoint silently ignores. */
function chat_ollama(cfg, messages, opts)
{
    opts = opts || {};
    var streaming = (opts.streaming === true);

    /* Lazy-load llm_tools.js the first time we need it.  Defined as
     * a side-effect of load(): puts llm_tools_defs (Array) and
     * llm_tools_execute (function) into the current scope.  Wrapped
     * in try/catch so a missing llm_tools.js disables tools cleanly
     * rather than breaking chat. */
    if (cfg.tools_enabled !== false && typeof llm_tools_defs == 'undefined') {
        try {
            load('llm_tools.js');
        } catch (e) {
            log(LOG_WARNING,
                'chat_llm: tools requested but llm_tools.js load failed: ' + e);
        }
    }
    var tools_available = (typeof llm_tools_defs != 'undefined')
                       && llm_tools_defs && llm_tools_defs.length
                       && (cfg.tools_enabled !== false)
                       && !streaming;  /* tools only on non-streaming path */

    var req = {
        model:    cfg.model,
        messages: messages,
        stream:   streaming,
        think:    false,
        options:  {
            temperature: cfg.temperature,
            num_predict: cfg.max_tokens,
            /* num_ctx: 0/unset => omitted (JSON.stringify drops an
             * undefined value), so Ollama uses its server default
             * (~4096).  Set cfg.num_ctx in chat_llm.ini when the system
             * prompt + RAG exceed the default -- otherwise llama.cpp
             * keeps the prompt's tail and truncates the front, where the
             * identity and style rules live (observed: a 4698-token
             * system prompt capped at 4096 dropped the "you are a
             * co-sysop, NOT an AI assistant" block).
             * History: the 4K default was once relied on as a forcing
             * function for tool fallback -- a larger window let the
             * model answer state queries from RAG chunks instead of
             * calling the directory/stats tools (sometimes fabricating).
             * After raising num_ctx, re-run the tool/archive regression
             * to confirm routing still holds. */
            num_ctx: cfg.num_ctx || undefined
        }
    };
    if (cfg.keep_alive) req.keep_alive = cfg.keep_alive;
    if (tools_available) req.tools = llm_tools_defs;
    var body = JSON.stringify(req);

    var headers = { 'Authorization': 'Bearer ' + cfg.api_key };
    var http = new HTTPRequest(undefined, undefined, headers, cfg.timeout);

    if (streaming) {
        /* NDJSON streaming: one complete JSON object per line, terminated
         * by '\n'.  Each line has shape {message:{content:"partial"},done:bool};
         * the final line has done=true and aggregate stats.  Same '?'-stop
         * and emoji-strip rules as the OpenAI SSE path so behavior stays
         * consistent across providers. */
        var full_reply = '';
        var line_buf = '';
        var have_console = (typeof console != 'undefined'
                            && typeof console.simulate_type == 'function');
        var typos       = opts.simulate_typos !== false;
        var speed       = opts.typing_speed_factor || 1.0;
        var t_stream_start = system.timer;
        var t_first_chunk  = null;
        var chunks_count   = 0;
        var deltas_count   = 0;
        var stopped = false;

        http.PostStreaming(cfg.endpoint, body, function (chunk) {
            chunks_count++;
            if (t_first_chunk === null) t_first_chunk = system.timer - t_stream_start;
            line_buf += chunk;
            var nl;
            while ((nl = line_buf.indexOf('\n')) >= 0) {
                var line = line_buf.substring(0, nl);
                line_buf = line_buf.substring(nl + 1);
                if (!line) continue;
                var json;
                try { json = JSON.parse(line); } catch (e) { continue; }
                var text = json.message && json.message.content;
                if (!text) continue;
                text = sanitize_reply(text);
                if (!text) continue;
                deltas_count++;
                if (stopped) continue;
                var out = '';
                for (var ci = 0; ci < text.length; ci++) {
                    var ch = text.charAt(ci);
                    if (ch === '\n' || ch === '\r') ch = ' ';
                    out += ch;
                    if (ch === '?') { stopped = true; break; }
                }
                if (!out) continue;
                full_reply += out;
                if (have_console)
                    console.simulate_type(out, typos, speed);
                else
                    write(out);
            }
        }, 'application/json');

        if (http.response_code != 200)
            throw new Error('HTTP ' + http.response_code);

        opts._stream_used    = true;
        opts._have_console   = have_console;
        opts._chunks_count   = chunks_count;
        opts._deltas_count   = deltas_count;
        opts._ttfc           = t_first_chunk;
        return full_reply;
    }
    opts._stream_used = false;

    /* Non-streaming path.  When tools are enabled, the model may emit
     * a `tool_calls` response instead of plain text.  In that case:
     *   1. execute each tool via llm_tools_execute
     *   2. append the assistant's tool_calls message + a `tool` role
     *      message per result onto our messages array
     *   3. re-POST with the extended messages so the model can produce
     *      a final reply that integrates the tool results
     *   4. loop, but cap at MAX_TOOL_ITERS to prevent runaway / loops
     *      where the model keeps calling tools without converging.
     * Tool calls are tracked on opts._tool_calls (array of names) so
     * the [prof] line shows when tools fired. */
    var MAX_TOOL_ITERS = 4;
    opts._tool_calls = [];
    var current_messages = messages;
    var current_body     = body;

    /* Deterministic pre-classifier.  If the user's input matches a
     * high-confidence pattern, pre-execute the matching tool and
     * inject the result into the message stream BEFORE the first
     * model call -- the model then just formats the reply, with no
     * opportunity to misroute or skip the tool.  Skip when tools
     * aren't available (no llm_tools_execute loaded). */
    if (tools_available && typeof llm_tools_execute == 'function') {
        var _ctx0 = (opts && opts._ctx) || {};
        var _user_input = (opts && opts._user_input) || '';
        if (_user_input) {
            var intent = null;
            try { intent = classify_intent(_user_input); } catch (e) {
                log(LOG_WARNING,
                    'classify_intent threw: ' + e + ' for input ' + _user_input);
            }
            if (intent) {
                var pre_env = {
                    speaker:           _ctx0.speaker,
                    mode:              _ctx0.mode,
                    channel:           _ctx0.channel || '',
                    channel_members:   _ctx0.channel_members || {},
                    seen_members:      _ctx0.seen_members || {},
                    relay_max_pending: cfg.relay_max_pending,
                    relay_path:        _ctx0.relay_path,
                    norelay_path:      _ctx0.norelay_path
                };
                var pre_result;
                try {
                    pre_result = llm_tools_execute(intent.tool,
                                                    intent.args,
                                                    pre_env);
                } catch (e2) {
                    pre_result = JSON.stringify(
                        { error: 'pre-classifier exec: ' + e2 });
                }
                /* relay_message: speak the tool's own result text verbatim
                 * and skip the model turn entirely.  The tool already
                 * returns first-person bot speech for every outcome --
                 * refusals (opt-out / unknown nick / per-recipient or
                 * per-sender cap) carry .error, a successful queue carries
                 * .note ("Stored. Will deliver the next time X speaks...").
                 * qwen2.5:7b has been observed DISCARDING a refusal result
                 * and fabricating a delivery promise instead ("I'll make
                 * sure X knows"), telling the sender a message was relayed
                 * that the tool actually refused (e.g. to an opted-out
                 * recipient).  Returning the tool text deterministically
                 * removes any chance of the model misreporting whether the
                 * relay happened. */
                if (intent.tool == 'relay_message') {
                    var _rr = null;
                    try { _rr = JSON.parse(String(pre_result)); } catch (e3) {}
                    var _say = _rr && (_rr.error || _rr.note);
                    if (_say) {
                        opts._tool_calls.push('!pre:' + intent.tool
                            + '(' + JSON.stringify(intent.args) + ')');
                        return _say;
                    }
                }
                var pre_id = 'preclass_0';
                current_messages = current_messages.slice();
                current_messages.push({
                    role:       'assistant',
                    content:    '',
                    tool_calls: [{
                        id:   pre_id,
                        type: 'function',
                        'function': {
                            name:      intent.tool,
                            arguments: intent.args
                        }
                    }]
                });
                current_messages.push({
                    role:         'tool',
                    content:      String(pre_result),
                    tool_call_id: pre_id
                });
                opts._tool_calls.push('!pre:' + intent.tool
                    + '(' + JSON.stringify(intent.args) + ')');
                /* Rebuild request body with the pre-injected turns. */
                var pre_req = {
                    model:    cfg.model,
                    messages: current_messages,
                    stream:   false,
                    think:    false,
                    options:  {
                        temperature: cfg.temperature,
                        num_predict: cfg.max_tokens,
                        num_ctx:     cfg.num_ctx || undefined
                    }
                };
                if (cfg.keep_alive)  pre_req.keep_alive = cfg.keep_alive;
                if (tools_available) pre_req.tools     = llm_tools_defs;
                current_body = JSON.stringify(pre_req);
            }
        }
    }

    for (var iter = 0; iter < MAX_TOOL_ITERS; iter++) {
        var raw = http.Post(cfg.endpoint, current_body, undefined, undefined,
                            'application/json');
        if (http.response_code != 200)
            throw new Error('HTTP ' + http.response_code + ': ' + raw);
        var resp = JSON.parse(raw);
        if (!resp.message)
            throw new Error('no message in response: ' + raw);

        var tool_calls = resp.message.tool_calls;
        if (!tool_calls || !tool_calls.length) {
            /* Plain text reply -- we're done. */
            if (resp.message.content === undefined)
                throw new Error('message has no content or tool_calls: ' + raw);
            var content = resp.message.content;
            /* Empty content after the model called at least one tool is
             * a known 7B failure mode -- the model receives a tool
             * result it doesn't know how to summarize and emits an
             * empty string instead of prose.  Coerce into a synthetic
             * "try again" prompt so the caller doesn't deliver a blank
             * line to IRC.  Log so we know it happened. */
            if (iter > 0 && (content === '' || /^\s*$/.test(content))) {
                log(LOG_WARNING, 'chat_llm: empty content from model '
                    + 'after ' + iter + ' tool iter(s); raw response='
                    + raw.slice(0, 500));
                return '(I got the data but did not produce a summary -- '
                     + 'try rephrasing the question.)';
            }
            return content;
        }

        /* Model wants to call one or more tools.  Append the assistant's
         * tool_calls turn, then a `tool` role message for each result. */
        current_messages = current_messages.slice();   /* don't mutate caller */
        current_messages.push({
            role:       'assistant',
            content:    resp.message.content || '',
            tool_calls: tool_calls
        });
        /* Context object the tools can consult (relay_message uses
         * env.speaker / env.channel / env.channel_members to validate
         * the recipient -- without these the LLM could fabricate an
         * attribution).  Optional: tools that don't need it ignore it.
         * ctx is stashed on opts by llm_chat() / open_session(); fall
         * back to {} so this is safe in non-IRC contexts too. */
        var _ctx = (opts && opts._ctx) || {};
        var tc_env = {
            speaker:         _ctx.speaker,
            mode:            _ctx.mode,
            channel:         _ctx.channel || '',
            channel_members: _ctx.channel_members || {},
            /* Per-channel ever-seen nicks (the bot persists everyone
             * it has ever observed in a channel).  Used by
             * relay_message to allow "tell <X> when you see <X>"
             * for a nick that's known but not currently connected. */
            seen_members:    _ctx.seen_members || {},
            relay_max_pending: _ctx.relay_max_pending || cfg.relay_max_pending,
            relay_path:        _ctx.relay_path,
            norelay_path:      _ctx.norelay_path
        };
        for (var tci = 0; tci < tool_calls.length; tci++) {
            var tc      = tool_calls[tci];
            var tc_name = (tc['function'] && tc['function'].name) || tc.name;
            var tc_args = (tc['function'] && tc['function'].arguments) || tc.arguments || {};
            opts._tool_calls.push(tc_name + '(' + JSON.stringify(tc_args) + ')');
            var tc_result;
            if (typeof llm_tools_execute == 'function') {
                tc_result = llm_tools_execute(tc_name, tc_args, tc_env);
            } else {
                tc_result = JSON.stringify({
                    error: 'tool executor not loaded'
                });
            }
            current_messages.push({
                role:    'tool',
                content: String(tc_result),
                tool_call_id: tc.id || ('call_' + tci)
            });
        }
        /* Re-build request body with extended messages.  Keep tools in
         * the request so the model COULD chain another tool call if
         * needed -- the iteration cap above bounds total round-trips. */
        var follow_req = {
            model:    cfg.model,
            messages: current_messages,
            stream:   false,
            think:    false,
            options:  {
                temperature: cfg.temperature,
                num_predict: cfg.max_tokens,
                num_ctx:     cfg.num_ctx || undefined
            }
        };
        if (cfg.keep_alive)  follow_req.keep_alive = cfg.keep_alive;
        if (tools_available) follow_req.tools      = llm_tools_defs;
        current_body = JSON.stringify(follow_req);
    }
    /* If we exhausted MAX_TOOL_ITERS without a text reply, give the
     * user something rather than throw. */
    return '(tool-call loop did not converge -- try rephrasing)';
}

function _dispatch_provider(cfg, messages, opts)
{
    switch (cfg.provider) {
    case 'openai':
        return chat_openai(cfg, messages, opts);
    case 'ollama':
        return chat_ollama(cfg, messages, opts);
    /* case 'gemini': return chat_gemini(cfg, messages, opts); */
    default:
        throw new Error('unknown provider: ' + cfg.provider);
    }
}

/* dispatch() with endpoint failover.  If cfg.endpoint_fallback is set and
 * the primary endpoint throws (transport error / non-200 -- the provider
 * functions throw "HTTP <code>" on any failure), retry once against the
 * fallback.  A short cooldown after a primary failure routes straight to
 * the fallback on subsequent turns so an outage doesn't add the primary's
 * full timeout to every reply. */
function dispatch(cfg, messages, opts)
{
    var fb = cfg.endpoint_fallback;
    if (!fb || fb === cfg.endpoint)
        return _dispatch_provider(cfg, messages, opts);   /* no fallback */

    function _via(ep) {
        if (ep === cfg.endpoint) return cfg;
        var c = {}; for (var k in cfg) c[k] = cfg[k];
        c.endpoint = ep;
        return c;
    }

    var now = time();
    /* Primary recently failed -> skip it, go straight to the fallback. */
    if (_primary_down_until > now) {
        try {
            return _dispatch_provider(_via(fb), messages, opts);
        } catch (e) {
            _primary_down_until = 0;   /* fallback down too -- re-probe primary next turn */
            throw e;
        }
    }

    try {
        var r = _dispatch_provider(cfg, messages, opts);
        _primary_down_until = 0;       /* primary healthy */
        return r;
    } catch (e) {
        _primary_down_until = now + ENDPOINT_FAILOVER_COOLDOWN;
        log(LOG_WARNING, 'chat_llm: primary endpoint ' + cfg.endpoint
            + ' failed (' + e + '); failing over to ' + fb);
        return _dispatch_provider(_via(fb), messages, opts);
    }
}

/* Strip backtick-escape codes that the legacy guru output system uses
 * for flow control (`Q hangup, `H quit, etc.) so user-influenced LLM
 * output can never trigger them. Control codes only come from the C++
 * dispatcher, not from the model.  Also strips emoji / pictographic
 * Unicode -- the system prompt forbids them but small models routinely
 * ignore that rule; this is the belt-and-suspenders backstop. */
function sanitize_reply(s)
{
    if (!s) return s;
    /* Strip backticks.  The legacy guru output system used `X codes
     * for flow control (`Q hangup, `H quit, etc.) but a naive
     * `/`./g` here also ate markdown-code-span delimiters' following
     * character ("the `configure FidoNet` utility" -> "the onfigure
     * FidoNetutility").  The output layer doesn't actually interpret
     * backtick codes from LLM-streamed text (only the legacy guru
     * pattern engine did), so removing JUST the backticks is safe and
     * preserves the surrounding text. */
    s = String(s).replace(/`/g, '');
    /* Emoji and pictographs.  Covers:
     *   - Surrogate-pair emoji in supplementary planes (U+1F000-U+1FFFF):
     *     high surrogate 0xD83C-0xD83E + any low surrogate.
     *   - Misc symbols / dingbats / arrows / weather / chess in
     *     U+2600-U+27BF.
     *   - Variation selectors (U+FE00-U+FE0F) used as emoji presentation
     *     modifiers, and the U+200D zero-width joiner that glues
     *     compound emoji together. */
    s = s.replace(/[\uD83C-\uD83E][\uDC00-\uDFFF]/g, '');
    s = s.replace(/[\u2600-\u27BF]/g, '');
    s = s.replace(/[\uFE00-\uFE0F]/g, '');
    s = s.replace(/\u200D/g, '');
    return s;
}

/* Post-process the FINAL assembled reply (NOT per-token chunks).
 * Several jobs the prompt can't reliably enforce on a 7B model:
 *   1. STRIP tool-name / JSON-call leakage.  Model occasionally
 *      writes "this_bbs({kind: stats})" or a markdown ```json block
 *      directly into the user-facing reply.  Regex them out.
 *   2. STRIP markdown bullets ("- item" / "* item") and numbered
 *      lists ("1. item").  Convert to comma-separated inline prose.
 *   3. STRIP trailing engagement-padding questions ("How can I assist
 *      you today?", "Anything else?", "Want to know more?").  These
 *      are explicitly forbidden by STYLE but the model keeps emitting
 *      them; drop the entire last sentence when it matches.
 *   4. ONE question maximum across the whole reply.  Keep the FIRST
 *      sentence ending in '?', drop later '?'-sentences.
 *   5. NO paragraph breaks within a reply.  Collapse "\n\n" to a
 *      single space.
 * Called from chat_session / open_session AFTER the streaming
 * assembly is complete, NOT in the per-token sanitize_reply path
 * (where post-processing the assembled state would conflict with
 * what was already emitted to the terminal). */

/* Patterns for the trailing-question filter.  Matched against the
 * LAST sentence (after trimming).  If the last sentence ends in '?'
 * AND matches one of these, drop it.  Kept narrow to avoid eating
 * legitimate clarifying questions (which usually have specific
 * subject matter, not generic "anything else"/"how can I help"). */
var ENGAGEMENT_QUESTION_RX = new RegExp(
    '^(.{0,40})(?:how (?:can|may) i (?:assist|help)|'
  + 'anything else|'
  + 'is there anything (?:else|more)|'
  + 'want to (?:know|hear) more|'
  + 'would you like (?:me to |to know |to hear )?(?:more|anything)|'
  + 'how can i (?:assist|help) you (?:further|today|with anything else)|'
  + 'how (?:about|else) (?:can|may) i|'
  + 'feel free to (?:ask|let me know|share)|'
  + 'let me know if you need (?:more|anything|further)|'
  + 'any (?:other|specific|more) (?:questions|ideas|thoughts|issues)|'
  + 'got any (?:other|more) questions)\\b',
    'i'
);

function final_reply_postprocess(s)
{
    if (!s) return s;

    /* (0) HTML-entity decode.  The 7B model occasionally emits escaped
     * characters -- "&lt;http://...&gt;" when it autolinks a URL in
     * angle brackets, "&amp;" inside a URL, and sometimes DOUBLE-escaped
     * forms ("&amp;lt;...&amp;gt;").  Output is plain text (IRC /
     * terminal) and the model never legitimately wants a literal "&lt;"
     * in a chat reply, so decode aggressively: iterate until the string
     * stops changing (bounded) so multiply-escaped entities fully
     * resolve -- the 7B has been seen QUADRUPLE-escaping
     * ("&amp;amp;amp;lt;"), so the cap is generous.  Runs BEFORE
     * markdown-strip and URL validation -- the literal entities must
     * not reach the user, and "&amp;" would break URL matching against
     * the curated/wiki sets. */
    var ent_prev, ent_passes = 0;
    do {
        ent_prev = s;
        s = s.replace(/&lt;/g, '<').replace(/&gt;/g, '>')
             .replace(/&quot;/g, '"').replace(/&#0*39;/g, "'")
             .replace(/&apos;/g, "'").replace(/&amp;/g, '&');
    } while (s !== ent_prev && ++ent_passes < 6);

    /* (1) Strip tool-name / JSON leakage.  The model sometimes writes
     * literal tool-call syntax into the user-facing content instead
     * of (or in addition to) emitting a structured tool_call.
     *
     * - ```json ... ``` fenced code blocks
     * - bare {"name": "...", "arguments": ...} JSON objects
     * - inline "this_bbs({kind: 'stats'})" pseudo-calls
     * - phrases like "Based on the response from <toolname>(...)" */
    s = s.replace(/```\s*json[\s\S]*?```/gi, '');
    s = s.replace(/```[\s\S]*?```/g, '');
    /* Strip the "Based on the response from X(args)" preamble but
     * leave the rest of the sentence intact -- common leak pattern
     * where the model narrates the tool call before delivering the
     * answer.  Eats an optional trailing comma+space so the
     * follow-on phrase ("for Vertrauen, here are...") reads cleanly. */
    s = s.replace(/\bbased on (?:the )?(?:response|result|output) from\s+\w+\([^)]*\)\s*,?\s*/gi, '');
    /* Inline pseudo-calls: this_bbs({kind: "stats"}) etc. */
    s = s.replace(/\b(?:external_archives|this_bbs|bbs_directory|lookup_bbs|list_bbses|relay_message|get_bbs_status|git_issues|git_commits)\s*\([^)]*\)/g, '');
    /* Bare tool-name leaks (no parens): the model sometimes names the
     * internal tool in prose -- "linked from external_archives:", "the
     * this_bbs tool shows", "via git_commits".  Strip an optional
     * lead-in preposition/article, the tool name, an optional "tool",
     * and a trailing colon.  The tool names are distinctive enough that
     * this won't touch ordinary prose.  (Leftover whitespace is
     * collapsed by the cleanup pass near the end of this function.) */
    s = s.replace(/(?:\b(?:from|via|using|through|per|in|on|see|the)\s+)?(?:the\s+)?\b(?:external_archives|this_bbs|bbs_directory|lookup_bbs|list_bbses|relay_message|get_bbs_status|git_issues|git_commits)\b(?:\s+tool)?\s*:?/gi, ' ');
    /* Rewrite Synchronet GitLab project URLs that the model emitted
     * with gitlab.com (its training-data default) -- the project is
     * actually self-hosted at gitlab.synchro.net.  Only rewrites
     * the project-path prefix to avoid touching legitimate
     * gitlab.com references elsewhere. */
    s = s.replace(/\bgitlab\.com\/main\/sbbs\b/g, 'gitlab.synchro.net/main/sbbs');
    /* Bare {"name": "...", "arguments": {...}} blobs (the Ollama
     * tool-call format leaked as content) -- and the "json" prefix
     * that often precedes them. */
    s = s.replace(/\bjson\s*\n?\s*\{[^}]*\}/gi, '');
    s = s.replace(/\{\s*"name"\s*:\s*"[^"]+"\s*,\s*"arguments"\s*:\s*\{[^}]*\}\s*\}/g, '');
    s = s.replace(/\{\s*"(?:kind|name|mode|recipient|sub|group|lib|section)"\s*:\s*"[^"]*"[^}]*\}/g, '');

    /* (2) Strip markdown bullets / numbered lists.  Convert to inline
     * comma-separated prose.  Handles both "- item" and "* item" and
     * "1. item" forms, with optional indent. */
    /* First: bullet lines as a contiguous block -> comma-joined. */
    s = s.replace(/(?:^|\n)([ \t]*[-*]\s+[^\n]+(?:\n[ \t]*[-*]\s+[^\n]+)*)/g,
        function (m, block) {
            var items = block.split(/\n/).map(function (ln) {
                return ln.replace(/^\s*[-*]\s+/, '').replace(/^\s+|\s+$/g, '');
            }).filter(function (x) { return x.length; });
            return (m.charAt(0) === '\n' ? ' ' : '') + items.join(', ');
        });
    /* Numbered lists too: "1. foo\n2. bar" -> "foo, bar". */
    s = s.replace(/(?:^|\n)([ \t]*\d+\.\s+[^\n]+(?:\n[ \t]*\d+\.\s+[^\n]+)*)/g,
        function (m, block) {
            var items = block.split(/\n/).map(function (ln) {
                return ln.replace(/^\s*\d+\.\s+/, '').replace(/^\s+|\s+$/g, '');
            }).filter(function (x) { return x.length; });
            return (m.charAt(0) === '\n' ? ' ' : '') + items.join(', ');
        });
    /* Strip markdown links: [text](url) -> "text url" so the URL
     * still reaches the IRC user but in plain form.  When the text
     * is just "Link" or similarly uninformative, drop it and keep
     * only the URL. */
    s = s.replace(/\[([^\]]+)\]\((https?:\/\/[^)]+)\)/g, function (m, t, u) {
        if (/^(link|here|this|details?|more)$/i.test(t.replace(/^\s+|\s+$/g, '')))
            return u;
        return t + ' ' + u;
    });

    /* Strip leftover **bold** and *italic* markers. */
    s = s.replace(/\*\*([^*]+)\*\*/g, '$1');
    /* *italic* markers (but NOT **bold** -- those caught above) and NOT
     * adjacent-word-char on either side.  SpiderMonkey 1.8.5 has no
     * regex lookbehind, so capture the prefix char and put it back. */
    s = s.replace(/(^|[^\w*])\*([^*\n]+)\*(?!\w)/g, '$1$2');

    /* Drop a dangling "...check out this link: <name>" tail that names
     * a resource but gives NO actual URL.  The 7B model frequently
     * appends these ("...for more info, check out this link: BBS
     * Documentaries"); strip_fake_urls() can't catch them because there
     * is no URL to strip.  Remove the trailing clause -- from a lead-in
     * verb through "(this|the) link[:] <name>" to the end -- but ONLY
     * when that clause carries no http(s) URL, so real citations
     * survive (verified by the callback). */
    s = s.replace(
        /[\s,;.]+(?:(?:and|but)\s+)?(?:for\s+[^.!?\n]{0,40}?\s+)?(?:you\s+can\s+|to\s+)?(?:check(?:ing)?\s+(?:it\s+)?out|see|visit|read\s+more|find\s+(?:out\s+)?more|learn\s+more|explore|refer\s+to)\s+[^.!?\n]*?\b(?:this|the|the\s+following)\s+(?:links?|resources?|pages?|articles?|guides?|entr(?:y|ies)|write[\s-]?ups?)\b\s*:?[^.!?\n]*$/i,
        function (m) { return /https?:\/\//.test(m) ? m : '.'; });

    /* (5) Collapse 2+ consecutive newlines to a single space.  By the
     * time we get here streaming has already drawn the paragraph
     * break on the terminal -- the post-processing affects the STORED
     * string (memory + log), not the on-screen rendering for THIS
     * turn. */
    s = s.replace(/\s*\n\s*\n\s*/g, ' ');
    /* List-to-inline conversion can leave "item., item" -- collapse the
     * period-then-comma into one comma. */
    s = s.replace(/\.,/g, ',');
    /* Collapse runs of whitespace introduced by removals above. */
    s = s.replace(/[ \t]{2,}/g, ' ');
    s = s.replace(/^\s+|\s+$/g, '');

    /* The model sometimes echoes an empty TOOL result as a mechanical
     * "No results found for X" / "I couldn't find any info on X" -- which
     * reads like a database error, not the guru.  Rewrite a whole-reply
     * no-results admission into the guru's natural voice, preserving the
     * topic.  Anchored to the full (trimmed) reply so it only fires when
     * the WHOLE reply is that admission, never on a sentence that merely
     * contains the word "found". */
    s = s.replace(
        /^\s*(?:sorry[,.!]?\s*)?(?:(?:i|we)\s+(?:couldn.?t|could\s+not|didn.?t|did\s+not|was\s+unable\s+to|wasn.?t\s+able\s+to)\s+(?:find|locate|turn\s+up|dig\s+up)\s+(?:any(?:thing)?\s+)?(?:(?:relevant|matching|specific|good|exact|direct|useful|related)\s+)?(?:results?|matches?|info(?:rmation)?|records?|details?|entries|hits)?|(?:there\s+(?:are|were|is|was)\s+)?no\s+(?:(?:relevant|matching|specific|good|exact|direct|useful|related)\s+)?(?:results?|matches?|matching\s+\w+|info(?:rmation)?|records?|entries|data|hits|details?)\s*(?:found|were\s+found|are\s+available|was\s+returned|returned|to\s+show)?|(?:the\s+)?search\s+(?:returned|yielded|found|came\s+up\s+with)\s+(?:no|nothing|zero)\b[^.!?\n]*)\s*(?:\b(?:for|on|about|regarding|matching|related\s+to|named)\s+([^.!?\n]+?))?[.!]*\s*$/i,
        function (m, topic) {
            topic = (topic || '').replace(/^\s+|\s+$/g, '');
            return topic
                ? "haven't come across anything on " + topic + "."
                : "haven't come across that one.";
        });

    /* (4) Keep first sentence ending in '?', drop later '?'-sentences.
     * Walk through sentences split on '.', '!', '?' boundaries; once
     * we've kept one '?'-sentence, any subsequent sentence ending in
     * '?' is dropped (we keep statements / exclamations after it). */
    var parts = s.match(/[^.!?]+[.!?]+|[^.!?]+$/g);
    if (parts) {
        var found_q = false;
        var out = [];
        for (var i = 0; i < parts.length; i++) {
            var p = parts[i];
            var trimmed = p.replace(/^\s+|\s+$/g, '');
            if (/\?\s*$/.test(trimmed)) {
                if (found_q) continue;
                found_q = true;
            }
            out.push(p);
        }
        s = out.join('').replace(/\s+$/, '');
    }

    /* (3) Strip trailing engagement-padding question.  Operates on the
     * LAST sentence only.  Don't strip clarifying questions that
     * mention specific subject matter -- the RX matches generic
     * filler patterns ("how can I assist", "anything else"). */
    var sentences = s.match(/[^.!?]+[.!?]+|[^.!?]+$/g);
    if (sentences && sentences.length) {
        var last = sentences[sentences.length - 1];
        var last_trimmed = last.replace(/^\s+|\s+$/g, '');
        if (/\?\s*$/.test(last_trimmed)
            && ENGAGEMENT_QUESTION_RX.test(last_trimmed)) {
            sentences.pop();
            s = sentences.join('').replace(/\s+$/, '');
        }
    }

    return s;
}

/* --- Public entries --- */

/* Generate a reply to the current input. Returns a string, or null in
 * multinode/irc mode if the caller indicated the bot wasn't addressed
 * (ctx.addressed === false).
 *
 * opts (optional) forwards to chat_openai:
 *   opts.streaming             default true (uses SSE + console.simulate_type)
 *   opts.simulate_typos        forwarded
 *   opts.typing_speed_factor   forwarded
 */
/* Returns true when `user_input` is a casual/closer turn that doesn't
 * need the full chat-turn system prompt (archive rules, anti-fab,
 * etc.).  Use the smaller opening-system prompt for these to cut
 * LLM TTFC.  Casual when remaining content tokens (after stopword
 * filter) are EITHER empty OR consist entirely of the bot's name:
 *   "bye"          -> [] (all stopwords)               -> casual
 *   "yo guru"      -> ["guru"] (just bot name)         -> casual
 *   "howdy guru"   -> ["guru"]                         -> casual
 *   "synchronet"   -> ["synchronet"]  (substantive)    -> NOT casual */
function is_casual_input(user_input, ctx)
{
    var tokens = tokenize_query(user_input);
    if (tokens.length === 0) return true;
    var bot_lc = ((ctx.persona && ctx.persona.name) || '').toLowerCase();
    if (!bot_lc) return false;
    for (var i = 0; i < tokens.length; i++) {
        if (bot_lc.indexOf(tokens[i]) < 0) return false;
    }
    return true;
}

function llm_chat(user_input, ctx, opts)
{
    if (ctx.mode != 'private' && ctx.addressed === false)
        return null;
    var cfg = load_config(ctx.persona && ctx.persona.code);
    var sys_override = is_casual_input(user_input, ctx)
                     ? cfg.opening_system_prompt : null;
    var messages = build_messages(cfg, user_input, ctx, sys_override);
    /* Thread ctx through to chat_ollama so tools can read
     * ctx.speaker, ctx.channel, etc. via the env arg of
     * llm_tools_execute.  Stashed on opts so we don't have to
     * widen dispatch()'s signature. */
    opts = opts || {};
    opts._ctx = ctx;
    /* Stash the raw user input so the dispatcher's pre-classifier
     * can match against it (build_messages may have wrapped or
     * @-expanded the prompt by the time it's in messages[]). */
    opts._user_input = user_input;
    return sanitize_reply(dispatch(cfg, messages, opts));
}

/* Open a new chat session: returns an opening greeting (or null if
 * opening_prompt is blank in the config). The returned greeting should
 * be displayed and appended to the transcript as a bot turn so
 * subsequent llm_chat() calls see it.
 *
 * opts is the same shape as llm_chat()'s opts (streaming, simulate_typos,
 * typing_speed_factor). Pass streaming=true to have the greeting typed
 * out as the model generates it (caller must then skip its own typing). */
function llm_open(ctx, opts)
{
    var cfg = load_config(ctx.persona && ctx.persona.code);
    if (!cfg.opening_prompt) return null;
    var mctx = build_macro_ctx(ctx);
    var prompt = expand_macros(cfg.opening_prompt, mctx);
    /* Use the smaller opening-specific system prompt if configured
     * (~1KB persona+style vs the full ~7KB system prompt with
     * archive/anti-fab rules).  Cuts opening TTFC dramatically on
     * small models. */
    var messages = build_messages(cfg, prompt, ctx, cfg.opening_system_prompt);
    return sanitize_reply(dispatch(cfg, messages, opts));
}

/* Returns true if `line` appears to address one of the bot's known
 * names (case-insensitive whole-word match). Callers use this in
 * multinode/irc modes to decide whether to invoke llm_chat at all. */
function is_addressed(line, names)
{
    if (!line || !names || !names.length) return false;
    var lower = String(line).toLowerCase();
    for (var i = 0; i < names.length; i++) {
        if (!names[i]) continue;
        var n = String(names[i]).toLowerCase();
        /* Escape regex metachars in the name. */
        var esc = n.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        if (new RegExp('\\b' + esc + '\\b').test(lower))
            return true;
    }
    return false;
}

/* --- Helper for callers: build a 'private' ctx from a User --- */

function ctx_from_user(useron, persona_code, persona_name, supports_utf8)
{
    /* Canonicalize the persona code to lowercase: ini section lookups are
     * case-insensitive anyway, and lowercasing keeps the derived filenames
     * (data/chat/<speaker>.<code>.json, <code>.idx) case-stable so a guru
     * coded "GURU" vs "guru" can't spawn duplicate/colliding files on a
     * case-insensitive filesystem.  Blank/missing -> the reserved "default"
     * fallback persona; a guru coded "default" collapses into it rather
     * than colliding with the fallback's files. */
    persona_code = String(persona_code || 'default').toLowerCase();
    var attrs = useron ? {
        real_name: useron.name,
        level:     useron.security ? useron.security.level : 0,
        location:  useron.location,
        lang:      useron.lang
    } : {};
    /* Surface a couple of config knobs onto ctx so the C++ caller can
     * read them via JS_GetProperty without re-parsing chat_llm.ini.
     * typing_speed_factor and simulate_typos control output_with_typing
     * (C++ side). Read defensively -- load_config can throw if the ini
     * is malformed, and we don't want to block ctx creation. */
    var typing_speed_factor = 2.0;
    var simulate_typos      = true;
    try {
        var c = load_config(persona_code || 'default');
        if (c.typing_speed_factor !== undefined)
            typing_speed_factor = parseFloat(c.typing_speed_factor);
        if (c.simulate_typos !== undefined)
            simulate_typos = (c.simulate_typos != 'false');
    } catch (e) { /* fall back to defaults */ }

    return {
        persona: {
            code: persona_code || 'default',
            name: persona_name || 'The Guru'
        },
        speaker: {
            /* Zero-pad the user number to a 4-digit minimum, matching
             * Synchronet's %04u convention for user-numbered data files
             * (data/user/0001.*, data/file/0001.in, etc.) so the chat
             * memory filename (data/chat/<name>/user_0001.json) lines up. */
            id:    useron ? ('user:' + format('%04u', useron.number)) : 'user:0000',
            alias: useron ? useron.alias : 'tester',
            attrs: attrs
        },
        participants: [],
        transcript:   [],
        mode:         'private',
        supports_utf8: supports_utf8 !== false,
        addressed:    true,
        /* Surfaced for C++ dispatch (read via JS_GetProperty). */
        typing_speed_factor: typing_speed_factor,
        simulate_typos:      simulate_typos
    };
}

/* --- Persistent per-(speaker, persona) memory ---
 *
 * Each conversation between a speaker (identified by ctx.speaker.id, e.g.
 * 'user:42' for BBS or 'irc:freenode/rob' for IRC) and a persona (the
 * bot identity, ctx.persona.code) is stored as a JSON file:
 *
 *     data/chat/<safe_speaker_id>.<persona_code>.json
 *
 * Stores:
 *   - rolling-window transcript of recent verbatim turns
 *   - long-term summary (LLM-compressed when transcript overflows)
 *   - first_seen / last_seen timestamps for age-based pruning
 *
 * llm_chat/llm_open remain pure dispatch (no I/O). chat_session/
 * open_session are the high-level entries that load memory, dispatch,
 * and persist updates. Callers choose: pure for unusual flows, session
 * for the typical "memory just works" case (BBS dispatch, IRC adapter). */

var MEMORY_VERSION = 1;
var MEMORY_SUBDIR  = 'chat';

function safe_id(id)
{
    /* Make speaker.id filename-safe: ':' and '/' and other reserved chars
     * become '_'. */
    return String(id).replace(/[<>:"\/\\|?*\x00-\x1f]/g, '_');
}

/* Split a persona code into its directory NAME (before the first ':')
 * and its mode PROTO (after).  The name becomes a per-guru subdirectory
 * under data/chat/ where the RAG index is shared across that guru's
 * modes; the proto, when present, tags the per-(speaker, mode) files so
 * the terminal guru ("guru") and the IRC bot ("guru:irc") don't collide
 * on the same speaker.  Both parts are run through safe_id(). */
function persona_parts(persona_code)
{
    var c = String(persona_code || 'default');
    var i = c.indexOf(':');
    if (i < 0) return { name: safe_id(c), proto: '' };
    return { name: safe_id(c.slice(0, i)), proto: safe_id(c.slice(i + 1)) };
}

/* Directory holding a persona's files (RAG index, IRC state, memory):
 * data/chat/<name>/.  Created on demand (mkdir is fire-and-forget --
 * false when it already exists, which is fine). */
function persona_dir(persona_code)
{
    var dir = system.data_dir + MEMORY_SUBDIR + '/'
            + persona_parts(persona_code).name + '/';
    mkdir(dir);
    return dir;
}

function memory_path(ctx)
{
    /* data/chat/<name>/<speaker>[.<proto>].json -- the name lives in the
     * directory; the proto (when the code has a ':mode' part) tags the
     * mode so one speaker's memories under different modes of the same
     * guru don't overwrite each other. */
    var p = persona_parts(ctx.persona.code);
    return persona_dir(ctx.persona.code) + safe_id(ctx.speaker.id)
         + (p.proto ? '.' + p.proto : '') + '.json';
}

function empty_memory(ctx)
{
    var now = time();
    return {
        version:    MEMORY_VERSION,
        speaker_id: ctx.speaker.id,
        persona:    ctx.persona.code,
        first_seen: now,
        last_seen:  now,
        summary:    '',
        topics:     [],
        transcript: []
    };
}

function load_memory(ctx)
{
    var path = memory_path(ctx);
    if (!file_exists(path))
        return empty_memory(ctx);
    var f = new File(path);
    if (!f.open('r'))
        return empty_memory(ctx);
    var raw = f.read();
    f.close();
    var mem;
    try {
        mem = JSON.parse(raw);
    } catch (e) {
        log(LOG_WARNING, 'chat_llm: corrupt memory file ' + path + ': ' + e);
        return empty_memory(ctx);
    }
    /* Fill in any missing fields (defensive against schema drift). */
    if (!mem.version || mem.version != MEMORY_VERSION) {
        log(LOG_INFO, 'chat_llm: memory ' + path + ' version '
            + mem.version + ', expected ' + MEMORY_VERSION);
    }
    if (!mem.transcript) mem.transcript = [];
    if (!mem.summary)    mem.summary = '';
    if (!mem.topics)     mem.topics = [];
    if (!mem.first_seen) mem.first_seen = time();
    return mem;
}

function save_memory(ctx, memory)
{
    memory.last_seen = time();
    var path = memory_path(ctx);
    var tmp  = path + '.tmp';
    var f = new File(tmp);
    if (!f.open('w'))
        throw new Error('cannot write ' + tmp);
    f.write(JSON.stringify(memory, null, 2));
    f.close();
    /* Atomic-ish replace: on Windows, rename fails if dest exists, so
     * remove first. Brief window where neither exists; acceptable for
     * small low-frequency writes. */
    if (file_exists(path)) file_remove(path);
    if (!file_rename(tmp, path))
        throw new Error('cannot rename ' + tmp + ' -> ' + path);
}

function delete_memory(ctx)
{
    var path = memory_path(ctx);
    if (file_exists(path)) file_remove(path);
}

function append_turn(memory, who, text, is_bot)
{
    memory.transcript.push({
        who:  who,
        text: text,
        ts:   time(),
        bot:  !!is_bot
    });
}

function should_summarize(memory, cfg)
{
    return memory.transcript.length >= cfg.summarize_threshold;
}

/* When the transcript overflows, compress the oldest `summarize_batch`
 * turns into the rolling `summary` via a one-shot LLM call.
 *
 * opts (all optional):
 *   on_token  -- callback(text) invoked per SSE delta when streaming.
 *                Use to emit "paging dots" so the caller sees real
 *                progress feedback during the 3-7s summarize.  Defaults
 *                to non-streaming (faster latency on single-shot, but
 *                no progress visible). */
function summarize_old_turns(cfg, ctx, memory, opts)
{
    opts = opts || {};
    var batch = memory.transcript.slice(0, cfg.summarize_batch);
    if (!batch.length) return;

    var transcript_text = batch.map(function (t) {
        return (t.bot ? '[bot] ' : '[' + (t.who || 'user') + '] ') + t.text;
    }).join('\n');

    /* IMPORTANT: ask for keyword/bullet-style notes, NOT narrative prose.
     * Narrative summaries (e.g. "Digital Man inquired about X, mentioned
     * Y...") have been observed getting echoed verbatim by the chat model
     * because they read like content. Notes in bullet form are
     * structurally distinct and less likely to leak into replies. */
    var prompt = 'You produce compact keyword-style memory notes for a '
        + 'chatbot. From the chat transcript below, output ONLY a short '
        + 'list of facts/topics the bot should remember about this user '
        + 'for future chats. Use bullet points starting with "- ". '
        + 'Keep each bullet under 8 words. NO narrative sentences. NO '
        + 'phrases like "the user mentioned" or "they expressed interest" '
        + '-- just the facts/topics themselves.\n\n'
        + 'Example output:\n'
        + '- interests: Synchronet dev, BBS history\n'
        + '- is the sysop of this BBS\n'
        + '- prefers casual terse chat\n'
        + '- familiar with door games, modems\n\n'
        + 'Prior notes:\n' + (memory.summary || '(none)') + '\n\n'
        + 'Transcript to incorporate:\n' + transcript_text + '\n\n'
        + 'Updated notes (bullets only, no preamble):';

    var messages = [{ role: 'system', content: prompt }];

    if (!opts.on_token) {
        /* Fast path: non-streaming single-shot. */
        var new_summary;
        try {
            new_summary = dispatch(cfg, messages);
        } catch (e) {
            log(LOG_WARNING, 'chat_llm: summarization failed: ' + e);
            return;
        }
        memory.summary = String(new_summary).replace(/\s+/g, ' ').trim();
        memory.transcript.splice(0, cfg.summarize_batch);
        return;
    }

    /* Streaming path: emit on_token(text) per SSE delta so the caller
     * can render "paging dots" (or similar progress feedback) that
     * matches actual summarize progress.  Inlined here rather than
     * routing through chat_openai because we want a different per-
     * token action (dots, not console.simulate_type). */
    var req = {
        model:       cfg.model,
        messages:    messages,
        max_tokens:  cfg.max_tokens,
        temperature: cfg.temperature,
        stream:      true
    };
    var http = new HTTPRequest(undefined, undefined,
        { 'Authorization': 'Bearer ' + cfg.api_key }, cfg.timeout);
    var body = JSON.stringify(req);
    var full = '';
    var sse_buf = '';
    try {
        http.PostStreaming(cfg.endpoint, body, function (chunk) {
            sse_buf += chunk;
            while (true) {
                var brk = sse_buf.indexOf('\n\n');
                if (brk < 0) break;
                var event = sse_buf.substring(0, brk);
                sse_buf = sse_buf.substring(brk + 2);
                var lines = event.split('\n');
                for (var li = 0; li < lines.length; li++) {
                    var line = lines[li];
                    if (line.indexOf('data: ') !== 0) continue;
                    var payload = line.substring(6);
                    if (payload == '[DONE]') continue;
                    try {
                        var j = JSON.parse(payload);
                        var d = j.choices && j.choices[0]
                             && j.choices[0].delta && j.choices[0].delta.content;
                        if (d) {
                            full += d;
                            try { opts.on_token(d); } catch (e) {}
                        }
                    } catch (e) {}
                }
            }
        }, 'application/json');
    } catch (e) {
        log(LOG_WARNING, 'chat_llm: summarization stream failed: ' + e);
        return;
    }
    if (http.response_code != 200) {
        log(LOG_WARNING, 'chat_llm: summarize HTTP '
            + http.response_code);
        return;
    }

    memory.summary = full.replace(/\s+/g, ' ').trim();
    memory.transcript.splice(0, cfg.summarize_batch);
}

function is_forget_command(input)
{
    if (!input) return false;
    return /^\s*\/?forget(\s+(me|everything|all))?\s*[.!?]?\s*$/i.test(input);
}

/* --- High-level session entries (load memory + dispatch + persist) ---
 *
 * These are what BBS C++ dispatch and IRC adapters should call. They
 * handle the load/save dance so callers don't have to. Memory I/O is
 * skipped when cfg.memory_persist == false. */

function chat_session(input, ctx)
{
    var t_start = system.timer;
    var cfg = load_config(ctx.persona && ctx.persona.code);

    /* /forget me -- wipe persistent memory and reply canned. */
    if (cfg.memory_persist && is_forget_command(input)) {
        delete_memory(ctx);
        return "Okay, " + ((ctx.speaker && ctx.speaker.alias) || 'friend')
             + ", I've forgotten our previous conversations.";
    }

    var t_mem0 = system.timer;
    var memory = cfg.memory_persist ? load_memory(ctx) : empty_memory(ctx);
    var t_mem = system.timer - t_mem0;

    /* Summarization runs in open_session under the cover of the
     * paging-dots animation (deferred there via pending_summarize).
     * If it ever fires here, it would visibly hang the terminal
     * between the bot's last spoken word and the user's next prompt
     * -- avoid that. */
    var t_sum = 0;

    /* memory_summary is wrapped in <long_term_memory>...</long_term_memory>
     * markers (see build_macro_ctx) -- the system prompt knows not to
     * quote those tags back. Enabled by default; sysop can disable on
     * small models that still leak (set memory_summary_in_prompt = false).
     * An earlier "PRIVATE NOTES" prose wrapper leaked verbatim into
     * replies on 7B and even 14B models; the XML-ish marker mirrors the
     * <board_archive> pattern that proved leakage-resistant. */
    if (cfg.memory_summary_in_prompt == 'false')
        memory.summary = '';

    /* For SHORT casual-chat inputs (<= 3 content tokens like "yo",
     * "bye", "lol"), use a trimmed transcript window (last 4 turns
     * instead of 20). Reduces prompt size dramatically -- the 14B
     * model's prompt-processing time scales with input length, so
     * smaller prompts = noticeably faster response on short turns. */
    var window = cfg.history_window;
    var input_tokens = tokenize(input);
    if (input_tokens.length < 4)
        window = Math.min(window, 4);

    ctx.summary    = memory.summary;
    ctx.transcript = memory.transcript.slice(-window);

    /* BM25 retrieval (step 3): query the per-persona index with the
     * user's input, set ctx.retrieved_context for @retrieved_context@. */
    var t_rag0 = system.timer;
    inject_retrieval(input, ctx, cfg);
    var t_rag = system.timer - t_rag0;
    var rag_hits = ctx._rag_hits || 0;   /* set by inject_retrieval */

    /* Measure prompt size before LLM call so we have it in profile.
     * Mirror llm_chat's sys_override logic (see is_casual_input)
     * so the byte count reflects what the LLM actually sees. */
    var dbg_sys_override = is_casual_input(input, ctx)
                         ? cfg.opening_system_prompt : null;
    var dbg_msgs = build_messages(cfg, input, ctx, dbg_sys_override);
    var prompt_bytes = JSON.stringify(dbg_msgs).length;

    /* Streaming opts. When stream=true, chat_openai uses SSE and calls
     * console.simulate_type() per token -- the C++ caller knows to skip
     * its own simulate_type because we set ctx._streamed below. */
    var stream_supported = (typeof console != 'undefined'
                            && typeof console.simulate_type == 'function');
    /* Only stream in PRIVATE mode (1:1 paging) where the user
     * watches the bot "type" character-by-character.  In multinode
     * and IRC modes the bot's reply is emitted as a single line --
     * stream-to-terminal would break the channel chat format. */
    var streaming = stream_supported && (cfg.stream !== 'false')
                 && (ctx.mode == 'private' || ctx.mode === undefined);
    var opts = {
        streaming:           streaming,
        simulate_typos:      ctx.simulate_typos !== false,
        typing_speed_factor: ctx.typing_speed_factor || 1.0
    };

    var t_llm0 = system.timer;
    var reply = llm_chat(input, ctx, opts);
    var t_llm = system.timer - t_llm0;
    /* Post-process the assembled reply.  Strips extra questions and
     * collapses paragraph breaks.  Affects the STORED string (memory,
     * log, returned to C++ caller).  Streaming output to the terminal
     * already happened during llm_chat, so this only changes what
     * future turns and the log see -- not THIS turn's on-screen
     * rendering. */
    if (reply !== null) {
        reply = final_reply_postprocess(reply);
        /* strip BEFORE append: fabricated wiki/archive URLs in the
         * body shouldn't suppress a legitimate appended top-wiki URL
         * via append_wiki_url_if_missing's "already cited" guard. */
        reply = strip_fake_urls(reply, ctx);
        reply = append_wiki_url_if_missing(reply, ctx);
    }

    /* Flag for C++ dispatch to skip its post-call simulate_type. */
    ctx._streamed = streaming && reply !== null;

    /* Only persist when the bot actually responded (addressed=true). */
    if (reply !== null && cfg.memory_persist) {
        var who = (ctx.speaker && ctx.speaker.alias) || 'user';
        var bot_who = (ctx.persona && ctx.persona.name) || 'bot';
        append_turn(memory, who, input, false);
        append_turn(memory, bot_who, reply, true);
        /* Summarization is DEFERRED to the start of the NEXT chat
         * turn -- ran at the top of this function if pending_summarize
         * was set.  Running it synchronously here adds 3-7s after the
         * bot's last spoken word, which visibly hangs the terminal. */
        if (should_summarize(memory, cfg))
            memory.pending_summarize = true;
        save_memory(ctx, memory);
    }

    /* Stash a profile string on ctx so the C++ caller can append it to
     * guru.log. Fields:
     *   turn   = total chat_session() time
     *   llm    = main chat HTTP roundtrip
     *   sum    = summarization HTTP roundtrip (when it fires, else 0)
     *   mem    = load_memory I/O
     *   rag    = BM25 search + injection
     *   prompt = bytes sent to LLM on main call
     *   hits   = retrieved chunks injected (0 to index_top_k)
     *   window = transcript turns included (4 for short inputs) */
    var t_total = system.timer - t_start;
    ctx._profile = 'persona=' + (ctx.persona && ctx.persona.code || '?')
                 + ' model=' + (cfg.model || '?')
                 + ' turn=' + t_total.toFixed(2) + 's'
                 + ' llm=' + t_llm.toFixed(2) + 's'
                 + (t_sum > 0 ? ' sum=' + t_sum.toFixed(2) + 's' : '')
                 + ' mem=' + t_mem.toFixed(3) + 's'
                 + ' rag=' + t_rag.toFixed(3) + 's'
                 + ' prompt=' + prompt_bytes + 'B'
                 + ' hits=' + rag_hits
                 + (ctx._rag_per_token !== undefined
                    ? ' rag_q=' + ctx._rag_per_token.toFixed(2)
                    : '')
                 + ' window=' + window
                 + ' stream=' + (opts._stream_used ? 'yes' : 'no')
                 + (opts._stream_used && opts._ttfc !== null
                    ? ' ttfc=' + opts._ttfc.toFixed(2) + 's'
                    : '')
                 + (opts._stream_used
                    ? ' chunks=' + opts._chunks_count
                      + ' deltas=' + opts._deltas_count
                      + ' console=' + (opts._have_console ? 'yes' : 'no')
                    : '')
                 + (opts._tool_calls && opts._tool_calls.length
                    ? ' tools=[' + opts._tool_calls.join(',') + ']'
                    : '');

    return reply;
}

function open_session(ctx)
{
    var t_start = system.timer;
    var cfg = load_config(ctx.persona && ctx.persona.code);

    var t_mem0 = system.timer;
    var memory = cfg.memory_persist ? load_memory(ctx) : empty_memory(ctx);
    var t_mem = system.timer - t_mem0;

    /* memory_summary_in_prompt enabled by default (XML-ish marker form);
     * sysop opts OUT with = false if a smaller model still leaks. */
    if (cfg.memory_summary_in_prompt == 'false')
        memory.summary = '';

    ctx.summary    = memory.summary;
    ctx.transcript = memory.transcript.slice(-cfg.history_window);

    /* Run deferred summarization (if pending from a prior chat
     * session that crossed summarize_threshold).  Hidden behind the
     * C++ side's pre-greeting dot animation -- runs silently here.
     * The legacy 25-50 dots × 200ms gives ~5-10s of cover, which
     * matches typical summarize duration on a 7B model. */
    var t_sum = 0;
    if (memory.pending_summarize && cfg.memory_persist) {
        var t_sum0 = system.timer;
        summarize_old_turns(cfg, ctx, memory);
        t_sum = system.timer - t_sum0;
        memory.pending_summarize = false;
    }

    /* Stream the opening the same way chat_session streams regular turns,
     * so the caller gets fast time-to-first-char and the bot appears to
     * "type" the greeting. ctx._streamed flags the C++ dispatch to skip
     * its post-call simulate_type (otherwise the greeting prints twice). */
    var stream_supported = (typeof console != 'undefined'
                            && typeof console.simulate_type == 'function');
    /* Only stream in PRIVATE mode (1:1 paging) where the user
     * watches the bot "type" character-by-character.  In multinode
     * and IRC modes the bot's reply is emitted as a single line --
     * stream-to-terminal would break the channel chat format. */
    var streaming = stream_supported && (cfg.stream !== 'false')
                 && (ctx.mode == 'private' || ctx.mode === undefined);
    var opts = {
        streaming:           streaming,
        simulate_typos:      ctx.simulate_typos !== false,
        typing_speed_factor: ctx.typing_speed_factor || 1.0
    };

    /* Measure prompt size for the profile breadcrumb -- use the
     * SAME sys_override (cfg.opening_system_prompt) that llm_open
     * will actually use, so the logged byte-count reflects what
     * the LLM sees, not the full chat-turn system prompt. */
    var dbg_msgs = build_messages(cfg, expand_macros(cfg.opening_prompt || '',
                                                     build_macro_ctx(ctx)),
                                  ctx, cfg.opening_system_prompt);
    var prompt_bytes = JSON.stringify(dbg_msgs).length;

    var t_llm0 = system.timer;
    var greeting = llm_open(ctx, opts);
    var t_llm = system.timer - t_llm0;
    if (greeting !== null) greeting = final_reply_postprocess(greeting);
    ctx._streamed = streaming && greeting !== null;

    if (greeting && cfg.memory_persist) {
        var bot_who = (ctx.persona && ctx.persona.name) || 'bot';
        append_turn(memory, bot_who, greeting, true);
        save_memory(ctx, memory);
    }

    /* Same shape of profile breadcrumb as chat_session so guru.log
     * shows opening latency next to chat-turn latency.  Lets the
     * sysop see if openings are slow (model cold-load, big system
     * prompt) vs chat-turns (RAG injection, summarization). */
    var t_total = system.timer - t_start;
    ctx._profile = 'persona=' + (ctx.persona && ctx.persona.code || '?')
                 + ' model=' + (cfg.model || '?')
                 + ' open turn=' + t_total.toFixed(2) + 's'
                 + ' llm=' + t_llm.toFixed(2) + 's'
                 + (t_sum > 0 ? ' deferred_sum=' + t_sum.toFixed(2) + 's' : '')
                 + ' mem=' + t_mem.toFixed(3) + 's'
                 + ' prompt=' + prompt_bytes + 'B'
                 + ' stream=' + (opts._stream_used ? 'yes' : 'no')
                 + (opts._stream_used && opts._ttfc !== null
                    ? ' ttfc=' + opts._ttfc.toFixed(2) + 's'
                    : '');

    return greeting;
}

/* --- BM25 retrieval (step 3) ---
 *
 * The indexer (exec/chat_index.js) writes a JSON index per persona;
 * we load it once per session and query it per turn. Top-K hits with
 * text + provenance get formatted and injected into the prompt via the
 * @retrieved_context@ macro.
 *
 * KEEP tokenize() identical to exec/chat_index.js's tokenize(). If they
 * diverge, retrieval breaks (query tokens won't match indexed tokens). */

var INDEX_CACHE = null;   /* module-level cache for one session */

function tokenize(text)
{
    if (!text) return [];
    var matches = String(text).toLowerCase().match(/[a-z0-9]+/g);
    return matches || [];
}

/* English stopwords + conversational filler. Stripped from QUERIES at
 * search time (NOT at index time -- the index stays general so we don't
 * need to rebuild it). Without this, queries like "so what do you know
 * about Synchronet?" let BM25 score on "what/do/you/know/about" -- which
 * pulls in completely irrelevant short posts that happen to contain
 * those words ("sup\nDO you know what's entertaining..."). Filtering
 * these leaves just the content words and dramatically improves recall.
 *
 * Side-effect: any of these words MUST be excluded at search time only.
 * The indexer's tokenize() must remain unchanged. */
var QUERY_STOPWORDS = (function () {
    var words = ('a an and any are as at be by do does for from has have he '
        + 'her here him his how i if in is it its just like me my no not of '
        + 'on or our she so some that the their them then there these they '
        + 'this those to up us was we were what when where which who whom '
        + 'why will with you your yours yourself yourselves know got more '
        + 'about all also can could would should being am been but did had '
        + "isn't aren't wasn't weren't don't doesn't didn't haven't hasn't "
        + "won't can't couldn't shouldn't wouldn't i'm i'll i've you're "
        + "you'll you've it's that's there's whats whos hows hes shes "
        + 'really very much too lot lots kinda gonna wanna yeah yep nope '
        + 'okay ok ya sure cool nice maybe '
        /* Common BBS chat closers / fillers -- excluded so single-word
         * exits like "bye" don't trigger retrieval (a "bye" alone was
         * scoring rag_q=12.70 and injecting 8KB of irrelevant content
         * into the exit-turn prompt). */
        + 'bye later cya seeya gtg ttyl thanks thx ty np lol haha hehe '
        + 'hi hey yo sup hiya hello howdy greetings').split(/\s+/);
    var set = Object.create(null);
    for (var i = 0; i < words.length; i++) set[words[i]] = true;
    return set;
}());

function tokenize_query(text)
{
    var raw = tokenize(text);
    var out = [];
    for (var i = 0; i < raw.length; i++) {
        if (raw[i].length < 2) continue;          /* drop "i", "a", etc. */
        if (QUERY_STOPWORDS[raw[i]]) continue;
        out.push(raw[i]);
    }
    return out;
}

function load_index(persona_code, cfg)
{
    /* Strip the mode-suffix from "<base>:<mode>" persona codes (e.g.
     * "guru:irc" -> "guru") when looking up the index file.  Personas
     * for different modes share the same index by default -- the
     * sysop would only need a separate per-mode index in unusual
     * setups, in which case they can rebuild with the full
     * "guru:irc" persona explicitly. */
    var key = safe_id((persona_code || 'default').split(':')[0]);
    var path = system.data_dir
             + String(cfg.index_output).replace(/<persona>/g, key);

    /* Cache check.  In addition to the persona match, compare the
     * cached index file's mtime against the on-disk file -- when the
     * nightly index rebuild (or a manual rebuild during debugging)
     * replaces the .idx file, this lets the running bot pick up the
     * new chunks WITHOUT a restart.  Without this, INDEX_CACHE held
     * the first-loaded index forever and stale data leaked into
     * answers until the next process restart. */
    if (file_exists(path)) {
        var fmtime = (function () {
            var fst = new File(path);
            var t = fst.date || 0;
            return t;
        }());
        if (INDEX_CACHE && INDEX_CACHE._persona == key
            && INDEX_CACHE._mtime == fmtime)
            return INDEX_CACHE;
    } else {
        if (INDEX_CACHE && INDEX_CACHE._persona == key + ':missing')
            return null;  /* already checked, no file */
        INDEX_CACHE = { _persona: key + ':missing' };
        return null;
    }

    var f = new File(path);
    if (!f.open('r')) return null;
    var raw = f.read();
    var loaded_mtime = f.date || 0;
    f.close();

    var idx;
    try {
        idx = JSON.parse(raw);
    } catch (e) {
        log(LOG_WARNING, 'chat_llm: corrupt index ' + path + ': ' + e);
        return null;
    }
    idx._persona = key;
    idx._mtime   = loaded_mtime;
    INDEX_CACHE = idx;
    return idx;
}

function bm25_search(idx, query_tokens, top_k, source_weights, recency_halflife_days, group_aliases, raw_query)
{
    if (!idx || !idx.N) return [];
    var k1    = idx.k1    || 1.5;
    var b     = idx.b     || 0.75;
    var avgdl = idx.avgdl || 1;
    var N     = idx.N;

    /* Title-match boost multiplier.  When a query token matches a
     * token in the doc's title (msgbase subject, wiki heading,
     * filename, issue title), that term's per-doc BM25 contribution
     * is scaled by this factor.  Surfaces short authoritative pages
     * (howto:report, util:smbutil, access:sysop) that contain the
     * answer in their title but lose to long msgbase posts on raw
     * keyword density.  2.0 is the standard "field-boost" value
     * used in classic IR (Lucene's `title^2` syntax). */
    var TITLE_BOOST = 2.0;

    var scores = {};  /* doc_idx -> cumulative BM25 score */
    for (var i = 0; i < query_tokens.length; i++) {
        var t = query_tokens[i];
        var term = idx.terms[t];
        /* Defensive: tokens like "constructor"/"toString" can return
         * Object.prototype methods if idx.terms isn't prototype-less.
         * Require both an object and the expected `postings` field. */
        if (!term || typeof term != 'object' || !term.postings) continue;
        var idf = Math.log((N - term.df + 0.5) / (term.df + 0.5) + 1);
        for (var p = 0; p < term.postings.length; p++) {
            var doc_idx = term.postings[p][0];
            var tf      = term.postings[p][1];
            var dl      = idx.docs[doc_idx].len;
            var score   = idf * (tf * (k1 + 1))
                        / (tf + k1 * (1 - b + b * dl / avgdl));
            var tt      = idx.docs[doc_idx].tt;
            if (tt && tt[t]) score *= TITLE_BOOST;
            scores[doc_idx] = (scores[doc_idx] || 0) + score;
        }
    }

    /* Apply per-source weight multipliers.  Source = first segment
     * of the chunk's id ("dokuwiki", "msgbase", "filebase", "gitlab").
     * Multiplier defaults to 1.0 when source isn't listed in
     * source_weights.  Lets sysops boost curated sources (wiki) over
     * noisier ones (msgbase) so the right chunks land in top-K. */
    if (source_weights) {
        for (var d in scores) {
            var id = idx.docs[d].id || '';
            var slash = id.indexOf('/');
            var src = slash > 0 ? id.slice(0, slash).toLowerCase() : id.toLowerCase();
            var w = source_weights[src];
            if (w !== undefined && w !== 1) scores[d] *= w;
        }
    }

    /* Source-group boost: when the query mentions a network/group
     * name (FidoNet, DOVE-Net, fsxNet, etc.), give chunks actually
     * FROM that group a strong multiplier.  Without this, "what's
     * being debated on FidoNet?" pulls DOVE-Net chunks that
     * *discuss* FidoNet because those have higher "fidonet" keyword
     * density than actual FidoNet posts (which simply ARE FidoNet
     * and don't repeat the network name in body text).  The boost
     * runs BEFORE recency decay so a recent-but-irrelevant DOVE-Net
     * post can't beat an older actual-FidoNet post.
     *
     * Detection: scan provenance for "Sub:<Group>/" matching one of
     * the query's group-name tokens via the per-deployment alias map
     * (cfg.group_aliases parsed by parse_group_aliases).  Lowercase
     * comparison; provenance is lowercased before the regex, alias
     * values are pre-lowercased by the parser.  When the alias map
     * is empty (no group_aliases configured) no boost is applied --
     * keeps the function generic across deployments. */
    var wanted_groups = {};
    if (group_aliases) {
        for (var qi2 = 0; qi2 < query_tokens.length; qi2++) {
            var aliases = group_aliases[query_tokens[qi2]];
            if (aliases) for (var ai = 0; ai < aliases.length; ai++)
                wanted_groups[aliases[ai]] = 1;
        }
    }
    var has_group_filter = false;
    for (var w in wanted_groups) { has_group_filter = true; break; }
    if (has_group_filter) {
        /* 1.5x is a gentle preference -- enough to flip the order
         * between two roughly-tied chunks (one from the named group,
         * one from elsewhere), not enough to swamp a wiki how-to
         * page when the question is "how do I join FidoNet" (where
         * the wiki is the authority, even though FidoNet is named).
         * Previously 3.0x was burying wiki howtos under any in-group
         * msgbase chunk that mentioned a couple of query tokens.
         *
         * Wiki pages with the group name in their PAGE ID get the
         * same boost as msgbase chunks in the group -- "dove" in
         * the query should benefit "dokuwiki/howto:dove-net" the
         * same as "msgbase/dove-deb/...".  Without this, a long
         * wiki howto loses to shorter incidentally-matching wiki
         * pages on BM25 length-normalization alone. */
        var GROUP_BOOST = 1.5;
        for (var d in scores) {
            var prov = (idx.docs[d].prov || '').toLowerCase();
            var id   = (idx.docs[d].id   || '').toLowerCase();
            var matched = false;
            var m = prov.match(/^sub:([^\/\s"]+)/);
            if (m && wanted_groups[m[1]]) matched = true;
            if (!matched && id.indexOf('dokuwiki/') === 0) {
                for (var wg in wanted_groups) {
                    if (id.indexOf(wg) >= 0) { matched = true; break; }
                }
            }
            if (matched) scores[d] *= GROUP_BOOST;
        }
    }

    /* Wiki precedence boost: the wiki is curated documentation --
     * the authoritative source-of-truth for factual / reference
     * questions.  Msgbase posts carry users' opinions, anecdotes,
     * and works-in-progress, valuable but NOT canonical.  Give all
     * wiki chunks a flat multiplier so a topically-matching wiki
     * page wins ties (or near-ties) against verbose msgbase chunks.
     *
     * For how-to / procedural queries ("how do I X", "how to Y"),
     * crank the multiplier higher: wiki howto pages tend to be long
     * (1000+ tokens) which BM25 length-normalization penalizes, and
     * they compete against in-group msgbase ad-style chunks that
     * also pick up the group-boost.  2x flat wasn't enough to
     * surface howto:dove-net (rank 23 with 16.17 vs dove-ads chunks
     * at 33+); a how-to-conditional 4x flips it.
     *
     * Detection works on the RAW query string -- "how" / "do" /
     * "would" are stopwords, stripped before tokenization, so we
     * have to scan the unfiltered phrasing. */
    var raw_lower = (raw_query || '').toLowerCase();
    var howto_intent = /\bhow\s+(to|do|does|would|can|should|might|i)\b/.test(raw_lower)
                    || /\bhow\s+would\s+\w+\s+(join|setup|configure|install)\b/.test(raw_lower);
    var WIKI_BOOST = howto_intent ? 4.0 : 2.0;
    /* Definitional intent ("what is X", "what does X stand for / mean",
     * "define X", "what's a/an X"): the authoritative source is the
     * acronyms list / glossary (ref:) / fact list.  But acronym/ and
     * syncfact/ docs don't get WIKI_BOOST (only dokuwiki/ does), so a
     * bare acronym entry otherwise loses to a wiki-boosted config page
     * (observed: "what does FOSSIL stand for" -> acronym/FOSSIL only #3).
     * Boost the definitional sources so they win for these queries. */
    var def_intent = _is_definitional_query(raw_query);
    /* Download / file-availability intent ("is X available for
     * download", "where do I download X", "do you have X.zip"): the
     * answer is a filebase entry, but the word "download" pulls in
     * wiki file-config pages (config:file_options, user:files) that get
     * WIKI_BOOST and outrank the actual files.  Boost filebase/ so the
     * real files win. */
    /* Exclude how-to phrasing ("how do I download X") so wiki how-tos
     * aren't demoted -- but DO keep "where can I download X" / "is X
     * available" as file-lookups (those start with "where", which the
     * broader docstyle check would have wrongly excluded). */
    var dl_intent  = _is_download_query(raw_query) && !howto_intent;
    var DEF_BOOST  = 3.5;
    var FILE_BOOST = 4.0;
    for (var d in scores) {
        var id = idx.docs[d].id || '';
        /* On a file-lookup query, skip the wiki boost so wiki
         * file-config pages (config:file_options, user:files) -- which
         * match "download" with high frequency -- don't get an extra
         * leg up over the actual filebase entries.  (Penalizing wiki
         * harder just let syncfacts surface instead, so leave it at no
         * boost.)  The filebase boost below lifts the real files when
         * the query term matches a filename/description; a term that
         * doesn't match any file (e.g. "hyperterminal" vs htpe63.zip)
         * can't be surfaced by boosting and is a data limitation. */
        if (id.indexOf('dokuwiki/') === 0 && !dl_intent) scores[d] *= WIKI_BOOST;
        if (def_intent && (id.indexOf('acronym/') === 0
                        || id.indexOf('syncfact/') === 0
                        || id.indexOf('dokuwiki/ref:') === 0))
            scores[d] *= DEF_BOOST;
        if (dl_intent && id.indexOf('filebase/') === 0) scores[d] *= FILE_BOOST;
    }

    /* Recency decay: multiply each doc's BM25 score by
     *   2 ^ (-age_days / halflife_days)
     * so a doc at exactly the half-life age scores at 0.5x, double
     * that age at 0.25x, etc.  Surfaces fresh content for broad
     * "latest X" queries without dropping historical docs from the
     * index -- they still surface when their keyword specificity
     * dominates (e.g. an 8-year-old Y2K-fix post is the ONLY chunk
     * matching the query, decay doesn't matter).  Docs with ts=0
     * (no authorship timestamp from the crawler) are treated as
     * ageless: no decay applied, original score kept.  Halflife
     * <= 0 disables the decay entirely. */
    if (recency_halflife_days && recency_halflife_days > 0) {
        var now_secs = time();
        var halflife_secs = recency_halflife_days * 86400;
        for (var d in scores) {
            var ts = idx.docs[d].ts;
            if (!ts) continue;                 /* ageless -- no decay */
            var age_secs = now_secs - ts;
            if (age_secs <= 0) continue;       /* future-dated -- skip */
            var decay = Math.pow(2, -age_secs / halflife_secs);
            scores[d] *= decay;
        }
    }

    var sorted = [];
    for (var d in scores)
        sorted.push({ idx: parseInt(d, 10), score: scores[d] });
    sorted.sort(function (a, b) { return b.score - a.score; });

    /* Intent-routed sort: when the query explicitly asks for the
     * newest / most recent items ("most recently reported issues",
     * "latest files uploaded", "what's been posted lately"), BM25's
     * keyword-density ranking + a 2-year half-life isn't aggressive
     * enough -- a January post with 3 mentions of "Synchronet" still
     * beats a May post with 1 mention.  When the QUERY itself signals
     * recency intent, widen the BM25 candidate pool to top_k*4, then
     * resort that pool strictly by ts descending and take the top.
     * This trusts BM25 to filter "topically relevant" but trusts the
     * timestamp to order by recency, which is what the caller asked
     * for. */
    /* Recency-intent tokens.  Match should be unambiguous -- "new"
     * is intentionally OMITTED because it more often means "novice"
     * than "most recent" ("a new sysop"/"a new user"/"new install").
     * Including it caused queries like "how would a new sysop join
     * FidoNet" to flip into recency mode and demote the authoritative
     * howto:fidonet wiki page (1+ year old) below recent FidoNet
     * chatter.  Stick to phrasings that genuinely mean fresh-by-date. */
    var RECENCY_TOKENS = { recent: 1, recently: 1, latest: 1, newest: 1,
                           lately: 1, today: 1, yesterday: 1 };
    var recency_intent = false;
    for (var qi = 0; qi < query_tokens.length; qi++) {
        if (RECENCY_TOKENS[query_tokens[qi]]) { recency_intent = true; break; }
    }
    if (recency_intent) {
        var pool = sorted.slice(0, Math.min(sorted.length, (top_k || 5) * 8));
        pool.sort(function (a, b) {
            var ta = idx.docs[a.idx].ts || 0;
            var tb = idx.docs[b.idx].ts || 0;
            return tb - ta;
        });
        /* Dedup by title.  Heated threads on a single subject (e.g.
         * a debate sub with 12 replies to "people with autism") would
         * otherwise fill the inject window with N copies of the same
         * topic.  When the caller asked "what's being debated lately"
         * they want N distinct topics, not N voices in one topic.
         * Title (msgbase subject post-Re:-strip, wiki heading, etc.)
         * is the right key: replies share it, distinct threads don't.
         * Keep the FIRST (newest, since pool is now newest-first) hit
         * for each title; drop subsequent same-title hits. */
        var seen_titles = Object.create(null);
        var deduped = [];
        for (var pi = 0; pi < pool.length; pi++) {
            var doc = idx.docs[pool[pi].idx];
            var key = '';
            if (doc.tt) {
                /* Reconstruct a normalized title key from the stored
                 * title-token set.  Sorted-token-join makes ordering
                 * deterministic regardless of original token order. */
                var keys = [];
                for (var k in doc.tt) keys.push(k);
                keys.sort();
                key = keys.join(' ');
            }
            if (key && seen_titles[key]) continue;
            if (key) seen_titles[key] = 1;
            deduped.push(pool[pi]);
        }
        sorted = deduped;
    }

    var limit = Math.min(top_k || 5, sorted.length);
    var hits = [];
    for (var i = 0; i < limit; i++) {
        var doc = idx.docs[sorted[i].idx];
        hits.push({
            score:      sorted[i].score,
            id:         doc.id,
            text:       doc.text,
            provenance: doc.prov,
            ts:         doc.ts || 0
        });
    }
    return hits;
}

function format_retrieved_context(hits)
{
    if (!hits || !hits.length) return '';
    var blocks = hits.map(function (h) {
        var prov = h.provenance || h.id || 'unknown';
        var text = h.text || '';
        /* Wiki hits inject ONLY the URL + title -- NO page content.
         * Small models (qwen2.5:7b) given wiki content either
         * reproduce it verbatim or hallucinate the URL because they
         * can't reliably copy long strings. Stripping content
         * eliminates both failure modes: the model has nothing to
         * reproduce, and the only URL it can possibly cite is the
         * one literally in the tag. */
        /* Truncate ALL hits (wiki / msgbase / gitissue) to short
         * snippets.  Full chunks are 2000+ chars; the model only
         * needs ~500-800 chars to extract an answer from.  Big
         * retrieved chunks blow up prompt size (10K+ bytes), which
         * drives TTFC on small models from 2s to 8-15s.
         *
         * Wiki hits used to cap tighter (250 chars) on the theory
         * that small models would reproduce step-by-step how-tos
         * verbatim.  In practice the 250-char cap clipped pages mid-
         * sentence: `user:list` got truncated before listing the
         * keyboard shortcuts (Ctrl-U / /L / U) and the model could
         * only say "press L" instead of enumerating the options.
         * Bumped to 800 once we moved IRC to qwen3:14b -- a 14B model
         * doesn't have the reproduce-or-hallucinate failure mode that
         * the tight cap was protecting against, and the extra ~550
         * chars per wiki chunk is well worth the ~+2s TTFC. */
        var max_chars = prov.indexOf('Wiki:') === 0 ? 800 : 500;
        if (text.length > max_chars)
            text = text.slice(0, max_chars) + '...';

        /* For wiki hits, extract the URL from the provenance tag
         * (which looks like "Wiki:https://wiki.synchro.net/history:versions ...")
         * and PREPEND it to the chunk body as a "Cite this URL:"
         * line.  This makes the URL much more visible to the model
         * during reply generation -- previously the URL was only in
         * the provenance tag, and the model was reconstructing it
         * from training-data patterns (dropping the namespace
         * prefix like "history:" because most wiki URLs in
         * training data don't have colons). */
        if (prov.indexOf('Wiki:') === 0) {
            var m = prov.match(/^Wiki:(https?:\/\/\S+)/);
            if (m) text = 'Cite this URL verbatim: ' + m[1] + '\n' + text;
        }

        return '[' + prov + ']\n' + text;
    });
    /* Minimal markers around the retrieved chunks. The bot's behavior
     * around these chunks (treat as own memory, don't deflect, cite the
     * tag when natural, etc.) is defined ONCE in the system prompt and
     * NOT repeated inline here. Why: prose instructions inside the
     * injected block read like content to small/mid models and get
     * quoted back verbatim in replies ("STUFF YOU'VE READ [Sub:Files]
     * - I haven't seen anything new..."). Bare XML-ish markers around
     * the chunks are much less likely to be echoed.
     *
     * If the system prompt mentions these markers (e.g.
     * "<board_archive> below"), use the exact same form there too. */
    return '<board_archive>\n'
         + blocks.join('\n\n')
         + '\n</board_archive>';
}

/* Populate ctx.retrieved_context (string for @retrieved_context@ macro)
 * by querying the BM25 index. No-op if no index exists or no hits.
 *
 * Pipeline:
 *  1. tokenize_query() strips stopwords ("what do you know about" -> []),
 *     so single-content-word queries reduce to that one word and we
 *     don't score on stopword noise.
 *  2. If 0 content tokens left ("yo", "lol", "what's up"), skip
 *     retrieval entirely -- casual chat doesn't need grounding and
 *     stuffing ~1.5KB of chunks into the prompt blows up TTFT.
 *  3. Compute top_score / content_token_count and gate on
 *     cfg.index_min_score_per_token (default 3.5). Below threshold, the
 *     retrieval is noise -- inject NOTHING (the system prompt's "if not
 *     in your board memory, say you haven't seen anything about it"
 *     rule takes over). Tune cfg.index_min_score_per_token after
 *     observing real per-token scores on YOUR corpus; the default
 *     was tuned against a Synchronet-flagship install where
 *     on-topic queries land around 4.5-5.0 per token and off-topic
 *     queries ("Beatles?") fall well below threshold. */
/* Doc-style query detection.  Fires for "how do I X", "how to X",
 * "is there a wiki page for X", "where in the docs / wiki is X",
 * "what's the config option for X", "what does <setting> do",
 * "step by step instructions", "give me a tutorial", etc. --
 * all queries where the right answer lives in the Synchronet wiki
 * rather than a file description or msgbase chatter. */
/* Definitional / "what does this term mean" queries.  bm25_search uses
 * this to boost the acronyms list / glossary (ref:) / fact list so a
 * short authoritative definition outranks a wiki config page that merely
 * mentions the term.  Examples: "what is FTN", "what does DSZ stand for",
 * "what's an ARS", "define FOSSIL", "meaning of SMB". */
function _is_definitional_query(input) {
    var s = String(input || '');
    if (/\bwhat\s+(?:is|are|'?s)\b/i.test(s)) return true;
    if (/\bwhat\s+(?:does|do|did)\b[^?]*\b(?:stands?\s+for|mean|means|abbreviat)/i.test(s))
        return true;
    if (/\bwhat'?s\s+(?:a|an)\b/i.test(s)) return true;
    if (/\b(?:define|definition\s+of|meaning\s+of|acronym\s+for|abbreviation\s+for|stands?\s+for)\b/i.test(s))
        return true;
    return false;
}

/* Download / file-availability queries.  bm25_search uses this to boost
 * filebase/ entries so the actual file outranks a wiki file-config page
 * that the word "download" otherwise pulls to the top.  Examples: "is
 * hyperterminal available for download", "where do I download X", "do
 * you have htpe63.zip", "can I grab X". */
function _is_download_query(input) {
    var s = String(input || '').toLowerCase();
    if (/\b(?:download|d\/l)\b/.test(s)) return true;
    if (/\bavailable\b/.test(s)
        && /\b(?:download|file|\.(?:zip|exe|arj|lzh|gz|tgz|rar|7z|lha))\b/.test(s))
        return true;
    if (/\bwhere\s+(?:do|can|could|would|to)\b[^?]*\b(?:download|get|grab|find)\b/.test(s))
        return true;
    if (/\b(?:do\s+you|does\s+(?:this|the)\s+bbs|does\s+vert\w*)\s+(?:have|carry|host|offer|stock)\b/.test(s))
        return true;
    if (/\bcan\s+i\s+(?:download|get|grab)\b/.test(s)) return true;
    return false;
}

function _is_docstyle_query(input) {
    var s = String(input || '');
    if (/\b(?:wiki|docs?|documentation|tutorial|guide|walkthrough|manual|howto|how-to)\b/i.test(s))
        return true;
    /* Imperative requests for instructions ("give me ...
     * instructions", "show me how to", "walk me through") -- treat
     * as docstyle even if "how to" is mid-sentence. */
    if (/\b(?:give|show|tell|teach|walk)\s+me\s+(?:.{0,30}?)?(?:how\s+to|instructions|steps?|the\s+steps|a\s+tutorial|a\s+guide)\b/i.test(s))
        return true;
    if (/\b(?:step[- ]by[- ]step|step\s+by\s+step)\s+(?:instructions|guide|tutorial|process|setup|directions)/i.test(s))
        return true;
    /* "how to X" / "how do I X" / "how can I X" anywhere in the
     * sentence (was previously anchored to ^ -- too strict). */
    if (/\bhow\s+(?:do|can|would|should)\s+(?:i|you|one|a sysop)\b/i.test(s))
        return true;
    if (/\bhow\s+to\s+(?:install|configure|setup|set\s+up|build|compile|run|use|enable|disable|join|migrate|upgrade)\b/i.test(s))
        return true;
    /* Imperative verbs for sysop tasks at the start of input. */
    if (/^\s*(?:install|configure|setup|set\s+up|build|compile|join|migrate|upgrade)\s+\w/i.test(s))
        return true;
    if (/^\s*where\s+(?:do|can|is|are)\b/i.test(s)) return true;
    if (/\bconfig(?:uration)?\s+(?:option|setting|file|key)\b/i.test(s))
        return true;
    /* Synchronet-specific config-file / utility names. */
    if (/\b(?:sbbs\.ini|main\.ini|modopts\.ini|services\.ini|ctrl\.ini|freqit\.ini|sbbsecho\.ini|binkd\.ini)\b/i.test(s))
        return true;
    if (/\b(?:scfg|sbbsctrl|jsexec|sbbsecho|binkd|sbbslist|smbutil|chksmb|fixsmb|useredit|sexyz|baja|unbaja)\b/i.test(s))
        return true;
    /* Install + OS combo is almost always a howto query.  Match both
     * the verb ("install") and the gerund ("installing") forms. */
    if (/\binstall(?:ing)?\b/i.test(s)
        && /\b(?:ubuntu|debian|linux|windows|freebsd|openbsd|macos|mac|fedora|rhel|centos|arch|wsl)\b/i.test(s))
        return true;
    /* "instructions for/on installing|setting up|configuring|building|
     * compiling X" -- "instructions" with a verb-shaped object. */
    if (/\binstructions\s+(?:for|on|to|about)\s+(?:install|setup|set\s+up|configur|build|compil|run|join)/i.test(s))
        return true;
    /* "walk me through (installing|setting up|configuring|...)". */
    if (/\bwalk\s+me\s+through\s+\S*(?:install|setup|set\s+up|configur|build|compil|run|join|use)/i.test(s))
        return true;
    return false;
}

/* Return a new source-weights map with the dokuwiki weight
 * multiplied.  Other source weights pass through unchanged.  Tolerates
 * an undefined base (defaults to dokuwiki=multiplier, others=1.0). */
function _wiki_boosted_weights(base, multiplier) {
    var out = {};
    if (base && typeof base === 'object') {
        for (var k in base) out[k] = base[k];
    }
    var cur = (out.dokuwiki !== undefined) ? out.dokuwiki : 1.0;
    out.dokuwiki = cur * multiplier;
    return out;
}

/* Self-referential / identity / social questions that RAG cannot answer
 * and actively HARMS: retrieving board + wiki content for "what's your
 * name?" injects text about the BBS that the model then conflates with
 * its own identity ("I'm Vertrauen BBS").  These should be answered from
 * the system prompt's identity rules alone, with no retrieved context.
 * Kept deliberately narrow -- only clearly about-the-bot / pleasantry
 * phrasings -- so genuine knowledge questions still retrieve. */
function _is_conversational_query(input)
{
    var s = String(input || '').toLowerCase();
    /* identity: who/what are you, your name/role, are you a bot/sysop */
    if (/\b(who|what)\s+(are|r)\s+(you|u|ya)\b/.test(s)) return true;
    if (/\bwhat'?s\s+your\s+name\b/.test(s)) return true;
    if (/\byour\s+(name|identity|role|job|purpose|gender|age)\b/.test(s)) return true;
    if (/\bare\s+you\s+(a|an|the)?\s*(real\s+)?(bot|ai|a\.i\.|human|person|robot|chat\s?bot|assistant|sysop|co.?sysop|guru|alive|sentient|conscious|there)\b/.test(s)) return true;
    if (/\b(introduce|tell\s+me\s+about)\s+yourself\b/.test(s)) return true;
    if (/\bwhat\s+(can|do)\s+you\s+do\b/.test(s)) return true;
    /* social pleasantries */
    if (/\b(how\s+are\s+(you|u|ya)|how'?s\s+it\s+going|how\s+do\s+you\s+do)\b/.test(s)) return true;
    if (/^\s*(thanks|thank\s+you|thx|ty|cheers|good\s+(morning|afternoon|evening|night)|gm|gn)\b/.test(s)) return true;
    return false;
}

function inject_retrieval(input, ctx, cfg)
{
    ctx._rag_hits = 0;   /* default for profile metric */
    if (!cfg.index_top_k) return;
    /* Don't retrieve for identity/social queries -- RAG can't help and
     * the injected board/wiki text confuses the bot's sense of self. */
    if (_is_conversational_query(input)) { ctx._rag_skipped = 'conversational'; return; }
    var tokens = tokenize_query(input);
    if (tokens.length < 1) return;   /* all stopwords / nothing to query */
    var persona = ctx.persona && ctx.persona.code;
    var idx = load_index(persona, cfg);
    if (!idx) return;
    var halflife = parseFloat(cfg.index_recency_halflife);
    if (isNaN(halflife)) halflife = 0;
    /* Bias source weights toward wiki for documentation-style
     * queries.  "How do I X", "where in the docs", "is there a
     * wiki page for X", "how to <verb>" -- these all want sysop
     * documentation, not filebase descriptions or msgbase chatter.
     * Multiplies the dokuwiki weight by ~1.8x for this turn; the
     * baseline cfg weights are unchanged across turns. */
    var weights = cfg.index_source_weights;
    if (_is_docstyle_query(input)) {
        weights = _wiki_boosted_weights(weights, 1.8);
        ctx._rag_wiki_boost = true;   /* profile breadcrumb */
    }
    var hits = bm25_search(idx, tokens, cfg.index_top_k,
                           weights, halflife,
                           cfg.group_aliases, input);
    if (!hits.length) return;

    var min_per_token = parseFloat(cfg.index_min_score_per_token);
    if (isNaN(min_per_token)) min_per_token = 3.5;
    var per_token = hits[0].score / tokens.length;
    ctx._rag_top_score = hits[0].score;     /* for profile breadcrumb */
    ctx._rag_per_token = per_token;
    /* URL-append floor is higher than the inject floor (see
     * append_wiki_url_if_missing()).  Stashed here while cfg is in
     * scope; consumed in postprocess.  Also stash the query tokens
     * so the post-process can require lexical overlap between the
     * query and the top-hit URL path before appending it -- BM25
     * can return a high-scoring hit that's actually off-topic
     * (e.g. person:digital_man for "what was PC-Pursuit?"). */
    ctx._rag_url_append_floor = min_per_token * 1.5;
    ctx._rag_query_tokens = tokens;
    if (per_token < min_per_token) {
        /* Below quality threshold -- top hit isn't really about the
         * query's content words. Don't inject; let the system prompt's
         * "you haven't seen anything about it" rule apply. */
        return;
    }

    ctx._rag_hits = hits.length;
    ctx.retrieved_context = format_retrieved_context(hits);

    /* Stash the top-hit Wiki URL (if any) so the post-process pass can
     * append it to the model's reply when the model doesn't cite it
     * itself.  See append_wiki_url_if_missing() below.  At 7B,
     * qwen2.5 routinely ignores retrieved wiki chunks for well-known
     * topics (FidoNet, file management, etc.); appending the URL
     * gives the caller the right pointer regardless of what the
     * model writes.
     *
     * Also collect the FULL set of wiki URLs the model "sees" this
     * turn (via the RAG context).  strip_fake_urls() uses this as
     * the allowlist when policing wiki citations -- the model is
     * routinely caught truncating real page names (e.g. emitting
     * /network:irc when the real provenance is /network:irc.synchro.net). */
    ctx._valid_wiki_urls = {};
    for (var hi = 0; hi < hits.length; hi++) {
        var prov = hits[hi].provenance || '';
        var m = prov.match(/^Wiki:(https?:\/\/\S+)/);
        if (m) {
            ctx._valid_wiki_urls[m[1].toLowerCase()] = true;
            if (!ctx._rag_top_wiki_url) ctx._rag_top_wiki_url = m[1];
        }
    }
}

/* All URLs known to the curated external_archives index.  Lazy +
 * cached for the process lifetime.  Keys are lowercase URLs. */
var _ARCHIVE_URLS_CACHE = null;
function _archive_urls_set() {
    if (_ARCHIVE_URLS_CACHE !== null) return _ARCHIVE_URLS_CACHE;
    _ARCHIVE_URLS_CACHE = {};
    try {
        var path = system.ctrl_dir + 'llm_external_archives.json';
        var f = new File(path);
        if (!f.open('r')) return _ARCHIVE_URLS_CACHE;
        var txt = f.read();
        f.close();
        var data = JSON.parse(txt);
        var entries = (data && data.entries) || [];
        for (var i = 0; i < entries.length; i++) {
            var url = String(entries[i].url || '').toLowerCase();
            if (url) _ARCHIVE_URLS_CACHE[url] = true;
        }
    } catch (e) {
        log(LOG_WARNING, '_archive_urls_set: ' + e);
    }
    return _ARCHIVE_URLS_CACHE;
}

/* Strip URLs that look fabricated -- the model wrote a path under a
 * known domain (wiki.synchro.net, textfiles.com, bbsdocumentary.com,
 * anticlimactic.org) that isn't in either the RAG hits' provenance
 * set (wiki) or the curated archive index.  The model is observed
 * to truncate real page names ("network:irc" instead of
 * "network:irc.synchro.net") and invent plausible-looking paths for
 * topics it "knows" but has no tool result for ("textfiles.com/raids/
 * stevetx.html", "bbsdocumentary.com/services/pc-pursuit").  Strip
 * the URL token, leave surrounding prose; the postprocess collapses
 * the whitespace. */
function strip_fake_urls(reply, ctx) {
    if (!reply) return reply;
    var valid_wiki = (ctx && ctx._valid_wiki_urls) || {};
    var valid_arch = _archive_urls_set();
    /* Require a path segment after the domain so we don't flag bare
     * domain mentions ("textfiles.com is great") as URLs.  Protocol
     * is optional so the model writing "textfiles.com/foo" gets
     * caught too. */
    var re = /(?:https?:\/\/)?(?:[\w-]+\.)*(?:wiki\.synchro\.net|textfiles\.com|bbsdocumentary\.com|anticlimactic\.org)\/[\w\-./:?#%=&@~+]+/gi;
    var stripped = reply.replace(re, function (url) {
        /* Add protocol if missing, for lookup vs. our valid sets
         * (which store fully-qualified URLs).  Default to https
         * for textfiles/wiki/anticlimactic, http for bbsdocumentary
         * (matches what's in the JSON). */
        var lookup = url.toLowerCase();
        if (!/^https?:\/\//.test(lookup)) {
            var proto = /bbsdocumentary\.com/.test(lookup) ? 'http://' : 'https://';
            lookup = proto + lookup;
        }
        var key = lookup.replace(/#.*$/, '')
                        .replace(/[.,;:!?)\]]+$/, '');
        if (valid_wiki[key]) return url;
        if (valid_arch[key]) return url;
        /* Slash-suffix tolerance: curated may be /foo/, model wrote /foo. */
        if (valid_wiki[key + '/']) return url;
        if (valid_arch[key + '/']) return url;
        if (key.charAt(key.length - 1) === '/') {
            var trimmed = key.replace(/\/+$/, '');
            if (valid_wiki[trimmed] || valid_arch[trimmed]) return url;
        }
        /* http <-> https tolerance.  JSON has e.g. http://www.bbsdocumentary.com/
         * but the model often writes https://.  Try the opposite scheme. */
        var swapped = /^https/.test(key)
            ? key.replace(/^https/, 'http')
            : key.replace(/^http/, 'https');
        if (valid_wiki[swapped]) return url;
        if (valid_arch[swapped]) return url;
        return '';
    });
    /* Clean up artifacts left where a URL was stripped:
     *   - "at <>" / "<>" residue when model wrapped url in angle brackets
     *   - "[text]()" / "[text]( )" markdown link with stripped URL
     *   - "See  ." / "See ." with multiple spaces from stripped URL
     *   - Trailing "this URL: " / "at this link: " with no URL after */
    stripped = stripped.replace(/<\s*>/g, '');
    stripped = stripped.replace(/\[([^\]]+)\]\(\s*\)/g, '$1');
    stripped = stripped.replace(/\b(?:at|see|visit|this URL|this link|here)\s*:?\s*([.,;])/gi, '$1');
    /* Trim a trailing "look here for more" lead-in that no longer
     * points anywhere (the URL was stripped above).  Conservative
     * patterns: only fires at the end of the reply, with no
     * meaningful text after the lead-in. */
    stripped = stripped.replace(
        /[,.\s]+(?:for more (?:info|information|details),?\s+)?(?:you can\s+)?(?:see|visit|refer to|find|check out|explore|read(?: more)?(?: about it)?)\s*(?:at|on|in|via)?\s*(?:this\s+(?:URL|link|page))?\s*[:.]?\s*$/i,
        '.'
    );
    /* Also: trailing "More information can be found" / "this URL:" alone. */
    stripped = stripped.replace(/[,.\s]+(?:more (?:info|information|details))\s+(?:can be found|is available)?\s*[:.]?\s*$/i, '.');
    stripped = stripped.replace(/[,.\s]+(?:this\s+(?:URL|link|page))\s*[:.]?\s*$/i, '.');
    /* Collapse runs of whitespace that strip left behind. */
    stripped = stripped.replace(/[ \t]{2,}/g, ' ');
    stripped = stripped.replace(/\s+([.,;:!?])/g, '$1');
    /* Avoid duplicate periods if the cleanup added one. */
    stripped = stripped.replace(/\.{2,}$/, '.');
    return stripped;
}

/* Append the top retrieved wiki URL to the reply if the reply doesn't
 * already mention it.  Band-aid over the 7B-model failure to follow
 * "cite the URL from the chunks" instructions for well-known topics
 * -- the model writes a training-data reply, we tack on the correct
 * pointer.  See memory note reference_7b_training_prior_overrides_rag. */
function append_wiki_url_if_missing(reply, ctx)
{
    if (!reply || !ctx || !ctx._rag_top_wiki_url) return reply;
    /* Higher confidence bar for appending a wiki URL than for
     * injecting RAG context.  The injection threshold lets weak
     * hits provide context to the model; but tacking the URL onto
     * the reply implies "this is THE authoritative page", which
     * is wrong when the top chunk just happened to share a couple
     * of common tokens with the query.  Require the top hit to
     * be meaningfully above the floor.  Stashed as _rag_url_append_floor
     * by retrieve_and_inject_rag() while cfg was in scope. */
    var per_token = parseFloat(ctx._rag_per_token);
    var append_floor = parseFloat(ctx._rag_url_append_floor);
    if (isNaN(append_floor)) append_floor = 5.25;
    if (isNaN(per_token) || per_token < append_floor) return reply;
    var url = ctx._rag_top_wiki_url;
    /* Require lexical overlap between the query and the wiki URL
     * path.  BM25 can return a high-scoring hit that's topically
     * off (e.g. person:digital_man scoring well for "what was
     * PC-Pursuit?" because of shared "BBS" / "history" tokens in
     * the chunk body).  Tokenize the URL's path (split on
     * non-alphanumerics) and the query's tokens; require at least
     * one >2-char overlap. */
    var query_tokens = ctx._rag_query_tokens;
    if (query_tokens && query_tokens.length) {
        var path = url.replace(/^https?:\/\/[^\/]+\/?/, '').toLowerCase();
        var url_tokens = {};
        var parts = path.split(/[^a-z0-9]+/);
        for (var pi = 0; pi < parts.length; pi++) {
            if (parts[pi].length >= 3) url_tokens[parts[pi]] = true;
        }
        var overlap = false;
        for (var qi = 0; qi < query_tokens.length; qi++) {
            var qt = String(query_tokens[qi]).toLowerCase();
            if (qt.length >= 3 && url_tokens[qt]) { overlap = true; break; }
        }
        if (!overlap) return reply;
    }
    /* Already cited (exact URL match or normalized -- strip trailing
     * punctuation that often follows a URL in prose). */
    if (reply.indexOf(url) >= 0) return reply;
    /* Don't append if the reply admits no answer ("I don't know" /
     * "no information") -- that's a legitimate response, no need to
     * point at an unrelated wiki page. */
    if (/\b(i don'?t (?:know|have|see)|no (?:info|information|results|matching)|not aware of)\b/i.test(reply))
        return reply;
    /* Don't append a generic wiki URL when the reply already cites
     * a tool-supplied URL (git_issues/git_commits -> gitlab,
     * external_archives -> textfiles.com / bbsdocumentary.com).
     * The tool-supplied URL is more relevant than whatever the top
     * BM25 wiki chunk was. */
    if (/gitlab\.synchro\.net\/main\/sbbs/.test(reply)) return reply;
    if (/(?:^|\W)(?:textfiles\.com|bbsdocumentary\.com|anticlimactic\.org\/Citadel)/.test(reply))
        return reply;
    return reply.replace(/\s+$/, '') + ' See ' + url + ' for the official Synchronet docs.';
}

/* --- standalone mode: jsexec chat_llm.js [-u N] "your message" --- */

/* Standalone-mode auto-run: fires when chat_llm.js is invoked as a
 * standalone script via jsexec, NOT when it's load()ed by another
 * script.  Skipped when:
 *   - bbs is defined (we're inside a BBS session)
 *   - the loading script set CHAT_LLM_NO_STANDALONE = true beforehand
 *     (chat_llm_irc.js, multinode dispatch, etc. -- callers who load
 *     chat_llm.js for its functions, not to execute its standalone
 *     greeting/chat flow).  Without this guard, loading the module
 *     into a services-subsystem script (where `bbs` is also undefined)
 *     would make an unwanted LLM call AND potentially call exit(1)
 *     on failure, killing the calling script. */
if (typeof bbs == 'undefined'
    && typeof CHAT_LLM_NO_STANDALONE == 'undefined') {
    var args = argv ? argv.slice(0) : [];
    var user_num = 0;
    while (args.length >= 2 && args[0] == '-u') {
        user_num = parseInt(args[1], 10);
        args.splice(0, 2);
    }
    var greet_only = (args.length == 0);

    var input = args.join(' ');
    var u = null;
    if (user_num > 0) {
        u = new User(user_num);
        if (!u.number) {
            writeln('ERROR: no such user #' + user_num);
            exit(1);
        }
    } else if (typeof useron != 'undefined' && useron && useron.number) {
        u = useron;
    }

    var ctx = ctx_from_user(u, 'default', 'The Guru', true);

    var start = system.timer;
    try {
        var reply;
        if (greet_only) {
            reply = open_session(ctx);
            if (reply === null) {
                writeln('(opening_prompt is blank; bot would wait silently)');
                exit(0);
            }
        } else {
            reply = chat_session(input, ctx);
        }
        var elapsed = system.timer - start;
        writeln('-- caller: ' + (u ? u.alias + ' (#' + u.number + ')' : '(none)'));
        if (!greet_only) {
            writeln('-- input  --------');
            writeln(input);
        }
        writeln('-- ' + (greet_only ? 'greeting' : 'reply') + ' (' + elapsed.toFixed(2) + 's) --');
        writeln(reply);
    } catch (e) {
        writeln('ERROR: ' + e);
        exit(1);
    }
}
