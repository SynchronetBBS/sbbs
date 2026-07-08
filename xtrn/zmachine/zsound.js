// zsound.js — @sound_effect player for the JSZM door (Z-machine sound support).
// Pure ES5; the door load()s this and wires JSZM_makeZSound()'s .sound as the
// engine hook (jszm.js case 245).
//
// Tier model (probed once, at creation, SyncTERM/cterm clients only):
//   2 digital — audio APCs + client libsndfile: sampled Blorb sounds (The
//               Lurking Horror, Sherlock) pass through as AIFF; bleeps are
//               Synth tones.
//   1 tone    — audio APCs without libsndfile: bleeps as Synth tones; sampled
//               sounds are silent.
//   0 bel     — everything else: bleeps map to ASCII BEL; samples silent.
//
// Sampled sounds come from the game's Blorb (<story>.blb, provisioned by
// getgames.js): each Snd resource is a complete Infocom AIFF file, uploaded to
// the terminal's persistent per-BBS cache once per session (cache name
// "zmachine/<story-base>/<num>") and re-decoded client-side on every play.
//
// Z-machine model (Z-Spec 9): only ONE sampled sound plays at a time, so
// samples own a single fixed channel, flushed before each Queue (voice
// stealing) and on the stop/finish effects. Bleeps use a second channel so a
// bleep doesn't cut off a playing sample. The @sound_effect completion routine
// is not supported (the terminal doesn't report playback end) — jszm.js
// documents that limitation at the opcode.

// Parse a Blorb's resource index for Snd entries. Returns { <num>: {off, len} }
// (len covers the full stored file: FORM/AIFF chunks include their own 8-byte
// chunk header — an AIFF file IS a FORM chunk — while OGGV/MOD payloads don't),
// or null when the file is missing/not a Blorb. Reads only the index, not the
// sound data; JSZM_makeZSound reads each sound lazily on first play.
function JSZM_zsBlorbSnds(path) {
  var f = new File(path);
  if (!f.open("rb")) return null;
  function rd(off, n) {
    f.position = off;
    var s = f.read(n);
    return (s != null && s.length === n) ? s : null;
  }
  function be32(s, o) {
    return ((s.charCodeAt(o) << 24) | (s.charCodeAt(o + 1) << 16) |
            (s.charCodeAt(o + 2) << 8) | s.charCodeAt(o + 3)) >>> 0;
  }
  var snds = null;
  var hdr = rd(0, 12);
  if (hdr && hdr.substr(0, 4) === "FORM" && hdr.substr(8, 4) === "IFRS") {
    var end = f.length, p = 12;
    while (p + 8 <= end) {                                  // walk top-level chunks for RIdx
      var ch = rd(p, 8);
      if (!ch) break;
      var cid = ch.substr(0, 4), clen = be32(ch, 4);
      if (cid === "RIdx") {
        var idx = rd(p + 8, clen);
        if (!idx || clen < 4) break;
        snds = {};
        var count = be32(idx, 0), i;
        for (i = 0; i < count && 4 + i * 12 + 12 <= clen; i++) {
          var e = 4 + i * 12;
          if (idx.substr(e, 4) !== "Snd ") continue;
          var num = be32(idx, e + 4), off = be32(idx, e + 8);
          var sh = rd(off, 8);                              // the resource's own chunk header
          if (!sh || off + 8 > end) continue;
          var slen = be32(sh, 4);
          if (off + 8 + slen > end) continue;               // rogue offset/length: skip
          if (sh.substr(0, 4) === "FORM")                   // AIFF: ship the whole chunk
            snds[num] = { off: off, len: 8 + slen };
          else                                              // OGGV/MOD: payload only
            snds[num] = { off: off + 8, len: slen };
        }
        break;
      }
      p += 8 + clen + (clen & 1);
    }
  }
  f.close();
  return snds;
}

