/*
 * llm_tools/relay_message.js -- the relay_message tool.
 *
 * Stores a message to be delivered the next time a specific user
 * speaks in any channel the bot is in.  The drain side lives in
 * chat_llm_irc.js (load_relay() / deliver_pending()); both sides share
 * the data/guru_irc_relay.json queue file.
 *
 * The tool takes env from llm_tools_execute(): env.speaker.alias is
 * used to refuse self-relay, and env.channel_members / env.seen_members
 * help validate the recipient nick against the current channel context.
 */

/* Pending-relay queue path -- shared with chat_llm_irc.js's
 * load_relay()/deliver_pending().  Both sides read/write the same
 * JSON file; the tool side appends, the bot side drains. */
var RELAY_PATH = (typeof system != 'undefined' && system.data_dir)
    ? system.data_dir + 'guru_irc_relay.json' : 'data/guru_irc_relay.json';

function _read_relay() {
    if (typeof File == 'undefined') return { messages: [] };
    var f = new File(RELAY_PATH);
    if (!f.open('r')) return { messages: [] };
    var raw = f.read();
    f.close();
    try {
        var s = JSON.parse(raw);
        if (s && s.messages) return s;
    } catch (e) {}
    return { messages: [] };
}
function _write_relay(state) {
    if (typeof File == 'undefined') return;
    var f = new File(RELAY_PATH);
    if (!f.open('w')) return;
    f.write(JSON.stringify(state, null, 2));
    f.close();
}

/* Resolve a recipient name to a canonical form by consulting:
 *   1. The current IRC channel's NAMES list (passed via env)
 *   2. Past chat-history filenames in data/chat/
 *   3. The Vertrauen local user database (system.matchuser)
 * Returns { canonical, source } on success, or { suggestions: [...] }
 * on miss (fuzzy-match suggestions, up to 3, drawn from the same
 * three sources).  Lookups are case-insensitive throughout. */
function _resolve_recipient(query, env) {
    var q = String(query || '').replace(/^\s+|\s+$/g, '');
    if (!q) return { suggestions: [] };
    var ql = q.toLowerCase();

    /* Source 1: channel members (env.channel_members is a name-keyed
     * map of lowercase -> canonical case provided by the IRC bot). */
    var members = (env && env.channel_members) || {};
    if (members[ql]) return { canonical: members[ql], source: 'channel' };

    /* Source 1b: ever-seen members in this channel (env.seen_members
     * is the bot's persisted "we have observed this nick here before"
     * set).  Lets a relay target a nick that was here yesterday but
     * isn't connected right now -- the relay queue holds it for them
     * the next time they speak. */
    var seen = (env && env.seen_members) || {};
    if (seen[ql]) return { canonical: seen[ql], source: 'seen' };

    /* Source 2: chat history filenames.  Scan data/chat/ for files
     * matching irc_<host>_<nick>.<persona>.json -- the embedded
     * nick is the canonical case from when we recorded it. */
    var chat_dir = (typeof system != 'undefined' && system.data_dir)
        ? system.data_dir + 'chat' : null;
    var history_nicks = {};
    if (chat_dir && typeof directory == 'function') {
        var files = [];
        try { files = directory(chat_dir + '/*.json') || []; } catch (e) {}
        for (var i = 0; i < files.length; i++) {
            var base = String(files[i]).split(/[\/\\]/).pop();
            /* irc_<host>_<nick>.<persona>.json */
            var m = base.match(/^irc_[^_]+\.[^_]+\.[^_]+_([^.]+)\./);
            if (m && m[1]) history_nicks[m[1].toLowerCase()] = m[1];
        }
    }
    if (history_nicks[ql]) return { canonical: history_nicks[ql], source: 'history' };

    /* Source 3: local Vertrauen user.  system.matchuser returns the
     * record number for an alias / name (case-insensitive), or 0. */
    if (typeof system != 'undefined' && typeof system.matchuser == 'function') {
        try {
            var n = system.matchuser(q);
            if (n > 0) {
                var u = new User(n);
                if (u && u.number) {
                    return { canonical: u.alias || q, source: 'local-user' };
                }
            }
        } catch (e) {}
    }

    /* Miss -- assemble fuzzy-match suggestions.  Score by substring
     * match (recipient-in-candidate or vice versa).  Pool draws from
     * everyone currently in-channel, ever-seen in-channel, and past
     * chat partners. */
    var candidates = {};
    for (var k in members)        candidates[k] = members[k];
    for (var k in seen)           if (!candidates[k]) candidates[k] = seen[k];
    for (var k in history_nicks)  if (!candidates[k]) candidates[k] = history_nicks[k];
    var suggestions = [];
    for (var k in candidates) {
        if (k === ql) continue;
        if (k.indexOf(ql) >= 0 || ql.indexOf(k) >= 0) {
            suggestions.push(candidates[k]);
            if (suggestions.length >= 3) break;
        }
    }
    return { suggestions: suggestions };
}

