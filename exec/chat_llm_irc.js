/*
 * exec/chat_llm_irc.js -- IRC bot adapter for the chat_llm engine
 *
 * Standalone IRC client that joins a channel and lets the chat_llm
 * guru participate, but only in two cases:
 *
 *   1. DIRECT ADDRESS -- someone says the bot's nick in channel.
 *      The bot replies in the channel, prefixing the questioner's
 *      nick so they see it's directed at them.
 *
 *   2. HIGH-CONFIDENCE INTERVENTION -- someone asked a question (line
 *      ending in '?'), nobody responded for `irc_intervention_wait`
 *      seconds, and the bot's BM25 retrieval score for the question
 *      passes a "confident enough to chime in unprompted" threshold.
 *      A question explicitly aimed at another participant ("nick: ...?")
 *      is NOT a room question -- the bot stays out of it (see
 *      directed_at_other).  Even past the BM25 gate the engine gets a
 *      final say: the dispatch runs with ctx.volunteering, so chat_llm.js
 *      abstains (returns null, nothing spoken or stored) unless the
 *      retrieved docs fully support a correct answer -- a guessy/wrong
 *      unsolicited answer is worse than silence.  When it does answer,
 *      the reply is prefixed with the asker's nick.  Cooldown between
 *      interventions per channel prevents the bot from dominating.
 *
 * Otherwise the bot lurks silently.
 *
 * PER-USER MUTE: a user can address the bot with "mute me" (or "shut
 * up", "be quiet", "leave me alone", ...) to make it stop replying to
 * them and stop volunteering answers to their questions; "unmute me"
 * (or "talk to me", "come back", ...) resumes.  Per-user, keyed by
 * nick, persisted across restarts in data/<bot_file_base>_mute.json.
 * Mute/unmute are always honored even while muted, so a user can
 * always bring the bot back.
 *
 * Run as a Synchronet service via services.ini:
 *   [GuruIRC]
 *   Cmd          = ?chat_llm_irc.js
 *   Port         = 0
 *   MaxClients   = 1
 *   Options      = STATIC | NO_HOST_LOOKUP
 *
 * Or one-shot test from jsexec:
 *   jsexec chat_llm_irc.js [persona_code]
 *
 * Config is read from chat_llm.ini's [<persona>] section (falling
 * back to [default]).  IRC-specific keys:
 *
 *   irc_network                            -- default "irc.synchro.net"
 *   irc_port                               -- default 6667
 *   irc_channels                           -- comma-separated list of
 *                                             channels to join;
 *                                             default "#synchronet"
 *   irc_nick                               -- default "<qwk_id>_<persona_name>",
 *                                             collisions get a "_" suffix
 *   irc_aliases                            -- comma-separated extra names
 *                                             the bot also answers to in
 *                                             channel address-detection,
 *                                             e.g. "guru, Guru, The Guru".
 *                                             The IRC nick is always
 *                                             matched in addition to these.
 *   irc_nickserv_password                  -- optional NickServ IDENTIFY
 *   irc_intervention_wait                  -- seconds to wait before chiming
 *                                             in on an unanswered question
 *                                             (default 120)
 *   irc_intervention_cooldown              -- minimum seconds between
 *                                             interventions (default 900)
 *   irc_intervention_min_score_per_token   -- BM25 per-token score floor
 *                                             for "high confidence"
 *                                             (default 8.0; baseline
 *                                             injection threshold is 3.5)
 *   irc_intervention_min_tokens            -- minimum content-token count a
 *                                             question must have to qualify
 *                                             for intervention; a single
 *                                             content word can't carry a
 *                                             "high confidence" score
 *                                             (default 2)
 *   irc_ack_enabled                        -- acknowledge a clear, brief
 *                                             reply to the bot's OWN last
 *                                             line: a social closer ("ah
 *                                             ok", "thanks"), a correction
 *                                             ("no, that's wrong"), or an
 *                                             answer to a question the bot
 *                                             itself just asked. One short
 *                                             in-character line, bare (no
 *                                             nick prefix); never two in a
 *                                             row (you always get the last
 *                                             word). Default on; set false
 *                                             to disable.
 *   irc_ack_window                         -- max seconds after the bot's
 *                                             line for a reply to count as
 *                                             an ack (default 90)
 *   irc_ack_cooldown                       -- minimum seconds between acks
 *                                             per channel (default 90)
 *   irc_ack_closer_chance                  -- probability (0..1) of acking a
 *                                             social closer; corrections are
 *                                             always handled (default 0.9)
 */

load("sockdefs.js");
load("sbbsdefs.js");

/* Suppress chat_llm.js's standalone-mode auto-run.  We're loading it
 * for its functions (chat_session, load_config, etc.) -- we DON'T
 * want its top-level "make a sample LLM call and exit(0/1)" block to
 * fire when we're running as a service.  Must be set BEFORE load(). */
var CHAT_LLM_NO_STANDALONE = true;
load("chat_llm.js");

/* ---- Config ---- */

/* Default persona is "guru:irc" -- looks up [guru:irc] section in
 * chat_llm.ini (model override, etc.) layered on top of [default].
 * Override with command-line arg if the sysop wants a different
 * persona configuration for this bot instance. */
/* Lowercase the persona code at the source so it's case-stable everywhere
 * it becomes a filename (BOT_FILE_BASE, the index key, per-user memory) --
 * a "GURU:IRC" arg can't produce files distinct from "guru:irc". */
var PERSONA = String((argv && argv[0]) || "guru:irc").toLowerCase();
var cfg     = load_config(PERSONA);

var IRC_HOST    = cfg.irc_network || "irc.synchro.net";
var IRC_PORT    = parseInt(cfg.irc_port, 10) || 6667;
/* IRC channels to join (comma-separated). */
var IRC_CHANNELS = (function () {
    var parts = String(cfg.irc_channels || "#synchronet").split(",");
    var out = [];
    for (var i = 0; i < parts.length; i++) {
        var c = parts[i].replace(/^\s+|\s+$/g, "");
        if (c) out.push(c);
    }
    return out;
}());

function is_our_channel(name) {
    if (!name) return false;
    var lower = name.toLowerCase();
    for (var i = 0; i < IRC_CHANNELS.length; i++) {
        if (IRC_CHANNELS[i].toLowerCase() === lower) return true;
    }
    return false;
}

/* Default nick: <qwk_id>_<persona_name>, e.g. "VERT_The_Guru".
 * Sanitize to IRC-valid characters -- nicks can't contain ':'
 * (which is the separator in "guru:irc"-style persona codes!),
 * spaces, or various other punctuation.  Strip the mode-suffix
 * from the persona code (everything after ':') and collapse any
 * remaining invalid chars to '_'. */
function sanitize_irc_nick(s) {
    s = String(s).split(":")[0];                          /* strip mode suffix */
    s = s.replace(/\s+/g, "_");                           /* spaces -> _ */
    s = s.replace(/[^A-Za-z0-9_\-\[\]\\\^\{\|\}]/g, "_"); /* keep IRC-valid chars only */
    return s;
}
var DEFAULT_NICK = sanitize_irc_nick((system.qwk_id || "BBS")
                                    + "_"
                                    + (cfg.bot_name || PERSONA));
var IRC_NICK = sanitize_irc_nick(cfg.irc_nick || DEFAULT_NICK);

var IRC_NICKSERV_PASSWORD     = cfg.irc_nickserv_password || "";
var INTERVENTION_WAIT         = parseInt(cfg.irc_intervention_wait, 10) || 120;
var INTERVENTION_COOLDOWN     = parseInt(cfg.irc_intervention_cooldown, 10) || 900;
var INTERVENTION_MIN_SCORE    = parseFloat(cfg.irc_intervention_min_score_per_token) || 8.0;
var INTERVENTION_MIN_TOKENS   = parseInt(cfg.irc_intervention_min_tokens, 10) || 2;

/* Unprompted acknowledgement of a clear, brief reply to the bot's own
 * last line (a social closer or a correction).  Heavily gated; see
 * ack_kind() + the ack block in handle_privmsg().  Default ON. */