// Build the player. opts:
//   storyPath — the story file; its ".blb" sibling supplies sampled sounds
//   tag       — client-cache prefix (the door's "zmachine/<storyId>"); sounds
//               upload as "<tag>/s<num>" ("s" keeps them clear of the picture
//               bridge's "<tag>/<pic>.ppm" entries in the same cache dir)
//   cterm     — a load({}, "cterm_lib.js") object (optional; loaded if absent)
// Returns { sound(number, effect, volume, repeats), tier, stop() }. `sound` is
// exception-safe: a playback error must never unwind into the interpreter.
function JSZM_makeZSound(opts) {
  var SND_CH = 2;                    // sampled-sound channel (exclusive, like Z-machine audio)
  var BLEEP_CH = 3;                  // bleeps mix on their own channel
  var MAX_REPEATS = 8;               // finite-repeat clamp (bounds queued APC bytes)
  var cterm = null, tier = 0;

  // Probe only cterm/SyncTERM clients: console.cterm_version is 0 for other
  // terminals under the BBS, and they must never be sent an audio APC/query.
  if (typeof console !== "undefined" && console.cterm_version) {
    cterm = opts.cterm || load({}, "cterm_lib.js");
    var caps = cterm.query_audio();                        // -1 none / 0 tones / 1 digital
    tier = caps < 0 ? 0 : (caps === 0 ? 1 : 2);
  }

  var blbPath = String(opts.storyPath || "").replace(/\.[zZ]\d$/, "") + ".blb";
  var tag = opts.tag || ("zmachine/" + file_getname(blbPath).replace(/\.blb$/, "")
                           .toLowerCase().replace(/[^a-z0-9]+/g, "_"));
  var snds = null, sndsParsed = false;                     // Blorb index, parsed on first sample
  var bytes = {};                                          // num -> AIFF bytes (read once)

  function sampleData(num) {
    if (!sndsParsed) { snds = JSZM_zsBlorbSnds(blbPath); sndsParsed = true; }
    if (!snds || !snds[num]) return null;
    if (bytes[num] === undefined) {
      var f = new File(blbPath), s = null;
      if (f.open("rb")) {
        f.position = snds[num].off;
        s = f.read(snds[num].len);
        f.close();
      }
      bytes[num] = (s != null && s.length === snds[num].len) ? s : null;
    }
    return bytes[num];
  }

  function volPct(v) {                                     // Z volume 1..8, 255 = loudest
    if (v <= 0 || v >= 255) return 100;
    if (v > 8) v = 8;
    return Math.round(v * 100 / 8);
  }

  function playSample(num, effect, volume, repeats) {
    if (tier < 2) return;
    var data = sampleData(num);
    if (data == null) return;
    var name = tag + "/s" + num;
    if (effect === 1) {                                    // prepare: upload now, start later is instant
      cterm.audio_prefetch(name, data);
      return;
    }
    cterm.audio_flush(SND_CH);                             // one sound at a time (voice stealing)
    var loop = (repeats === 255);
    var n = loop ? 1 : Math.min(Math.max(repeats, 1), MAX_REPEATS);
    for (var i = 0; i < n; i++)                            // FIFO append = consecutive repeats
      cterm.play_sound(name, data, { ch: SND_CH, vol: volPct(volume), loop: loop });
  }

  function bleep(num) {
    if (tier >= 1)                                         // Synth tone (no libsndfile needed)
      cterm.play_tone(num === 2 ? 330 : 880, num === 2 ? 200 : 120, { ch: BLEEP_CH });
    else if (typeof console !== "undefined")
      console.write("\x07");                               // ASCII BEL: works on any terminal
  }

  return {
    tier: tier,
    sound: function (number, effect, volume, repeats) {
      try {
        if (number <= 2) {                                 // 0 = stop-all (Z-Spec 1.1), 1/2 = bleeps
          if (number >= 1) { if (effect === 2) bleep(number); }
          else if (tier >= 1) cterm.audio_flush(SND_CH);
          return;
        }
        if (effect === 3 || effect === 4) {                // stop / finish-with
          if (tier >= 2) cterm.audio_flush(SND_CH);
          return;
        }
        playSample(number, effect, volume, repeats);
      } catch (e) {
        log(LOG_WARNING, "zsound: " + e);
      }
    },
    stop: function () {                                    // door cleanup (game exit)
      try {
        if (tier >= 1) { cterm.audio_flush(SND_CH); cterm.audio_flush(BLEEP_CH); }
      } catch (e) { }
    }
  };
}