function relay_message(args, env) {
    var recipient = String((args && args.recipient) || '').replace(/^\s+|\s+$/g, '');
    var text      = String((args && args.text) || '').replace(/^\s+|\s+$/g, '');
    if (!recipient) {
        return { error: 'I need the recipient nick (who the message is for).' };
    }
    if (!text) {
        return { error: 'I need the message text to relay.' };
    }
    /* Refuse self-relay (recipient == sender or recipient == bot). */
    var from = (env && env.speaker && env.speaker.alias) || '(unknown)';
    if (recipient.toLowerCase() === String(from).toLowerCase()) {
        return { error: 'You\'re asking me to deliver a message to yourself -- I\'ll let you handle that one.' };
    }

    var resolved = _resolve_recipient(recipient, env);
    if (!resolved.canonical) {
        var sug = (resolved.suggestions && resolved.suggestions.length)
            ? ' Did you mean: ' + resolved.suggestions.join(', ') + '?'
            : '';
        /* Wording note: avoid mentioning THIS_BBS_NAME as a noun --
         * the model paraphrases the error and a 7B head latches
         * onto the system name as a "did you mean" candidate
         * (observed: "Did you mean Vertrauen?" for a funbot relay). */
        return {
            error: 'No record of a user / nick called "' + recipient
                 + '" -- they are not currently in any channel I am '
                 + 'in, have never chatted with me, and have no '
                 + 'local account on this BBS.' + sug
        };
    }

    /* Cap queue length per recipient to keep things sane (one user
     * shouldn't stockpile dozens of relays against one target). */
    var state = _read_relay();
    var per_recipient = 0;
    var canon_lower = resolved.canonical.toLowerCase();
    for (var i = 0; i < state.messages.length; i++) {
        if (String(state.messages[i].recipient || '').toLowerCase()
            === canon_lower) per_recipient++;
    }
    if (per_recipient >= 5) {
        return {
            error: 'I am already holding 5 pending messages for '
                 + resolved.canonical + '. Try again after some of those '
                 + 'have been delivered.'
        };
    }

    state.messages.push({
        recipient: resolved.canonical,
        from:      from,
        text:      text,
        ts:        time(),
        queued_in: (env && env.channel) || ''
    });
    _write_relay(state);
    return {
        ok:        true,
        recipient: resolved.canonical,
        source:    resolved.source,
        queued:    state.messages.length,
        note:      'Stored. Will deliver the next time ' + resolved.canonical
                 + ' speaks in any channel I am in.'
    };
}

llm_tool_register({
    name: 'relay_message',
    execute: relay_message,
    def: {
        type: 'function',
        'function': {
            name: 'relay_message',
            description:
                'CALL THIS to store a message to be delivered the next time '
              + 'a specific user speaks in any channel the bot is in.  Use '
              + 'when the caller asks you to pass a message to someone who '
              + 'is not currently here.\n\n'
              + 'TRIGGER PHRASES:\n'
              + '  - "next time you see <nick>, tell them <msg>"\n'
              + '  - "tell <nick> <msg>" / "let <nick> know <msg>"\n'
              + '  - "pass on to <nick> that <msg>"\n'
              + '  - "leave a message for <nick>: <msg>"\n\n'
              + 'Args: { recipient: <nick>, text: <message body> }.  Pass the '
              + 'recipient nick as the caller specified it; the tool validates '
              + 'it against the current channel\'s NAMES list, past chat '
              + 'history, and the local user base.  If validation fails, '
              + 'returns an error WITH up to 3 close matches as "Did you mean: '
              + 'X, Y, Z?" -- relay those suggestions to the caller verbatim '
              + 'and ask them to confirm.\n\n'
              + 'On success: returns { ok: true, recipient: <canonical-case>, '
              + 'source, queued, note }.  Acknowledge briefly: "Got it -- I\'ll '
              + 'let <recipient> know."  Do NOT echo the JSON.\n\n'
              + 'Do NOT call this for a message the caller wants to deliver '
              + 'immediately to themselves or to the bot.',
            parameters: {
                type: 'object',
                properties: {
                    recipient: { type: 'string',
                                 description: 'The IRC nick (or BBS alias) to deliver the message to.' },
                    text:      { type: 'string',
                                 description: 'The message body to relay.' }
                },
                required: ['recipient', 'text']
            }
        }
    }
});