var ACK_ENABLED               = !/^(?:false|0|no|off)$/i.test(String(cfg.irc_ack_enabled));
var ACK_WINDOW                = parseInt(cfg.irc_ack_window, 10) || 90;
var ACK_COOLDOWN              = parseInt(cfg.irc_ack_cooldown, 10) || 90;
var ACK_CLOSER_CHANCE         = (cfg.irc_ack_closer_chance !== undefined
                                 && cfg.irc_ack_closer_chance !== '')
                                ? parseFloat(cfg.irc_ack_closer_chance) : 0.9;

/* Build the alias list -- names the bot answers to when uttered in
 * channel.  The current IRC nick is ALWAYS matched (added at lookup
 * time via current_address_names so 433 collision suffix is picked
 * up).  Only explicit sysop-provided aliases are added here.
 *
 * Persona code ("guru") is NOT auto-added because short common words
 * cause false-positive triggers in public channels -- any sentence
 * mentioning "guru" would address the bot.  Sysops who want short
 * aliases should set them explicitly in irc_aliases. */
function build_aliases(extra) {
    var names = [];
    function add(n) {
        n = String(n || "").replace(/^\s+|\s+$/g, "");
        if (!n) return;
        for (var i = 0; i < names.length; i++)
            if (names[i].toLowerCase() === n.toLowerCase()) return;
        names.push(n);
    }
    if (extra) {
        var parts = String(extra).split(",");
        for (var i = 0; i < parts.length; i++) add(parts[i]);
    }
    return names;
}
var STATIC_ALIASES = build_aliases(cfg.irc_aliases);

function current_address_names() {
    /* Combine the dynamic current nick (which may have a "_" suffix
     * after 433 collision) with the static aliases. */
    return [our_nick].concat(STATIC_ALIASES);
}

var RECONNECT_DELAY_SECONDS   = 30;
var MAX_REPLY_BYTES           = 400;  /* IRC line limit ~512, account for protocol overhead */

/* Persona-derived runtime file base.  PERSONA looks like "guru:irc"
 * (the mode suffix names the protocol).  We use "<persona>_<proto>"
 * as the base for all per-bot files so multiple bots with different
 * personas can coexist in the same data dir without collision.  For
 * the default persona "guru:irc" this gives "guru_irc". */
var BOT_FILE_BASE = (function () {
    var parts  = String(PERSONA).split(":");
    /* Run each part through safe_id() (from chat_llm.js, load()ed above)
     * so an unfortunate persona arg can't produce an invalid path or
     * escape data/chat/ (e.g. "a/b:irc" -> "a_b_irc", not a subdir). */
    var name   = safe_id(parts[0] || "guru");
    var proto  = safe_id(parts[1] || "irc");
    return name + "_" + proto;
})();

/* Per-persona subdirectory under data/chat/.  name = persona code before
 * ':' (the index-sharing unit, shared with the terminal guru), proto =
 * mode after ':' (default "irc").  The bot's persistent state
 * (relay/mute/norelay/seen) lives here, alongside the RAG index and the
 * per-speaker memory written by chat_llm.js.  The data-root chat log and
 * .announce semfile keep using BOT_FILE_BASE (short hand-touch paths). */
var PERSONA_NAME  = safe_id(String(PERSONA).split(":")[0] || "guru");
var PERSONA_PROTO = safe_id(String(PERSONA).split(":")[1] || "irc");
var PERSONA_DIR   = system.data_dir + "chat/" + PERSONA_NAME + "/";
mkdir(PERSONA_DIR);   /* fire-and-forget; false if it already exists */

/* Stop semfile -- the bot's main loop checks for this file's existence
 * every iteration.  When present, the bot sends a graceful QUIT to IRC
 * and exits BEFORE the services subsystem forces termination (which
 * happens via an uncatchable JS exception that bypasses our shutdown
 * handlers).  Sysop procedure for clean shutdown:
 *   1. touch data/<bot_file_base>.stop  (signals the bot to leave IRC)
 *   2. Wait 1-2 seconds for the QUIT message to reach the IRC server
 *   3. Then recycle / shutdown the services subsystem
 * The bot removes the semfile when it sees it. */
var STOP_SEMFILE = system.data_dir + BOT_FILE_BASE + ".stop";

/* One-shot announce file.  Sysop writes a short multi-line message
 * here BEFORE restarting the bot; on next startup, after we've
 * JOINed each configured channel, the bot posts each non-empty
 * line to the channel and then deletes the file (so we don't
 * re-announce on every reconnect within the same restart).  Use
 * for visibility into what changed: "restarted with hybrid channel
 * context awareness -- the bot can now follow cross-conversations". */
var ANNOUNCE_FILE = system.data_dir + BOT_FILE_BASE + ".announce";

/* Self-managed bot-debug log file -- sbbsctrl doesn't persist
 * services-tab output to disk, so we write our own line-buffered log
 * here for post-hoc debugging.  Appended-to on every blog() call.
 * Lives in data/ root (parallel to <persona>.log / <bot_file_base>
 * _chat.log / error.log) -- data/logs/ is for dated session-log
 * files (MMDDYY.log) and per-incident files, not for persistent
 * fixed-name event logs. */
var BOT_LOG_PATH = system.data_dir + BOT_FILE_BASE + "_bot.log";
function blog(msg) {
    /* Also pass through to the Synchronet log (sbbsctrl real-time view). */
    log(LOG_INFO, msg);
    var f = new File(BOT_LOG_PATH);
    if (!f.open("a")) return;
    /* Local-time ISO-8601 with offset (e.g. 2026-05-31T16:59:01-0700) via
     * xpdev's strftime() -- local wallclock, matching the chat log and the
     * rest of Synchronet's logs, unlike Date.toISOString()'s UTC.  No
     * sub-second field (C strftime has none) and the offset uses the basic
     * colon-less form. */
    var ts = strftime("%Y-%m-%dT%H:%M:%S%z", time());
    f.write(ts + " " + msg + "\r\n");
    f.close();
}

/* Q/A chat log -- mirrors the format the C++ caller writes to
 * <persona>.log for terminal/multinode chat (timestamp, speaker,
 * input, persona, reply, [prof] line).  Lives at <bot_file_base>
 * _chat.log so it doesn't interleave with the terminal-side
 * <persona>.log.  Appended-to after each successful chat_session
 * dispatch. */
var IRC_CHAT_LOG = system.data_dir + BOT_FILE_BASE + "_chat.log";

/* Pending message-relay queue.  Written by the relay_message tool
 * (llm_tools/relay_message.js) when a caller asks the bot to pass a
 * message to another user; drained by deliver_pending() when that
 * user next joins or speaks in ANY channel the bot is in.  Persisted across
 * bot restarts so messages don't get lost on restart. */
var IRC_RELAY_PATH = PERSONA_DIR + PERSONA_PROTO + "_relay.json";
function load_relay() {
    if (!file_exists(IRC_RELAY_PATH)) return { messages: [] };
    var f = new File(IRC_RELAY_PATH);
    if (!f.open("r")) return { messages: [] };
    var raw = f.read();
    f.close();
    try {
        var s = JSON.parse(raw);
        if (s && s.messages) return s;
    } catch (e) {}
    return { messages: [] };
}
function save_relay(state) {
    var f = new File(IRC_RELAY_PATH);
    if (!f.open("w")) return;
    f.write(JSON.stringify(state, null, 2));
    f.close();
}

/* ---- Per-user mute ----
 * A user can tell the bot "mute me" (and variants) to stop it replying
 * to them AND auto-intervening on their questions; "unmute me" resumes.
 * Persisted to data/<bot_file_base>_mute.json so a restart doesn't
 * silently un-mute everyone.  Keyed by lowercased nick (IRC nicks are
 * case-insensitive), matching the relay/seen convention.  IRC-only:
 * this is the sole context where the guru speaks unprompted, so it's
 * the only place there's anything to mute. */
