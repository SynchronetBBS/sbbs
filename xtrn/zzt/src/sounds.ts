namespace ZZT {
  var SOUND_TICK_MS: number = 55;
  var BRIDGE_PREFIX: string = "FLWEB/1 ";
  var BRIDGE_RESPONSE_PREFIX: string = "FLWEB/1R ";
  var ZZT_SOUND_ID: string = "zzt-main";
  var ZZT_MUSIC_ID: string = "zzt-bgm";
  var SHARED_MUSIC_REL_ROOT: string = "zzt_worlds";

  export var SoundEnabled: boolean = true;
  export var SoundBlockQueueing: boolean = false;
  export var SoundCurrentPriority: number = -1;
  export var SoundDurationMultiplier: number = 1;
  export var SoundDurationCounter: number = 1;
  export var SoundBuffer: string = "";
  export var SoundBufferPos: number = 0;
  export var SoundIsPlaying: boolean = false;
  export var AnsiMusicMode: string = "AUTO";
  export var AnsiMusicIntroducer: string = "|";
  export var AnsiMusicForeground: boolean = false;

  var SoundFreqTable: number[] = [];
  var SoundTickLastMs: number = 0;
  var SoundBridgeState: number = 0; // 0 unknown, 1 available, -1 unavailable
  var SoundOutputActive: boolean = false;
  var AnsiMusicActive: boolean = false;
  var WorldMusicTracks: WorldMusicTrack[] = [];
  var WorldMusicCurrentAssetPath: string = "";

  interface WorldMusicTrack {
    AssetPath: string;
    TitleKey: string;
    Tokens: string[];
  }

  function nowMs(): number {
    return new Date().getTime();
  }

  function toByteCode(ch: string): number {
    if (ch.length <= 0) {
      return 0;
    }
    return ch.charCodeAt(0) & 0xff;
  }

  function buildAsciiSafeJson(value: unknown): string {
    return JSON.stringify(value).replace(/[^\x20-\x7e]/g, function (ch: string): string {
      var code: string = ch.charCodeAt(0).toString(16).toUpperCase();
      while (code.length < 4) {
        code = "0" + code;
      }
      return "\\u" + code;
    });
  }

  function writeBridgeRaw(text: string): boolean {
    if (typeof console === "undefined") {
      return false;
    }

    try {
      if (typeof console.write === "function") {
        console.write(text);
        return true;
      }
      if (typeof console.print === "function") {
        console.print(text);
        return true;
      }
    } catch (_err) {
      return false;
    }

    return false;
  }

  function soundTrimSpaces(value: string): string {
    return String(value || "").replace(/^\s+/, "").replace(/\s+$/, "");
  }

  function normalizeAnsiMusicModeValue(mode: string): string {
    var upper: string = soundTrimSpaces(mode).toUpperCase();
    if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1") {
      return "ON";
    }
    if (upper === "AUTO" || upper === "CTERM" || upper === "SYNCTERM") {
      return "AUTO";
    }
    return "OFF";
  }

  function normalizeAnsiMusicIntroducerValue(introducer: string): string {
    var upper: string = soundTrimSpaces(introducer).toUpperCase();
    if (upper === "N" || upper === "CSI_N" || upper === "BANANSI" || upper === "BANSI") {
      return "N";
    }
    if (upper === "M" || upper === "CSI_M" || upper === "DL") {
      return "M";
    }
    if (upper === "|" || upper === "PIPE" || upper === "BAR" || upper === "CSI_PIPE") {
      return "|";
    }
    return "|";
  }

  function normalizeAnsiMusicForegroundValue(value: string): boolean {
    var upper: string = soundTrimSpaces(value).toUpperCase();
    if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1" ||
        upper === "FOREGROUND" || upper === "FG" || upper === "SYNC") {
      return true;
    }
    return false;
  }

  function detectAnsiMusicSupportForTerminal(): boolean {
    var ctermVersion: number;
    if (typeof console === "undefined" ||
        (typeof console.write !== "function" && typeof console.print !== "function")) {
      return false;
    }

    if (AnsiMusicMode === "ON") {
      return true;
    }
    if (AnsiMusicMode !== "AUTO") {
      return false;
    }

    ctermVersion = (typeof console.cterm_version === "number" ? console.cterm_version : -1);
    return ctermVersion >= 0;
  }

  function ansiMusicLengthFromDuration(durationCode: number): number {
    var denom: number;
    if (durationCode <= 0) {
      return 32;
    }
    denom = Math.floor((32 + Math.floor(durationCode / 2)) / durationCode);
    if (denom < 1) {
      denom = 1;
    }
    if (denom > 64) {
      denom = 64;
    }
    return denom;
  }

  function ansiMusicToneToken(tone: number): string {
    switch (tone) {
      case 0:
        return "C";
      case 1:
        return "C#";
      case 2:
        return "D";
      case 3:
        return "D#";
      case 4:
        return "E";
      case 5:
        return "F";
      case 6:
        return "F#";
      case 7:
        return "G";
      case 8:
        return "G#";
      case 9:
        return "A";
      case 10:
        return "A#";
      case 11:
        return "B";
      default:
        return "";
    }
  }

  function ansiMusicDrumToNoteCode(drumId: number): number {
    var noteCodes: number[] = [0x20, 0x23, 0x27, 0x30, 0x34, 0x37, 0x40, 0x44, 0x47, 0x50];
    var index: number = drumId % noteCodes.length;
    if (index < 0) {
      index += noteCodes.length;
    }
    return noteCodes[index];
  }

  function buildAnsiMusicFromPattern(pattern: string): string {
    var tokens: string[] = [];
    var i: number = 0;
    var noteCode: number;
    var durationCode: number;
    var noteLength: number = 32;
    var currentLength: number = 32;
    var octave: number = 3;
    var currentOctave: number = 3;
    var tone: number;
    var token: string;

    if (pattern.length <= 1) {
      return "";
    }

    tokens.push("T120");
    tokens.push("O3");
    tokens.push("L32");

    while ((i + 1) < pattern.length) {
      noteCode = toByteCode(pattern.charAt(i));
      durationCode = toByteCode(pattern.charAt(i + 1));
      i += 2;

      noteLength = ansiMusicLengthFromDuration(durationCode);
      if (noteLength !== currentLength) {
        tokens.push("L" + String(noteLength));
        currentLength = noteLength;
      }

      if (noteCode === 0) {
        tokens.push("P");
        continue;
      }

      if (noteCode >= 240) {
        noteCode = ansiMusicDrumToNoteCode(noteCode - 240);
      }

      tone = noteCode % 16;
      if (tone < 0 || tone > 11) {
        tokens.push("P");
        continue;
      }

      octave = Math.floor(noteCode / 16);
      if (octave < 0) {
        octave = 0;
      }
      if (octave > 6) {
        octave = 6;
      }

      if (octave !== currentOctave) {
        tokens.push("O" + String(octave));
        currentOctave = octave;
      }

      token = ansiMusicToneToken(tone);
      if (token.length <= 0) {
        tokens.push("P");
      } else {
        tokens.push(token);
      }
    }

    if (tokens.length <= 3) {
      return "";
    }
    return tokens.join("");
  }

  function emitAnsiMusicFromPattern(pattern: string): void {
    var prefix: string;
    var intro: string;
    var music: string;
    if (!AnsiMusicActive || !SoundEnabled) {
      return;
    }

    music = buildAnsiMusicFromPattern(pattern);
    if (music.length <= 0) {
      return;
    }

    intro = normalizeAnsiMusicIntroducerValue(AnsiMusicIntroducer);
    prefix = "";
    if (intro === "|" || intro === "M") {
      prefix = (AnsiMusicForeground ? "F" : "B");
    }
    writeBridgeRaw("\x1b[" + intro + prefix + music + "\x0E");
  }

  function emitBridgePacket(action: string, payload: { [name: string]: unknown }): boolean {
    var packet: { [name: string]: unknown } = {};
    var key: string;

    if (typeof console === "undefined") {
      return false;
    }

    packet.action = action;
    for (key in payload) {
      if (Object.prototype.hasOwnProperty.call(payload, key)) {
        packet[key] = payload[key];
      }
    }

    return writeBridgeRaw("\x1b_" + BRIDGE_PREFIX + buildAsciiSafeJson(packet) + "\x1b\\");
  }

  function soundBridgeProbe(timeoutMs: number): boolean {
    var nonce: string;
    var deadline: number;
    var buffer: string;
    var ch: string;
    var start: number;
    var end: number;
    var payload: string;
    var data: { action?: string; nonce?: string } | null;
    var flushDeadline: number;

    if (typeof console === "undefined" ||
        (typeof console.write !== "function" && typeof console.print !== "function") ||
        typeof console.inkey !== "function") {
      return false;
    }

    nonce = String(nowMs()).slice(-8) + String(Math.floor(Math.random() * 10000));

    while (console.inkey(0, 0) !== "") {
      // Flush stale input before probing.
    }

    if (!emitBridgePacket("bridge.probe", { nonce: nonce })) {
      return false;
    }

    buffer = "";
    deadline = nowMs() + timeoutMs;
    while (nowMs() < deadline) {
      ch = console.inkey(0, 50);
      if (ch.length <= 0) {
        continue;
      }
      buffer += ch;

      start = buffer.indexOf("\x1b_");
      if (start < 0) {
        if (buffer.length > 512) {
          buffer = buffer.slice(buffer.length - 256);
        }
        continue;
      }

      end = buffer.indexOf("\x1b\\", start + 2);
      if (end < 0) {
        continue;
      }

      payload = buffer.slice(start + 2, end);
      buffer = buffer.slice(end + 2);

      if (payload.indexOf(BRIDGE_RESPONSE_PREFIX) !== 0) {
        continue;
      }

      data = null;
      try {
        data = JSON.parse(payload.slice(BRIDGE_RESPONSE_PREFIX.length)) as { action?: string; nonce?: string };
      } catch (_parseErr) {
        data = null;
      }

      if (data !== null &&
          data.action === "bridge.ack" &&
          data.nonce === nonce) {
        flushDeadline = nowMs() + 100;
        while (nowMs() < flushDeadline) {
          if (console.inkey(0, 10) === "") {
            break;
          }
        }
        return true;
      }
    }

    return false;
  }

  function ensureBridgeDetected(): boolean {
    if (SoundBridgeState > 0) {
      return true;
    }
    if (SoundBridgeState < 0) {
      return false;
    }

    SoundBridgeState = soundBridgeProbe(450) ? 1 : -1;
    return SoundBridgeState > 0;
  }

  function emitSoundStop(): void {
    if (!SoundOutputActive) {
      return;
    }
    if (ensureBridgeDetected()) {
      emitBridgePacket("audio.zzt.stop", {
        id: ZZT_SOUND_ID
      });
    }
    SoundOutputActive = false;
  }

  function emitSoundTone(freqHz: number, durationTicks: number): void {
    var durationMs: number;
    if (freqHz <= 0) {
      emitSoundStop();
      return;
    }
    if (!ensureBridgeDetected()) {
      return;
    }

    durationMs = Math.floor(durationTicks * SOUND_TICK_MS);
    if (durationMs < 20) {
      durationMs = 20;
    }

    emitBridgePacket("audio.zzt.note", {
      id: ZZT_SOUND_ID,
      frequencyHz: freqHz,
      durationMs: durationMs,
      volume: 0.18,
      waveform: "square"
    });
    SoundOutputActive = true;
  }

  function emitSoundDrum(drumId: number, durationTicks: number): void {
    var durationMs: number;
    if (!ensureBridgeDetected()) {
      return;
    }

    durationMs = Math.floor(durationTicks * SOUND_TICK_MS);
    if (durationMs < 20) {
      durationMs = 20;
    }

    emitBridgePacket("audio.zzt.drum", {
      id: ZZT_SOUND_ID,
      drumId: drumId,
      durationMs: durationMs,
      volume: 0.25
    });
    SoundOutputActive = true;
  }

  function normalizePath(path: string): string {
    return String(path || "").split("\\").join("/");
  }

  function stripFileExtension(path: string): string {
    var dot: number = path.lastIndexOf(".");
    if (dot <= 0) {
      return path;
    }
    return path.slice(0, dot);
  }

  function pathDirname(path: string): string {
    var normalized: string = normalizePath(path);
    var slashPos: number = normalized.lastIndexOf("/");
    if (slashPos < 0) {
      return "";
    }
    return normalized.slice(0, slashPos + 1);
  }

  function pathBasename(path: string): string {
    var normalized: string = normalizePath(path);
    var slashPos: number = normalized.lastIndexOf("/");
    if (slashPos < 0) {
      return normalized;
    }
    return normalized.slice(slashPos + 1);
  }

  function splitTokens(text: string): string[] {
    var cleaned: string = String(text || "").toLowerCase().replace(/[^a-z0-9]+/g, " ");
    var parts: string[] = cleaned.split(/\s+/);
    var out: string[] = [];
    var i: number;
    for (i = 0; i < parts.length; i += 1) {
      if (parts[i].length > 0) {
        out.push(parts[i]);
      }
    }
    return out;
  }

  function sanitizePathSegment(value: string): string {
    var out: string = String(value || "").toLowerCase().replace(/[^a-z0-9._-]+/g, "-");
    out = out.replace(/^-+/, "").replace(/-+$/, "");
    if (out.length <= 0) {
      return "world";
    }
    return out;
  }

  function fileExists(path: string): boolean {
    var f: SyncFile;
    if (typeof File === "undefined") {
      return false;
    }
    try {
      f = new File(path);
      return !!f.exists;
    } catch (_err) {
      return false;
    }
  }

  function pathJoin(base: string, rel: string): string {
    var b: string = String(base || "");
    if (b.length > 0) {
      var tail: string = b.charAt(b.length - 1);
      if (tail !== "/" && tail !== "\\") {
        b += "/";
      }
    }
    return b + rel;
  }

  function ensureDirectory(path: string): boolean {
    if (typeof file_isdir === "function" && file_isdir(path)) {
      return true;
    }
    if (typeof mkpath !== "function") {
      return false;
    }
    try {
      if (!!mkpath(path)) {
        return true;
      }
      if (typeof file_isdir === "function" && file_isdir(path)) {
        return true;
      }
      return false;
    } catch (_err) {
      return false;
    }
  }

  function copyBinaryFile(sourcePath: string, destinationPath: string): boolean {
    var source: SyncFile;
    var destination: SyncFile;
    var chunk: string | null;
    if (typeof File === "undefined") {
      return false;
    }

    try {
      source = new File(sourcePath);
      destination = new File(destinationPath);
    } catch (_err) {
      return false;
    }

    if (!source.open("rb")) {
      return false;
    }
    if (!destination.open("wb")) {
      source.close();
      return false;
    }

    try {
      while (true) {
        chunk = source.read(8192);
        if (chunk === null || typeof chunk === "undefined" || chunk.length <= 0) {
          break;
        }
        destination.write(chunk);
      }
    } finally {
      source.close();
      destination.close();
    }

    return true;
  }

  function stageWorldMusicFile(sourcePath: string, worldBucket: string): string {
    var modsRoot: string;
    var relativeAssetPath: string;
    var destinationDir: string;
    var destinationPath: string;
    var sourceSize: number = -1;
    var destinationSize: number = -2;
    var copied: boolean;
    var fileName: string = pathBasename(sourcePath);

    if (typeof system === "undefined" ||
        typeof system.mods_dir !== "string" ||
        system.mods_dir.length <= 0) {
      return "";
    }
    if (fileName.length <= 0) {
      return "";
    }

    modsRoot = system.mods_dir;
    relativeAssetPath = SHARED_MUSIC_REL_ROOT + "/" + worldBucket + "/" + fileName;
    destinationDir = pathJoin(modsRoot, "flweb/assets/" + SHARED_MUSIC_REL_ROOT + "/" + worldBucket + "/");
    destinationPath = pathJoin(destinationDir, fileName);

    if (!ensureDirectory(destinationDir)) {
      return "";
    }

    if (typeof file_size === "function") {
      try {
        sourceSize = file_size(sourcePath);
        destinationSize = file_size(destinationPath);
      } catch (_err) {
        sourceSize = -1;
        destinationSize = -2;
      }
    }

    if (fileExists(destinationPath) && sourceSize >= 0 && sourceSize === destinationSize) {
      return relativeAssetPath;
    }

    copied = false;
    if (typeof file_copy === "function") {
      try {
        copied = !!file_copy(sourcePath, destinationPath);
      } catch (_err) {
        copied = false;
      }
    }

    if (!copied) {
      copied = copyBinaryFile(sourcePath, destinationPath);
    }

    if (!copied) {
      return "";
    }
    return relativeAssetPath;
  }

  function listWorldPackMp3Files(worldFilePath: string): string[] {
    var worldDir: string = pathDirname(worldFilePath);
    var listed: string[] = [];
    var seen: { [name: string]: boolean } = {};
    var files: string[] = [];
    var i: number;
    var normalized: string;
    var upper: string;

    if (worldDir.length <= 0 || typeof directory !== "function") {
      return files;
    }

    listed = directory(pathJoin(worldDir, "*"));
    for (i = 0; i < listed.length; i += 1) {
      normalized = normalizePath(listed[i]);
      if (typeof file_isdir === "function" && file_isdir(normalized)) {
        continue;
      }
      upper = normalized.toUpperCase();
      if (upper.length < 4 || upper.slice(upper.length - 4) !== ".MP3") {
        continue;
      }
      if (seen[upper] === true) {
        continue;
      }
      seen[upper] = true;
      files.push(normalized);
    }

    files.sort(function (a: string, b: string): number {
      var aUpper: string = a.toUpperCase();
      var bUpper: string = b.toUpperCase();
      if (aUpper < bUpper) {
        return -1;
      }
      if (aUpper > bUpper) {
        return 1;
      }
      return 0;
    });
    return files;
  }

  function scoreTrackForBoard(track: WorldMusicTrack, boardName: string): number {
    var boardKey: string = stripFileExtension(pathBasename(boardName)).toLowerCase();
    var boardTokens: string[] = splitTokens(boardKey);
    var trackTokens: string[] = track.Tokens;
    var score: number = 0;
    var i: number;
    var j: number;

    if (boardKey.length > 0) {
      if (boardKey === track.TitleKey) {
        score += 25;
      } else if (boardKey.indexOf(track.TitleKey) >= 0 || track.TitleKey.indexOf(boardKey) >= 0) {
        score += 12;
      }
    }

    for (i = 0; i < boardTokens.length; i += 1) {
      if (boardTokens[i].length <= 1) {
        continue;
      }
      for (j = 0; j < trackTokens.length; j += 1) {
        if (boardTokens[i] === trackTokens[j]) {
          score += 3;
        }
      }
    }

    return score;
  }

  function chooseWorldMusicTrack(boardName: string): WorldMusicTrack | null {
    var i: number;
    var score: number;
    var bestScore: number = -1;
    var best: WorldMusicTrack | null = null;

    if (WorldMusicTracks.length <= 0) {
      return null;
    }
    if (boardName.length <= 0) {
      return WorldMusicTracks[0];
    }

    for (i = 0; i < WorldMusicTracks.length; i += 1) {
      score = scoreTrackForBoard(WorldMusicTracks[i], boardName);
      if (score > bestScore) {
        bestScore = score;
        best = WorldMusicTracks[i];
      }
    }

    if (best === null) {
      return WorldMusicTracks[0];
    }
    if (bestScore <= 0) {
      return WorldMusicTracks[0];
    }
    return best;
  }

  export function SoundWorldMusicConfigureFromWorldFile(worldFilePath: string): void {
    var sources: string[] = [];
    var i: number;
    var worldBase: string;
    var worldParent: string;
    var worldBucket: string;
    var assetPath: string;
    var trackTitle: string;

    SoundWorldMusicStop();
    WorldMusicTracks = [];

    if (worldFilePath.length <= 0) {
      return;
    }

    sources = listWorldPackMp3Files(worldFilePath);
    if (sources.length <= 0) {
      return;
    }

    worldBase = sanitizePathSegment(stripFileExtension(pathBasename(worldFilePath)));
    worldParent = sanitizePathSegment(pathBasename(pathDirname(worldFilePath)));
    if (worldParent.length > 0 && worldParent !== "world") {
      worldBucket = worldParent + "-" + worldBase;
    } else {
      worldBucket = worldBase;
    }

    for (i = 0; i < sources.length; i += 1) {
      assetPath = stageWorldMusicFile(sources[i], worldBucket);
      if (assetPath.length <= 0) {
        continue;
      }
      trackTitle = stripFileExtension(pathBasename(sources[i])).toLowerCase();
      WorldMusicTracks.push({
        AssetPath: assetPath,
        TitleKey: trackTitle,
        Tokens: splitTokens(trackTitle)
      });
    }
  }

  export function SoundWorldMusicStop(): void {
    if (SoundBridgeState > 0) {
      emitBridgePacket("audio.stop", {
        id: ZZT_MUSIC_ID
      });
    }
    WorldMusicCurrentAssetPath = "";
  }

  export function SoundWorldMusicOnBoardChanged(boardName: string): void {
    var selected: WorldMusicTrack | null;

    if (!SoundEnabled) {
      return;
    }

    selected = chooseWorldMusicTrack(boardName);
    if (selected === null) {
      return;
    }
    if (selected.AssetPath === WorldMusicCurrentAssetPath) {
      return;
    }
    if (!ensureBridgeDetected()) {
      return;
    }

    if (emitBridgePacket("audio.play", {
      id: ZZT_MUSIC_ID,
      asset: {
        scope: "shared",
        path: selected.AssetPath
      },
      volume: 0.55,
      loop: true
    })) {
      WorldMusicCurrentAssetPath = selected.AssetPath;
    }
  }

  export function SoundWorldMusicHasTracks(): boolean {
    return WorldMusicTracks.length > 0;
  }

  function initFreqTable(): void {
    var octave: number;
    var note: number;
    var freqC1: number = 32.0;
    var noteStep: number = Math.exp(Math.log(2.0) / 12.0);
    var noteBase: number;
    var i: number;

    SoundFreqTable = [];
    for (i = 0; i <= 255; i += 1) {
      SoundFreqTable.push(0);
    }

    for (octave = 1; octave <= 15; octave += 1) {
      noteBase = Math.exp(octave * Math.log(2.0)) * freqC1;
      for (note = 0; note <= 11; note += 1) {
        SoundFreqTable[(octave * 16) + note] = Math.floor(noteBase);
        noteBase *= noteStep;
      }
    }
  }

  function soundTimerTick(): void {
    var noteCode: number;
    var durationCode: number;
    var duration: number;
    var freqHz: number;

    if (!SoundEnabled) {
      if (SoundIsPlaying) {
        SoundIsPlaying = false;
      }
      emitSoundStop();
      return;
    }

    if (!SoundIsPlaying) {
      return;
    }

    SoundDurationCounter -= 1;
    if (SoundDurationCounter > 0) {
      return;
    }

    emitSoundStop();
    if (SoundBufferPos >= SoundBuffer.length) {
      SoundIsPlaying = false;
      return;
    }

    noteCode = toByteCode(SoundBuffer.charAt(SoundBufferPos));
    SoundBufferPos += 1;

    if (SoundBufferPos >= SoundBuffer.length) {
      SoundIsPlaying = false;
      return;
    }

    durationCode = toByteCode(SoundBuffer.charAt(SoundBufferPos));
    SoundBufferPos += 1;

    duration = SoundDurationMultiplier * durationCode;
    if (duration <= 0) {
      duration = 1;
    }
    SoundDurationCounter = duration;

    if (noteCode === 0) {
      return;
    }

    if (noteCode < 240) {
      freqHz = SoundFreqTable[noteCode];
      if (freqHz > 0) {
        emitSoundTone(freqHz, duration);
      }
      return;
    }

    emitSoundDrum(noteCode - 240, duration);
  }

  export function SoundInit(): void {
    initFreqTable();
    SoundEnabled = true;
    SoundBlockQueueing = false;
    SoundCurrentPriority = -1;
    SoundDurationMultiplier = 1;
    SoundDurationCounter = 1;
    SoundBuffer = "";
    SoundBufferPos = 0;
    SoundIsPlaying = false;
    SoundOutputActive = false;
    SoundTickLastMs = nowMs();
    SoundBridgeState = 0;
    AnsiMusicMode = normalizeAnsiMusicModeValue(AnsiMusicMode);
    AnsiMusicIntroducer = normalizeAnsiMusicIntroducerValue(AnsiMusicIntroducer);
    AnsiMusicForeground = normalizeAnsiMusicForegroundValue(AnsiMusicForeground ? "ON" : "OFF");
    AnsiMusicActive = detectAnsiMusicSupportForTerminal();
    WorldMusicTracks = [];
    WorldMusicCurrentAssetPath = "";
    // Probe once up front to avoid probing during gameplay input handling.
    ensureBridgeDetected();
  }

  export function SoundUpdate(): void {
    var now: number = nowMs();
    var elapsed: number;
    var ticks: number;
    var i: number;

    if (SoundTickLastMs <= 0) {
      SoundTickLastMs = now;
      return;
    }

    if (now < SoundTickLastMs) {
      SoundTickLastMs = now;
      return;
    }

    elapsed = now - SoundTickLastMs;
    if (elapsed < SOUND_TICK_MS) {
      return;
    }

    ticks = Math.floor(elapsed / SOUND_TICK_MS);
    if (ticks > 12) {
      ticks = 12;
    }
    SoundTickLastMs += ticks * SOUND_TICK_MS;

    for (i = 0; i < ticks; i += 1) {
      soundTimerTick();
    }
  }

  export function SoundUninstall(): void {
    SoundWorldMusicStop();
    emitSoundStop();
    SoundBridgeState = -1;
  }

  export function SoundClearQueue(): void {
    SoundBuffer = "";
    SoundBufferPos = 0;
    SoundIsPlaying = false;
    SoundCurrentPriority = -1;
    SoundDurationCounter = 1;
    emitSoundStop();
    SoundWorldMusicStop();
  }

  export function SoundQueue(priority: number, pattern: string): void {
    if (SoundBlockQueueing) {
      return;
    }

    if (!SoundIsPlaying ||
        (((priority >= SoundCurrentPriority) && (SoundCurrentPriority !== -1)) || (priority === -1))) {
      if (priority >= 0 || !SoundIsPlaying) {
        SoundCurrentPriority = priority;
        SoundBuffer = pattern;
        SoundBufferPos = 0;
        SoundDurationCounter = 1;
      } else {
        SoundBuffer = SoundBuffer.slice(SoundBufferPos);
        SoundBufferPos = 0;
        if ((SoundBuffer.length + pattern.length) < 255) {
          SoundBuffer += pattern;
        }
      }
      SoundIsPlaying = true;
      emitAnsiMusicFromPattern(pattern);
    }
  }

  export function SoundParse(input: string): string {
    var noteOctave: number = 3;
    var noteDuration: number = 1;
    var output: string = "";
    var i: number = 0;
    var tone: number;
    var ch: string;

    function appendNote(noteTone: number): void {
      output += String.fromCharCode(((noteOctave * 16) + noteTone) & 0xff);
      output += String.fromCharCode(noteDuration & 0xff);
    }

    while (i < input.length) {
      ch = input.charAt(i).toUpperCase();
      tone = -1;

      if (ch === "T") {
        noteDuration = 1;
        i += 1;
      } else if (ch === "S") {
        noteDuration = 2;
        i += 1;
      } else if (ch === "I") {
        noteDuration = 4;
        i += 1;
      } else if (ch === "Q") {
        noteDuration = 8;
        i += 1;
      } else if (ch === "H") {
        noteDuration = 16;
        i += 1;
      } else if (ch === "W") {
        noteDuration = 32;
        i += 1;
      } else if (ch === ".") {
        noteDuration = Math.floor((noteDuration * 3) / 2);
        i += 1;
      } else if (ch === "3") {
        noteDuration = Math.floor(noteDuration / 3);
        i += 1;
      } else if (ch === "+") {
        if (noteOctave < 6) {
          noteOctave += 1;
        }
        i += 1;
      } else if (ch === "-") {
        if (noteOctave > 1) {
          noteOctave -= 1;
        }
        i += 1;
      } else if (ch >= "A" && ch <= "G") {
        if (ch === "C") {
          tone = 0;
        } else if (ch === "D") {
          tone = 2;
        } else if (ch === "E") {
          tone = 4;
        } else if (ch === "F") {
          tone = 5;
        } else if (ch === "G") {
          tone = 7;
        } else if (ch === "A") {
          tone = 9;
        } else {
          tone = 11;
        }
        i += 1;

        if (i < input.length) {
          ch = input.charAt(i).toUpperCase();
          if (ch === "!") {
            tone -= 1;
            i += 1;
          } else if (ch === "#") {
            tone += 1;
            i += 1;
          }
        }

        appendNote(tone);
      } else if (ch === "X") {
        output += String.fromCharCode(0);
        output += String.fromCharCode(noteDuration & 0xff);
        i += 1;
      } else if (ch >= "0" && ch <= "9") {
        output += String.fromCharCode((ch.charCodeAt(0) + 0xf0 - "0".charCodeAt(0)) & 0xff);
        output += String.fromCharCode(noteDuration & 0xff);
        i += 1;
      } else {
        i += 1;
      }
    }

    return output;
  }
}