var IRC_MUTE_PATH = PERSONA_DIR + PERSONA_PROTO + "_mute.json";
var muted_users   = null;   /* lazy { "<lc-nick>": muted_at_ts } */
function load_mutes() {
    if (muted_users !== null) return muted_users;
    muted_users = {};
    if (!file_exists(IRC_MUTE_PATH)) return muted_users;
    var f = new File(IRC_MUTE_PATH);
    if (!f.open("r")) return muted_users;
    var raw = f.read(); f.close();
    try { var s = JSON.parse(raw); if (s && s.users) muted_users = s.users; }
    catch (e) { blog("load_mutes: corrupt " + IRC_MUTE_PATH + ": " + e); }
    return muted_users;
}
function save_mutes() {
    load_mutes();
    var f = new File(IRC_MUTE_PATH);
    if (!f.open("w")) return;
    f.write(JSON.stringify({ users: muted_users }, null, 2));
    f.close();
}
function is_muted(nick) {
    return !!load_mutes()[String(nick).toLowerCase()];
}
function set_mute(nick, on) {
    load_mutes();
    var k = String(nick).toLowerCase();
    if (on) muted_users[k] = time();
    else delete muted_users[k];
    save_mutes();
}
/* Classify a message (already stripped of the bot's address prefix) as
 * a 'mute' / 'unmute' command, or null.  Trailing . or ! tolerated. */
function mute_command(input) {
    var s = String(input || "").toLowerCase()
                .replace(/^\s+|\s+$/g, "").replace(/[.!]+$/, "");
    if (/^(unmute|unmute me|talk to me|you can talk( again)?|come back|listen( up)?|resume)$/.test(s))
        return "unmute";
    if (/^(mute|mute me|shut up|stfu|be quiet|quiet|silence|hush|shush|stop talking( to me)?|leave me alone)$/.test(s))
        return "mute";
    /* Looser, conversational "go away" phrasings -- often embedded in a
     * longer sentence ("fuck off and don't talk to me"), so matched as a
     * substring rather than anchored.  The verb must follow the
     * negative/stop word directly so we don't catch unrelated lines
     * like "how do I stop spam to me".  Note the exact-match unmute
     * test above already claimed a bare "talk to me", so the
     * "don't talk to me" form here can't be misread as an unmute. */
    if (/\b(don'?t|do not|stop|quit|no need to|never)\s+(talk|speak|respond|reply|messag|msg|pm|dm)\w*\s+(to\s+)?me\b/.test(s)
        || (!/\?$/.test(s)
            && /\b(go away|fuck off|piss off|buzz off|bugger off|get lost|leave me be)\b/.test(s)))
        return "mute";
    return null;
}

/* ---- Per-user relay opt-out ("no-relay") ----
 * A user can tell the bot "don't relay messages to me" (and variants) so
 * the relay_message tool refuses to queue anything for them; "relay me"
 * opts back in.  Persisted to data/chat/<bot_file_base>_norelay.json,
 * shared with the relay_message tool (which reads it before queuing),
 * the same way the relay queue is shared.  Lowercased-nick keyed. */
var IRC_NORELAY_PATH = PERSONA_DIR + PERSONA_PROTO + "_norelay.json";
var norelay_users    = null;   /* lazy { "<lc-nick>": optout_at_ts } */
function load_norelay() {
    if (norelay_users !== null) return norelay_users;
    norelay_users = {};
    if (!file_exists(IRC_NORELAY_PATH)) return norelay_users;
    var f = new File(IRC_NORELAY_PATH);
    if (!f.open("r")) return norelay_users;
    var raw = f.read(); f.close();
    try { var s = JSON.parse(raw); if (s && s.users) norelay_users = s.users; }
    catch (e) { blog("load_norelay: corrupt " + IRC_NORELAY_PATH + ": " + e); }
    return norelay_users;
}
function save_norelay() {
    load_norelay();
    var f = new File(IRC_NORELAY_PATH);
    if (!f.open("w")) return;
    f.write(JSON.stringify({ users: norelay_users }, null, 2));
    f.close();
}
function is_norelay(nick) {
    return !!load_norelay()[String(nick).toLowerCase()];
}
function set_norelay(nick, on) {
    load_norelay();
    var k = String(nick).toLowerCase();
    if (on) norelay_users[k] = time();
    else delete norelay_users[k];
    save_norelay();
}
/* Classify a message as a 'norelay' (opt-out) / 'relay-ok' (opt-in)
 * command, or null.  Tolerates trailing context like "from others". */
function norelay_command(input) {
    var s = String(input || "").toLowerCase()
                .replace(/^\s+|\s+$/g, "").replace(/[.!,]+$/, "");
    if (/^(relay me|allow relays?( to me)?|you (can|may) relay( to me)?|resume relays?|relays? (on|ok)( for me)?)$/.test(s))
        return "relay-ok";
    if (/^(please\s+)?(don'?t|do\s+not|never|no|stop)\s+(relay\w*|pass\w*)\b/.test(s)
        && /\bme\b/.test(s))
        return "norelay";
    return null;
}

function format_age(secs) {
    if (secs < 60) return Math.floor(secs) + "s";
    if (secs < 3600) return Math.floor(secs / 60) + "m";
    if (secs < 86400) return Math.floor(secs / 3600) + "h";
    return Math.floor(secs / 86400) + "d";
}
function deliver_pending(channel, speaker_nick) {
    var state = load_relay();
    if (!state.messages.length) return;
    var lower = String(speaker_nick).toLowerCase();
    var remaining = [];
    var deliver   = [];
    for (var i = 0; i < state.messages.length; i++) {
        var m = state.messages[i];
        if (String(m.recipient || "").toLowerCase() === lower)
            deliver.push(m);
        else
            remaining.push(m);
    }
    if (!deliver.length) return;
    state.messages = remaining;
    save_relay(state);
    /* If the recipient has since opted out of relays, drop the queued
     * messages (already dequeued above) rather than delivering them. */
    if (is_norelay(speaker_nick)) return;
    var now = time();
    for (var i = 0; i < deliver.length; i++) {
        var m = deliver[i];
        var age = format_age(now - (m.ts || now));
        /* Quote the relayed body so it's unambiguous which part is the
         * sender's verbatim words vs. the bot's framing. */
        irc_say(channel, speaker_nick + ": " + (m.from || "someone")
                + " left you a message " + age + " ago: \"" + m.text + "\"");
    }
}
function log_chat_turn(speaker_nick, channel, input, reply, profile) {
    var f = new File(IRC_CHAT_LOG);
    if (!f.open("a")) return;
    /* system.timestr() matches the date format used in guru.log so the
     * two files can be eyeballed side-by-side. */
    f.write(system.timestr() + "\r\n");
    f.write("[IRC " + channel + "] " + speaker_nick + ":\r\n");
    f.write(String(input || "") + "\r\n");
    f.write((cfg.bot_name || "The Guru") + ":\r\n");
    f.write(String(reply || "") + "\r\n");
    if (profile) f.write("[prof] " + profile + "\r\n");
    f.close();
}

/* ---- State ---- */

var sock        = null;
var connected   = false;
var registered  = false;
var joined      = false;
var our_nick    = IRC_NICK;

/* One-shot announce: read the file at startup (so the bot still
 * has the content even if a crash/restart deletes the file later);
 * post per-channel on JOIN; delete the file after the first POST
 * so subsequent reconnects within this session DON'T re-announce.
 * announced_channels{} tracks which we've already posted to in
 * THIS process so multi-channel JOINs don't double-post. */
var pending_announce = (function () {
    if (!file_exists(ANNOUNCE_FILE)) return null;
    var f = new File(ANNOUNCE_FILE);
    if (!f.open('r')) return null;
    var raw = f.read();
    f.close();
    if (!raw) return null;
    var lines = String(raw).replace(/\r/g, '').split('\n');
    var out = [];
    for (var i = 0; i < lines.length; i++) {
        var ln = lines[i].replace(/^\s+|\s+$/g, '');
        if (ln) out.push(ln);
    }
    return out.length ? out : null;
}());
var announced_channels = {};
var announce_file_removed = false;

/* mtime of chat_llm.ini at startup -- we poll for changes in the
 * main loop and reload bot-glue knobs (aliases, intervention timings,
 * etc.) if the file is touched.  chat_session inside chat_llm.js
 * already reloads per turn, so model/temperature/prompt changes
 * pick up automatically; this handles the bot-side knobs that we
 * cache in module-level vars. */
var CONFIG_PATH = system.ctrl_dir + "chat_llm.ini";
var config_mtime = (function () {
    var f = new File(CONFIG_PATH);
    return f.date || 0;
}());

function reload_config_if_changed() {
    var f = new File(CONFIG_PATH);
    var t = f.date || 0;
    if (t === config_mtime) return;
    config_mtime = t;
    try {
        cfg = load_config(PERSONA);
    } catch (e) {
        log(LOG_WARNING, "chat_llm_irc: config reload failed: " + e);
        return;
    }
    INTERVENTION_WAIT      = parseInt(cfg.irc_intervention_wait, 10) || 120;
    INTERVENTION_COOLDOWN  = parseInt(cfg.irc_intervention_cooldown, 10) || 900;
    INTERVENTION_MIN_SCORE = parseFloat(cfg.irc_intervention_min_score_per_token) || 8.0;
    INTERVENTION_MIN_TOKENS = parseInt(cfg.irc_intervention_min_tokens, 10) || 2;
    ACK_ENABLED            = !/^(?:false|0|no|off)$/i.test(String(cfg.irc_ack_enabled));
    ACK_WINDOW             = parseInt(cfg.irc_ack_window, 10) || 90;
    ACK_COOLDOWN           = parseInt(cfg.irc_ack_cooldown, 10) || 90;
    ACK_CLOSER_CHANCE      = (cfg.irc_ack_closer_chance !== undefined
                             && cfg.irc_ack_closer_chance !== '')
                            ? parseFloat(cfg.irc_ack_closer_chance) : 0.9;
    STATIC_ALIASES         = build_aliases(cfg.irc_aliases);
    /* irc_nick / irc_network / irc_channels changes require a
     * reconnect to take effect -- we DON'T auto-reconnect on them
     * because that would disrupt the active session.  Sysop should
     * recycle the service if they want to change these. */
    log(LOG_INFO, "chat_llm_irc: reloaded config (aliases="
        + JSON.stringify(STATIC_ALIASES)
        + " wait=" + INTERVENTION_WAIT
        + " cooldown=" + INTERVENTION_COOLDOWN
        + " min_score=" + INTERVENTION_MIN_SCORE
        + " min_tokens=" + INTERVENTION_MIN_TOKENS + ")");
}

/* Open-question tracking.  Currently single-channel (we only joined
 * one); generalizing to multi-channel is a key:channel hash. */
/* Per-channel state.  channel_name (lowercased) -> {
 *   open_question: { text, nick, when } | null,
 *   last_intervention: system.timer value or 0,
 *   last_bot: { nick, when, kind, text } | null,  // bot's most recent line;
 *             kind 'reply'|'intervention' is substantive (ack-eligible),
 *             set when the bot speaks to a user; nulled the moment any
 *             user line is handled (so an ack only fires for a direct,
 *             immediate reply with nobody in between).
 *   last_ack: system.timer of the last acknowledgement (cooldown)
 * }
 * Independent per channel so a cooldown in #channel1 doesn't gag
 * the bot in #channel2. */
var channel_state = {};
function chan_state(name) {
    var k = String(name).toLowerCase();
    if (!channel_state[k])
        channel_state[k] = { open_question: null, last_intervention: 0,
                             last_bot: null, last_ack: 0 };
    return channel_state[k];
}

/* ---- IRC line I/O ---- */

function irc_send(line) {
    if (!sock || !connected) return false;
    return !!sock.send(line + "\r\n");
}

/* Split a reply for sending across multiple PRIVMSG lines without
 * cutting words / mid-sentence.  Preference order at each cut:
 *   1. End of a sentence (. ? !) within the budget
 *   2. End of a word (last whitespace) within the budget
 *   3. Hard cut at the budget (last resort)
 *
 * Each returned piece is whitespace-trimmed and <= max bytes.  The
 * "don't cut too early" thresholds (idx < max/2 for sentences,
 * idx < max/4 for words) keep us from emitting a one-word first
 * line just because there's an early period -- if no decent split
 * point exists we'd rather pack the line densely. */
function split_for_irc(text, max) {
    var parts = [];
    while (text.length > max) {
        var slice = text.substring(0, max);
        var idx = -1;
        var sm = slice.match(/[.?!](?=\s)(?!\s*$)/g);  /* count via match */
        /* lastIndexOf walks for each terminator */
        var s1 = slice.lastIndexOf(". ");
        var s2 = slice.lastIndexOf("? ");
        var s3 = slice.lastIndexOf("! ");
        idx = Math.max(s1, s2, s3);
        if (idx >= 0 && idx >= max / 2) {
            idx += 1;  /* include the terminator, leave the space */
        } else {
            idx = slice.lastIndexOf(" ");
            if (idx < max / 4) idx = max;  /* hard cut */
        }
        parts.push(text.substring(0, idx).replace(/^\s+|\s+$/g, ""));
        text = text.substring(idx).replace(/^\s+/, "");
    }
    if (text.length) parts.push(text.replace(/^\s+|\s+$/g, ""));
    return parts;
}

function irc_say(target, text) {
    text = String(text).replace(/[\r\n]+/g, " ").replace(/\s+/g, " ");
    text = text.replace(/^\s+|\s+$/g, "");
    if (!text) return;
    var pieces = split_for_irc(text, MAX_REPLY_BYTES);
    for (var pi = 0; pi < pieces.length; pi++) {
        if (pieces[pi]) irc_send("PRIVMSG " + target + " :" + pieces[pi]);
    }
    /* Record the bot's own channel line into the rolling buffer so
     * @channel_context@ includes it on subsequent dispatches.  This
     * is what gives the bot recall of "what did I just announce /
     * say earlier".  Only for channel targets -- DMs and one-shot
     * server commands (NickServ IDENTIFY etc.) shouldn't pollute
     * channel context. */
    if (target && (target.charAt(0) === "#" || target.charAt(0) === "&")) {
        record_channel_msg(target, our_nick, text);
    }
}

function parse_irc_line(line) {
    var prefix = "";
    if (line.charAt(0) === ":") {
        var sp = line.indexOf(" ");
        if (sp < 0) return null;
        prefix = line.slice(1, sp);
        line = line.slice(sp + 1);
    }
    var parts = line.split(" ");
    if (!parts.length) return null;
    var cmd = parts[0].toUpperCase();
    var args = [];
    for (var i = 1; i < parts.length; i++) {
        if (parts[i].charAt(0) === ":") {
            args.push(parts.slice(i).join(" ").slice(1));
            break;
        }
        args.push(parts[i]);
    }
    return { prefix: prefix, cmd: cmd, args: args };
}

function nick_from_prefix(prefix) {
    var bang = prefix.indexOf("!");
    return bang >= 0 ? prefix.slice(0, bang) : prefix;
}

/* ---- chat_llm dispatch ---- */

function build_irc_ctx(speaker_nick, channel) {
    return {
        persona: {
            code: PERSONA,
            name: (cfg.bot_name || "The Guru")
        },
        speaker: {
            /* IRC nicks are case-insensitive per RFC 1459 ("Frosty"
             * and "frosty" are the same user), so lowercase the nick
             * for the speaker id / memory filename to avoid splitting
             * one user's history across multiple files.  Display name
             * (alias) keeps the original case as the user typed it. */
            id:    "irc:" + IRC_HOST + "/" + speaker_nick.toLowerCase(),
            alias: speaker_nick,
            attrs: {}
        },
        participants: [],
        transcript:   [],
        mode:         "irc",
        /* Relay queue + opt-out paths, so the relay_message tool uses the
         * SAME persona-derived files this bot reads/writes -- no hardcoded
         * "guru_irc" base in the tool. */
        relay_path:    IRC_RELAY_PATH,
        norelay_path:  IRC_NORELAY_PATH,
        supports_utf8: true,
        addressed:    true,
        /* IRC has no terminal -- typing simulation makes no sense.
         * Set speed_factor=0 (skip per-char animation) and disable
         * typo simulation.  chat_llm.js's streaming-mode emit goes
         * through console.simulate_type which isn't bound here
         * anyway, so we'll naturally fall to non-streaming. */
        typing_speed_factor: 0,
        simulate_typos:      false,
        /* Recent channel chatter (last ~30 lines, all speakers) for
         * cross-conversation awareness.  Picked up by chat_llm.js's
         * @channel_context@ macro and wrapped in <channel_room> tags. */
        channel_context:     format_channel_context(channel),
        /* Current channel + its NAMES list, so llm_tools.js's
         * relay_message can validate the recipient against who's
         * actually in the channel.  channel_members is a name-keyed
         * map: lowercased-nick -> canonical-case. */
        channel:             channel,
        channel_members:     chan_members(channel),
        /* Per-channel ever-seen nicks -- relay_message uses this
         * to accept "tell <X> when you see them" relays for nicks
         * the bot has met before but who aren't currently online. */
        seen_members:        chan_seen(channel)
    };
}

function dispatch_chat(speaker_nick, text, channel, opts) {
    var ctx = build_irc_ctx(speaker_nick, channel);
    /* Unprompted interventions ask the engine to abstain (reply SKIP ->
     * null) unless it's confident -- see chat_llm.js build_messages. */
    if (opts && opts.volunteering) ctx.volunteering = true;
    try {
        var reply = chat_session(text, ctx);
        /* Best-effort persist: failures to write the log shouldn't
         * break chat.  ctx._profile is set by chat_session(); read it
         * AFTER the call.  Log an abstained intervention explicitly so
         * the decision is visible in the chat log (the engine returns
         * null and didn't persist it to memory). */
        try {
            var logged = (reply === null && ctx._abstained)
                ? "[abstained -- not confident enough to volunteer]" : reply;
            log_chat_turn(speaker_nick, channel, text, logged, ctx._profile);
        } catch (le) { /* ignore -- chat itself succeeded */ }
        return reply;
    } catch (e) {
        log(LOG_ERR, "chat_llm_irc: chat_session error for "
            + speaker_nick + ": " + e);
        return null;
    }
}

/* ---- Channel-wide rolling context ---- */

/* In-memory rolling buffer of the last N lines per channel, ALL
 * speakers (not just bot-addressed lines).  Lets the bot follow
 * cross-conversations: when Alice asks a follow-up that depends
 * on something Bob just said, the room context is available.
 * Per-channel keyed by lowercase channel name.  Each entry is
 * { nick, text, ts }. */
var channel_buffer = {};

/* Channel NAMES list -- lowercased nick -> canonical case.  Populated
 * from the IRC 353 RPL_NAMREPLY response after JOIN and maintained
 * thereafter on JOIN / PART / QUIT / NICK / KICK.  Used by the
 * relay_message tool's recipient-validation pass so the bot can refuse
 * to queue messages for nicks that aren't actually on the channel
 * (and suggest close matches when it's a typo).  Keyed by lowercase
 * channel name. */
var channel_members = {};
function chan_members(name) {
    var k = String(name || "").toLowerCase();
    if (!channel_members[k]) channel_members[k] = {};
    return channel_members[k];
}
function add_member(channel, nick) {
    if (!channel || !nick) return;
    /* Strip leading IRC mode prefixes (@op, +voice, %halfop, etc.). */
    var clean = String(nick).replace(/^[@+%&~!]/, "");
    if (!clean) return;
    chan_members(channel)[clean.toLowerCase()] = clean;
    /* Also remember them in the per-channel ever-seen set so
     * relay_message can resolve "tell <X> when you see them" for
     * nicks that have been around before but aren't connected now. */
    add_seen_member(channel, clean);
}

/* ---- Ever-seen nicks per channel (persistent) ----
 * Accumulates every nick we've ever observed in each channel (via
 * NAMES, JOIN, NICK, or PRIVMSG).  Persisted to disk so it survives
 * bot restarts.  Used as a 4th resolver source by relay_message --
 * lets the bot accept a "tell funbot hi when you see them" relay
 * for funbot even when funbot isn't currently connected, as long as
 * the bot has seen funbot here at least once before. */
var SEEN_FILE = PERSONA_DIR + PERSONA_PROTO + "_seen.json";
var seen_members = {};   /* { channel_lower: { nick_lower: canonical } } */
var seen_dirty = false;
function chan_seen(name) {
    var k = String(name || "").toLowerCase();
    if (!seen_members[k]) seen_members[k] = {};
    return seen_members[k];
}
function add_seen_member(channel, nick) {
    if (!channel || !nick) return false;
    var clean = String(nick).replace(/^[@+%&~!]/, "");
    if (!clean) return false;
    var nk = clean.toLowerCase();
    var s = chan_seen(channel);
    if (s[nk]) return false;
    s[nk] = clean;
    seen_dirty = true;
    return true;
}
function load_seen_members() {
    try {
        var f = new File(SEEN_FILE);
        if (!f.open("r")) return;
        var txt = f.read();
        f.close();
        var data = JSON.parse(txt);
        if (data && typeof data === "object") seen_members = data;
        blog("loaded ever-seen nicks for "
             + Object.keys(seen_members).length + " channels");
    } catch (e) {
        log(LOG_WARNING, "load_seen_members: " + e);
    }
}
function save_seen_members() {
    if (!seen_dirty) return;
    try {
        var tmp = SEEN_FILE + ".tmp";
        var f = new File(tmp);
        if (!f.open("w")) throw new Error("cannot open " + tmp);
        f.write(JSON.stringify(seen_members));
        f.close();
        if (file_exists(SEEN_FILE)) file_remove(SEEN_FILE);
        file_rename(tmp, SEEN_FILE);
        seen_dirty = false;
    } catch (e) {
        log(LOG_WARNING, "save_seen_members: " + e);
    }
}
function remove_member(channel, nick) {
    if (!channel || !nick) return;
    delete chan_members(channel)[String(nick).toLowerCase()];
}
function remove_member_everywhere(nick) {
    if (!nick) return;
    var lower = String(nick).toLowerCase();
    for (var k in channel_members) delete channel_members[k][lower];
}
function rename_member_everywhere(old_nick, new_nick) {
    if (!old_nick || !new_nick) return;
    var ol = String(old_nick).toLowerCase();
    for (var k in channel_members) {
        if (channel_members[k][ol] !== undefined) {
            delete channel_members[k][ol];
            channel_members[k][String(new_nick).toLowerCase()] = new_nick;
            /* Remember BOTH names in seen -- the original and the
             * new -- so a relay sent to either resolves. */
            add_seen_member(k, old_nick);
            add_seen_member(k, new_nick);
        }
    }
}


/* Cap on history we keep per channel.  Tune for prompt size: ~30
 * lines * ~80 chars = ~2400 bytes of channel context max.  More
 * than enough for follow-up awareness without blowing TTFC. */
var CHANNEL_BUFFER_LINES = 30;
var CHANNEL_BUFFER_MAX_AGE_SECS = 3600;  /* drop entries > 1h old */

function chan_buf(channel) {
    var k = (channel || "").toLowerCase();
    if (!channel_buffer[k]) channel_buffer[k] = [];
    return channel_buffer[k];
}

function record_channel_msg(channel, nick, text) {
    var buf = chan_buf(channel);
    buf.push({ nick: nick, text: text, ts: time() });
    /* Evict oldest if over the line cap. */
    while (buf.length > CHANNEL_BUFFER_LINES) buf.shift();
    /* Evict stale entries by age (the cap above is the upper bound;
     * this drops idle-channel staleness even if the line count's
     * fine). */
    var cutoff = time() - CHANNEL_BUFFER_MAX_AGE_SECS;
    while (buf.length && buf[0].ts < cutoff) buf.shift();
}

/* Format the channel's recent buffer as "<nick>: text\n..." for
 * chat_llm's @channel_context@ macro.  Excludes the current
 * caller's just-spoken line (it's already the user input).  Skips
 * our own bot lines too if any leaked in (we DON'T record our
 * replies into the buffer below). */
function format_channel_context(channel) {
    if (!channel) return '';
    var buf = chan_buf(channel);
    if (!buf.length) return '';
    var lines = [];
    /* Drop the last (most recent) entry -- it's the message the
     * bot is responding to right now, already in the user input.
     * Keep everything before it as context. */
    for (var i = 0; i < buf.length - 1; i++) {
        lines.push('<' + buf[i].nick + '> ' + buf[i].text);
    }
    return lines.join('\n');
}

/* ---- Confidence test for unprompted intervention ---- */

function is_high_confidence(question) {
    var idx = load_index(PERSONA, cfg);
    if (!idx) return false;
    var tokens = tokenize_query(question);
    /* A single content word can't carry a high-confidence score: the
     * per-token average below is then just the raw BM25 hit with no
     * averaging, so a lone high-salience term (a name like "Deuce", a
     * corpus keyword like "binkp") trivially clears the floor.  Require
     * a minimum number of content tokens before we'll volunteer. */
    if (tokens.length < INTERVENTION_MIN_TOKENS) return false;
    var hits = bm25_search(idx, tokens, 1, cfg.index_source_weights);
    if (!hits.length) return false;
    var per_token = hits[0].score / tokens.length;
    return per_token >= INTERVENTION_MIN_SCORE;
}

/* ---- Address detection ---- */

/* Uses chat_llm.js's is_addressed(line, names) -- word-boundary
 * case-insensitive match against any of the names.  We pass current
 * nick + persona-derived defaults + sysop's irc_aliases. */
function bot_addressed_in(text) {
    return is_addressed(text, current_address_names());
}

/* Strip any of our address-names (with optional trailing :/,) so the
 * model doesn't see "<bot>, what is X?" as the question -- just
 * "what is X?". */
function strip_address(text) {
    var names = current_address_names();
    var t = text;
    for (var i = 0; i < names.length; i++) {
        /* Match the name with optional trailing ":", "," or ";" and
         * any following whitespace.  The trailing \b is GONE on
         * purpose: "VERT_guru: text" -- ":" then space has no word
         * boundary, which made the prior regex backtrack [:,]? to
         * empty and leave a stray ":" in the stripped text.  That
         * stray colon broke the classify_intent regex anchors on
         * every "<bot>:" style address.  IRC nicks are distinctive
         * (contain "_", digits, etc.) so no English word collides. */
        var rx = new RegExp("\\b"
            + names[i].replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
            + "[:,;]?\\s*", "gi");
        t = t.replace(rx, "");
    }
    /* Clean up space-before-punctuation introduced by mid-sentence
     * address removal: "hi VERT_guru!" -> "hi !" -> "hi!".  Only
     * touches sentence-terminator punctuation; other separators
     * (commas, em-dashes) we leave alone to avoid changing meaning. */
    t = t.replace(/\s+([.!?])/g, "$1");
    return t.replace(/^\s+|\s+$/g, "");
}

/* True when `text` is essentially just a ping at another channel
 * participant -- a bare nick optionally followed by '?' or address
 * punctuation ("Deuce ?", "Deuce:", "nelgin?") -- rather than a
 * question for the room.  These must NOT be recorded as open
 * questions: the user is trying to get a specific person's attention,
 * not asking the bot (or anyone) something it could answer.  Only
 * fires when the lone remaining word names someone the bot has seen
 * on this channel (current member or ever-seen) and isn't one of the
 * bot's own address-names. */
function is_nick_ping(channel, text) {
    var s = String(text || "").replace(/^\s+|\s+$/g, "");
    /* Drop a trailing '?' (and any space before it) plus any trailing
     * address punctuation, then require what's left to be a single
     * bare word. */
    s = s.replace(/\s*\?+\s*$/, "").replace(/[:,;]+\s*$/, "")
         .replace(/^\s+|\s+$/g, "");
    if (!s || /\s/.test(s)) return false;   /* empty or multi-word */
    var lc = s.toLowerCase();
    /* An address to the bot itself is not a ping at someone else. */
    var names = current_address_names();
    for (var i = 0; i < names.length; i++)
        if (names[i].toLowerCase() === lc) return false;
    /* Must name someone the bot has actually seen in this channel. */
    return !!chan_members(channel)[lc] || !!chan_seen(channel)[lc];
}

/* True when `text` is a message directed at a specific OTHER channel
 * participant by a leading "Nick:" or "Nick," address that carries
 * actual content after it ("nelgin: why are you changing ip?",
 * "Dan_C: you around?").  This is the conversational counterpart to
 * is_nick_ping (which only catches a BARE "Nick?"): a person-to-person
 * exchange the bot must NOT hijack.  In particular it must not arm an
 * unprompted intervention on a question one user explicitly aimed at
 * another -- volunteering an answer there (or worse, answering as if it
 * were the addressee) is exactly the "too eager / unsolicited" failure.
 * The named participant must be someone the bot has actually seen on
 * this channel and must NOT be one of the bot's own address-names (a
 * line aimed at the bot is handled by the bot-addressed path instead). */
function directed_at_other(channel, text) {
    var m = /^\s*([^\s:,]+)[:,]\s+\S/.exec(String(text || ""));
    if (!m) return false;
    var lc = m[1].toLowerCase();
    var names = current_address_names();
    for (var i = 0; i < names.length; i++)
        if (names[i].toLowerCase() === lc) return false;
    return !!chan_members(channel)[lc] || !!chan_seen(channel)[lc];
}

/* ---- Message handling ---- */

/* Per-nick redirect state -- one auto-reply per DM-er per session
 * so we don't spam them on every line they send. */
var dm_redirected = {};

function handle_privmsg(from_nick, target, text) {
    if (from_nick.toLowerCase() === our_nick.toLowerCase()) return;

    /* DM to the bot's nick -- not a channel message.  Respond once
     * with a polite redirect to the channel, then ignore further DMs
     * from this nick.  Skip system / service replies (NickServ etc.)
     * by only matching DMs addressed to OUR nick. */
    if (target.charAt(0) !== "#" && target.charAt(0) !== "&") {
        blog("non-channel PRIVMSG from=" + from_nick
            + " target=" + target
            + " (our_nick=" + our_nick + ")");
        if (target.toLowerCase() !== our_nick.toLowerCase()) return;
        var key = from_nick.toLowerCase();
        if (dm_redirected[key]) {
            blog("DM from " + from_nick
                + " -- already redirected this session, ignoring");
            return;
        }
        dm_redirected[key] = true;
        blog("DM from " + from_nick + " -- sending channel redirect");
        irc_say(from_nick, "Hi! I only chat in " + IRC_CHANNELS.join(" / ")
            + " -- come join us there.");
        return;
    }

    if (!is_our_channel(target)) return;
    var st = chan_state(target);

    /* Anyone speaking in our channel is "seen" -- remember them so
     * relay_message can target them later even after they leave. */
    add_seen_member(target, from_nick);

    /* Record EVERY channel line (addressed or not) into the per-
     * channel rolling buffer.  This is what gives the bot
     * cross-conversation awareness: when Bob asks a follow-up that
     * depends on what Alice just said, the prior lines are there
     * via @channel_context@.  Skip our own replies (they're
     * irc_say'd directly without round-tripping through here, so
     * this just sees other speakers). */
    record_channel_msg(target, from_nick, text);

    /* Deliver any pending relay messages addressed to this speaker.
     * Channel-agnostic: a message queued in #synchronet for "Bob"
     * delivers the next time Bob speaks anywhere the bot's joined (the
     * JOIN handler delivers on entry too; whichever happens first wins).
     * Fires BEFORE address detection so the delivery isn't held
     * back waiting for the recipient to address the bot. */
    deliver_pending(target, from_nick);

    if (bot_addressed_in(text)) {
        st.open_question = null;
        var input = strip_address(text);
        if (!input) input = text;
        /* Mute/unmute commands are handled regardless of current mute
         * state, so a muted user can always bring the bot back. */
        var mc = mute_command(input);
        if (mc === "mute") {
            set_mute(from_nick, true);
            irc_say(target, from_nick
                + ": ok, i'll pipe down -- say \"unmute me\" when you want me back.");
            return;
        }
        if (mc === "unmute") {
            set_mute(from_nick, false);
            irc_say(target, from_nick + ": back atcha -- what's up?");
            return;
        }
        /* Relay opt-out, also handled regardless of mute state. */
        var rc = norelay_command(input);
        if (rc === "norelay") {
            set_norelay(from_nick, true);
            irc_say(target, from_nick + ": got it -- i won't relay anyone's "
                + "messages to you. say \"relay me\" to undo.");
            return;
        }
        if (rc === "relay-ok") {
            set_norelay(from_nick, false);
            irc_say(target, from_nick + ": ok, i'll relay messages to you again.");
            return;
        }
        if (is_muted(from_nick)) return;   /* muted: stay silent */
        var reply = dispatch_chat(from_nick, input, target);
        if (reply) {
            irc_say(target, from_nick + ": " + reply);
            /* Remember our line so a brief reply to it can be acknowledged. */
            st.last_bot = { nick: from_nick, when: system.timer,
                            kind: "reply", text: reply };
        }
        return;
    }

    /* --- Acknowledgement: a brief, clear reply to the bot's OWN last line
     * (a social closer like "ah ok / thanks", or a correction).  Read and
     * CONSUME last_bot first -- any user line means the bot's prior line is
     * no longer the immediately-preceding one, which also enforces "nobody
     * spoke in between" and "never two acks in a row" (an ack doesn't set
     * last_bot, so the next line can't ack again -- the user gets the last
     * word). */
    var lb = st.last_bot;
    st.last_bot = null;

    /* A "go away / stop talking to me" line right after the bot spoke
     * TO this user -- its last line was a reply/intervention directed at
     * them, still within the ack window -- is almost certainly aimed at
     * the bot even though it didn't name it.  Honor it as a mute: this
     * is the unaddressed counterpart to the "mute me" handling in the
     * addressed branch above (where mute_command is only consulted when
     * the bot is explicitly addressed).  Muting also drops any pending
     * open_question from this user so check_intervention won't volunteer
     * an answer to them either. */
    if (lb && lb.nick === from_nick
        && (lb.kind === "reply" || lb.kind === "intervention")
        && (system.timer - lb.when) <= ACK_WINDOW
        && mute_command(text) === "mute") {
        set_mute(from_nick, true);
        if (st.open_question && st.open_question.nick === from_nick)
            st.open_question = null;
        blog("auto-mute (unaddressed go-away) nick=" + from_nick
             + " text=" + JSON.stringify(text));
        return;
    }

    if (ACK_ENABLED && lb && lb.nick === from_nick
        && (lb.kind === "reply" || lb.kind === "intervention")
        && (system.timer - lb.when) <= ACK_WINDOW
        && (system.timer - st.last_ack) >= ACK_COOLDOWN
        && !is_muted(from_nick)
        && !/\?\s*$/.test(text)) {
        var akind = ack_kind(text);
        /* If OUR last line was a question to them and they just answered it
         * (a short reply that isn't a closer/correction -- the user's line
         * is already known not to be a question), react too: ignoring an
         * answer to a question we asked is the rude case. */
        if (!akind && /\?\s*$/.test(String(lb.text || ""))
            && text.replace(/^\s+|\s+$/g, "").length <= 80)
            akind = "answer";
        /* Closers are probabilistic; corrections and answers always react. */
        if (akind && (akind !== "closer" || Math.random() < ACK_CLOSER_CHANCE)) {
            var aline = chat_llm_ack(cfg, lb.text, text, akind);
            if (aline) {
                irc_say(target, aline);   /* bare -- no "nick: " prefix */
                st.last_ack = system.timer;
            }
            return;
        }
    }

    /* Track open question.  A new question replaces any previous
     * unanswered one in THIS channel (latest unanswered wins). */
    if (/\?\s*$/.test(text)) {
        /* A bare ping at another participant ("Deuce ?") is not a
         * question for the room -- don't arm an intervention on it.
         * Leave any existing open_question untouched. */
        if (is_nick_ping(target, text)) return;
        /* A question explicitly addressed to another participant
         * ("nelgin: why ... ?") is theirs to answer, not a room
         * question -- never barge into a person-to-person exchange. */
        if (directed_at_other(target, text)) return;
        st.open_question = {
            text: text,
            nick: from_nick,
            when: system.timer
        };
        return;
    }

    /* Anyone else's non-question message clears the open question
     * for this channel (someone's already responded).  The
     * questioner's own follow-up is allowed -- they may be
     * elaborating. */
    if (st.open_question && from_nick !== st.open_question.nick)
        st.open_question = null;
}

function check_intervention() {
    /* Per-channel intervention check.  Iterate joined channels and
     * fire independently in each -- cooldown in one doesn't gag
     * the others. */
    for (var i = 0; i < IRC_CHANNELS.length; i++) {
        var chan = IRC_CHANNELS[i];
        var st = chan_state(chan);
        if (!st.open_question) continue;
        /* Don't volunteer answers to a muted user's questions. */
        if (is_muted(st.open_question.nick)) { st.open_question = null; continue; }
        var age = system.timer - st.open_question.when;
        if (age < INTERVENTION_WAIT) continue;

        var since_last = system.timer - st.last_intervention;
        if (st.last_intervention && since_last < INTERVENTION_COOLDOWN) {
            /* In cooldown -- drop the question, don't keep checking. */
            st.open_question = null;
            continue;
        }

        if (!is_high_confidence(st.open_question.text)) {
            /* Not confident.  Let it expire. */
            st.open_question = null;
            continue;
        }

        /* Chime in -- but only if the engine is confident.  The
         * volunteering flag makes chat_llm.js abstain (return null)
         * rather than emit a guessy, possibly-wrong unsolicited answer. */
        var reply = dispatch_chat(st.open_question.nick,
                                  st.open_question.text,
                                  chan,
                                  { volunteering: true });
        if (reply) {
            irc_say(chan, st.open_question.nick + ": " + reply);
            /* An intervention is a substantive line too -- let the asker's
             * brief reply to it be acknowledged. */
            st.last_bot = { nick: st.open_question.nick, when: system.timer,
                            kind: "intervention", text: reply };
        }
        st.last_intervention = system.timer;
        st.open_question = null;
    }
}

/* Post the one-shot announce to a channel we just joined.  Each
 * non-empty line in pending_announce becomes a separate channel
 * message.  After the first channel gets the message we remove
 * the announce file (so we don't re-announce on subsequent
 * reconnects within the same restart) but keep the in-memory
 * copy so the OTHER configured channels get it too on their
 * own JOINs.  announced_channels{} dedupes within this session. */
function post_announce(channel) {
    if (!pending_announce || !pending_announce.length) return;
    var key = (channel || "").toLowerCase();
    if (!key || announced_channels[key]) return;
    announced_channels[key] = true;
    for (var i = 0; i < pending_announce.length; i++) {
        irc_say(channel, pending_announce[i]);
    }
    if (!announce_file_removed) {
        try { file_remove(ANNOUNCE_FILE); } catch (e) {}
        announce_file_removed = true;
        blog("posted announce to " + channel
             + " and removed " + ANNOUNCE_FILE);
    } else {
        blog("posted announce to " + channel + " (file already removed)");
    }
}

/* ---- Connection / registration ---- */

function dispatch_line(parsed) {
    switch (parsed.cmd) {
    case "PING":
        irc_send("PONG :" + (parsed.args[0] || ""));
        break;
    case "001":  /* RPL_WELCOME -- we're registered */
        registered = true;
        if (IRC_NICKSERV_PASSWORD)
            irc_send("PRIVMSG NickServ :IDENTIFY " + IRC_NICKSERV_PASSWORD);
        /* JOIN all configured channels.  IRC allows comma-separated
         * channel lists in a single JOIN command. */
        irc_send("JOIN " + IRC_CHANNELS.join(","));
        break;
    case "JOIN":
        var joiner = nick_from_prefix(parsed.prefix);
        var jchan  = parsed.args[0] || "?";
        if (joiner.toLowerCase() === our_nick.toLowerCase()) {
            /* parsed.args[0] is the channel we just joined.
             * We're "joined" once we've gotten any JOIN confirmation;
             * intervention checks etc. don't need ALL channels to
             * be joined simultaneously to run. */
            joined = true;
            blog("joined " + jchan + " as " + our_nick);
            /* Reset member list -- a 353 RPL_NAMREPLY follows JOIN
             * with the full roster, which will repopulate.  Stale
             * entries from a previous join cycle would be wrong. */
            channel_members[jchan.toLowerCase()] = {};
            add_member(jchan, our_nick);
            post_announce(jchan);
        } else {
            add_member(jchan, joiner);
            /* A relay request is phrased "next time you SEE X" -- seeing
             * X re-join the channel satisfies that, so drain any pending
             * relays for them now rather than waiting for them to speak.
             * deliver_pending() drains the queue, so this is idempotent
             * with the speak-triggered delivery in handle_privmsg(). */
            deliver_pending(jchan, joiner);
        }
        break;
    case "353":   /* RPL_NAMREPLY -- ":<server> 353 <us> = <chan> :<names>" */
        /* args = [our_nick, "=" or "*" or "@", channel, "<names>"] */
        var ch353 = parsed.args[2];
        var names = (parsed.args[3] || "").split(/\s+/);
        for (var ni = 0; ni < names.length; ni++) {
            if (names[ni]) add_member(ch353, names[ni]);
        }
        break;
    case "PART":
        remove_member(parsed.args[0],
                      nick_from_prefix(parsed.prefix));
        break;
    case "QUIT":
        remove_member_everywhere(nick_from_prefix(parsed.prefix));
        break;
    case "KICK":
        remove_member(parsed.args[0], parsed.args[1]);
        break;
    case "NICK":
        /* NICK new-nick is in args[0]; OLD nick is in the prefix. */
        rename_member_everywhere(nick_from_prefix(parsed.prefix),
                                 parsed.args[0]);
        break;
    case "432":  /* ERR_ERRONEUSNICKNAME -- nick contains invalid chars.
                  * Sanitize and retry.  Defensive: shouldn't fire
                  * since we sanitize at config time, but a server's
                  * acceptable-char set may differ. */
        var fixed = sanitize_irc_nick(our_nick).replace(/[^A-Za-z0-9]/g, "");
        if (fixed && fixed !== our_nick) {
            blog("server rejected nick " + our_nick
                + " as erroneous, retrying as " + fixed);
            our_nick = fixed;
            irc_send("NICK " + our_nick);
        }
        break;
    case "433":  /* ERR_NICKNAMEINUSE */
        our_nick = our_nick + "_";
        irc_send("NICK " + our_nick);
        break;
    case "PRIVMSG":
        handle_privmsg(nick_from_prefix(parsed.prefix),
                       parsed.args[0],
                       parsed.args[1] || "");
        break;
    /* ignored: NOTICE, MODE, other numeric replies */
    }
}

function connect() {
    sock = new Socket(SOCK_STREAM);
    if (!sock.connect(IRC_HOST, IRC_PORT)) {
        log(LOG_ERR, "chat_llm_irc: connect to "
            + IRC_HOST + ":" + IRC_PORT + " failed");
        sock = null;
        return false;
    }
    connected  = true;
    registered = false;
    joined     = false;
    irc_send("NICK " + our_nick);
    irc_send("USER " + our_nick + " 0 * :"
             + (cfg.bot_name || PERSONA) + " (Synchronet)");
    log(LOG_INFO, "chat_llm_irc: connected to "
        + IRC_HOST + ":" + IRC_PORT + ", registering as " + our_nick);
    return true;
}

/* ---- Main loop ---- */

/* Graceful shutdown -- register via js.on_exit so it runs when the
 * services subsystem (or sysop) terminates the script.  Sends QUIT
 * to the IRC server then closes the socket.  Socket.close() in
 * Synchronet is a graceful TCP close (FIN, kernel flushes queued
 * data) so the QUIT bytes from send() reach the server before the
 * connection tears down -- no explicit flush wait needed. */
function shutdown() {
    log(LOG_INFO, "chat_llm_irc: shutdown() called, sock="
        + (sock ? "live" : "null"));
    /* Final flush of ever-seen nicks so we don't lose the last
     * batch of additions accumulated since the previous tick. */
    try { save_seen_members(); } catch (se) {}
    if (!sock) return;
    try {
        var ok = sock.send("QUIT :guru signing off\r\n");
        log(LOG_INFO, "chat_llm_irc: QUIT send returned " + ok);
        sock.close();
        log(LOG_INFO, "chat_llm_irc: socket closed");
    } catch (e) {
        log(LOG_WARNING, "chat_llm_irc: shutdown error: " + e);
    }
    sock = null;
}
js.on_exit("shutdown()");

function main_loop() {
    var buffer = "";
    while (!js.terminated && !file_exists(STOP_SEMFILE)) {
        if (!sock || !connected || !sock.is_connected) {
            connected = false;
            if (sock) {
                try { sock.close(); } catch (e) {}
                sock = null;
            }
            if (!connect()) {
                mswait(RECONNECT_DELAY_SECONDS * 1000);
                continue;
            }
            buffer = "";
        }
        /* Poll with a short timeout (0.25s) so js.terminated is
         * noticed quickly on services recycle / shutdown -- otherwise
         * up to 1s of lag between termination signal and the loop
         * actually exiting and on_exit firing. */
        if (sock.poll(0.25, false)) {
            var data;
            try { data = sock.recv(4096); } catch (e) {
                log(LOG_WARNING, "chat_llm_irc: recv error: " + e);
                data = null;
            }
            if (!data) {
                log(LOG_WARNING, "chat_llm_irc: socket closed by peer");
                sock.close();
                sock = null;
                continue;
            }
            buffer += data;
            var nl;
            while ((nl = buffer.indexOf("\n")) >= 0) {
                var line = buffer.substring(0, nl).replace(/\r$/, "");
                buffer = buffer.substring(nl + 1);
                if (!line) continue;
                try {
                    var parsed = parse_irc_line(line);
                    if (parsed) dispatch_line(parsed);
                } catch (e) {
                    log(LOG_WARNING, "chat_llm_irc: dispatch error: " + e);
                }
            }
        }
        if (joined) {
            check_intervention();
            reload_config_if_changed();
            /* Persist any new ever-seen nicks accumulated since
             * the last flush.  Cheap no-op when nothing is dirty;
             * actual write happens at most once per tick when new
             * nicks come in (and is then quiescent). */
            save_seen_members();
        }
    }
    /* Loop exited cleanly.  Could be:
     *   a) STOP_SEMFILE appeared (sysop wants us to leave IRC) --
     *      remove the semfile so it doesn't trigger again on next
     *      restart, then graceful QUIT.
     *   b) js.terminated noticed (services subsystem starting
     *      shutdown) -- graceful QUIT before runtime tears us down.
     * Either way, shutdown() sends QUIT + closes the socket while
     * the JS context is still healthy. */
    if (file_exists(STOP_SEMFILE)) {
        try { file_remove(STOP_SEMFILE); } catch (e) {}
    }
    shutdown();
}

/* Skip the IRC connection + main loop when loaded for testing
 * (the loader sets CHAT_LLM_IRC_NO_MAIN=true first), so the adapter's
 * helpers can be unit-tested via jsexec without connecting a duplicate
 * bot.  Mirrors chat_llm.js's CHAT_LLM_NO_STANDALONE guard. */
if (typeof CHAT_LLM_IRC_NO_MAIN == 'undefined') {
    load_seen_members();
    main_loop();
}
